/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/mfd/pm8xxx/pm8921-bms.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/mfd/pm8xxx/ccadc.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#define BMS_CONTROL		0x224
#define BMS_S1_DELAY		0x225
#define BMS_OUTPUT0		0x230
#define BMS_OUTPUT1		0x231
#define BMS_TOLERANCES		0x232
#define BMS_TEST1		0x237

#define ADC_ARB_SECP_CNTRL	0x190
#define ADC_ARB_SECP_AMUX_CNTRL	0x191
#define ADC_ARB_SECP_ANA_PARAM	0x192
#define ADC_ARB_SECP_DIG_PARAM	0x193
#define ADC_ARB_SECP_RSV	0x194
#define ADC_ARB_SECP_DATA1	0x195
#define ADC_ARB_SECP_DATA0	0x196

#define ADC_ARB_BMS_CNTRL	0x18D
#define AMUX_TRIM_2		0x322
#define TEST_PROGRAM_REV	0x339

#define TEMP_SOC_STORAGE	0x107

enum pmic_bms_interrupts {
	PM8921_BMS_SBI_WRITE_OK,
	PM8921_BMS_CC_THR,
	PM8921_BMS_VSENSE_THR,
	PM8921_BMS_VSENSE_FOR_R,
	PM8921_BMS_OCV_FOR_R,
	PM8921_BMS_GOOD_OCV,
	PM8921_BMS_VSENSE_AVG,
	PM_BMS_MAX_INTS,
};

struct pm8921_soc_params {
	uint16_t	last_good_ocv_raw;
	int		cc;

	int		last_good_ocv_uv;
};

struct pm8921_rbatt_params {
	uint16_t	ocv_for_rbatt_raw;
	uint16_t	vsense_for_rbatt_raw;
	uint16_t	vbatt_for_rbatt_raw;

	int		ocv_for_rbatt_uv;
	int		vsense_for_rbatt_uv;
	int		vbatt_for_rbatt_uv;
};

/**
 * struct pm8921_bms_chip -
 * @bms_output_lock:	lock to prevent concurrent bms reads
 * @bms_100_lock:	lock to prevent concurrent updates to values that force
 *			100% charge
 *
 */
struct pm8921_bms_chip {
	struct device		*dev;
	struct dentry		*dent;
	unsigned int		r_sense;
	unsigned int		i_test;
	unsigned int		v_failure;
	unsigned int		fcc;
	struct single_row_lut	*fcc_temp_lut;
	struct single_row_lut	*fcc_sf_lut;
	struct pc_temp_ocv_lut	*pc_temp_ocv_lut;
	struct pc_sf_lut	*pc_sf_lut;
	struct work_struct	calib_hkadc_work;
	struct delayed_work	calib_ccadc_work;
	unsigned int		calib_delay_ms;
	unsigned int		revision;
	unsigned int		xoadc_v0625;
	unsigned int		xoadc_v125;
	unsigned int		batt_temp_channel;
	unsigned int		vbat_channel;
	unsigned int		ref625mv_channel;
	unsigned int		ref1p25v_channel;
	unsigned int		batt_id_channel;
	unsigned int		pmic_bms_irq[PM_BMS_MAX_INTS];
	DECLARE_BITMAP(enabled_irqs, PM_BMS_MAX_INTS);
	struct mutex		bms_output_lock;
	spinlock_t		bms_100_lock;
	struct single_row_lut	*adjusted_fcc_temp_lut;
	unsigned int		charging_began;
	unsigned int		start_percent;
	unsigned int		end_percent;

	uint16_t		ocv_reading_at_100;
	int			cc_reading_at_100;
	int			max_voltage_uv;

	int			batt_temp_suspend;
	int			amux_2_trim_delta;
	uint16_t		prev_last_good_ocv_raw;
	int			pon_ocv_uv;
	int			shutdown_soc;
	int			shutdown_soc_timer_expired;
	struct delayed_work	shutdown_soc_work;
#ifdef CONFIG_PM8921_TEST_OVERRIDE
	int			user_override;
	int			user_override_is_chg;
#endif
#ifdef CONFIG_PM8921_EXTENDED_INFO
	unsigned int		meter_offset;
	unsigned int		init_rbatt;
	unsigned int		k_factor;
#endif
};

/*
 * protects against simultaneous adjustment of ocv based on shutdown soc and
 * invalidating the shutdown soc
 */
static DEFINE_MUTEX(soc_invalidation_mutex);
static int shutdown_soc_invalid;
static struct pm8921_bms_chip *the_chip;

#define DEFAULT_RBATT_MOHMS		128
#define DEFAULT_OCV_MICROVOLTS		3900000
#define DEFAULT_CHARGE_CYCLES		0

static int last_chargecycles = DEFAULT_CHARGE_CYCLES;
static int last_charge_increase;
module_param(last_chargecycles, int, 0644);
module_param(last_charge_increase, int, 0644);

static int last_rbatt = -EINVAL;
static int last_ocv_uv = -EINVAL;
static int last_soc = -EINVAL;
static int last_real_fcc_mah = -EINVAL;
static int last_real_fcc_batt_temp = -EINVAL;

static int bms_ops_set(const char *val, const struct kernel_param *kp)
{
	if (*(int *)kp->arg == -EINVAL)
		return param_set_int(val, kp);
	else
		return 0;
}

static struct kernel_param_ops bms_param_ops = {
	.set = bms_ops_set,
	.get = param_get_int,
};

module_param_cb(last_rbatt, &bms_param_ops, &last_rbatt, 0644);
module_param_cb(last_ocv_uv, &bms_param_ops, &last_ocv_uv, 0644);
module_param_cb(last_soc, &bms_param_ops, &last_soc, 0644);

/*
 * bms_fake_battery is set in setups where a battery emulator is used instead
 * of a real battery. This makes the bms driver report a different/fake value
 * regardless of the calculated state of charge.
 */
static int bms_fake_battery = -EINVAL;
module_param(bms_fake_battery, int, 0644);

/* bms_start_XXX and bms_end_XXX are read only */
static int bms_start_percent;
static int bms_start_ocv_uv;
static int bms_start_cc_uah;
static int bms_end_percent;
static int bms_end_ocv_uv;
static int bms_end_cc_uah;
#ifdef CONFIG_PM8921_EXTENDED_INFO
static int bms_meter_offset;
#endif

static int bms_ro_ops_set(const char *val, const struct kernel_param *kp)
{
	return -EINVAL;
}

static struct kernel_param_ops bms_ro_param_ops = {
	.set = bms_ro_ops_set,
	.get = param_get_int,
};
module_param_cb(bms_start_percent, &bms_ro_param_ops, &bms_start_percent, 0644);
module_param_cb(bms_start_ocv_uv, &bms_ro_param_ops, &bms_start_ocv_uv, 0644);
module_param_cb(bms_start_cc_uah, &bms_ro_param_ops, &bms_start_cc_uah, 0644);

module_param_cb(bms_end_percent, &bms_ro_param_ops, &bms_end_percent, 0644);
module_param_cb(bms_end_ocv_uv, &bms_ro_param_ops, &bms_end_ocv_uv, 0644);
module_param_cb(bms_end_cc_uah, &bms_ro_param_ops, &bms_end_cc_uah, 0644);
#ifdef CONFIG_PM8921_EXTENDED_INFO
module_param_cb(bms_meter_offset, &bms_ro_param_ops, &bms_meter_offset, 0644);
#endif

static int bms_aged_capacity = 0;
module_param(bms_aged_capacity, int, 0644);

static int timestamp;

module_param(timestamp, int, 0644);
MODULE_PARM_DESC(timestamp, "Epoch format timestamp value which indicates last"
	"time when cycle_count var was updated");

static int interpolate_fcc(struct pm8921_bms_chip *chip, int batt_temp);

static void readjust_fcc_table(void)
{
	struct single_row_lut *temp, *old;
	int i, fcc, ratio;

	if (!the_chip->fcc_temp_lut) {
		pr_err("The static fcc lut table is NULL\n");
		return;
	}

#ifdef CONFIG_PM8921_FLOAT_CHARGE
	if (!the_chip->adjusted_fcc_temp_lut)
		return;
#endif

	temp = kzalloc(sizeof(struct single_row_lut), GFP_KERNEL);
	if (!temp) {
		pr_err("Cannot allocate memory for adjusted fcc table\n");
		return;
	}

	fcc = interpolate_fcc(the_chip, last_real_fcc_batt_temp);

	temp->cols = the_chip->fcc_temp_lut->cols;
	for (i = 0; i < the_chip->fcc_temp_lut->cols; i++) {
		temp->x[i] = the_chip->fcc_temp_lut->x[i];
		ratio = div_u64(the_chip->fcc_temp_lut->y[i] * 1000, fcc);
		temp->y[i] =  (ratio * last_real_fcc_mah);
		temp->y[i] /= 1000;
		pr_debug("temp=%d, staticfcc=%d, adjfcc=%d, ratio=%d\n",
				temp->x[i], the_chip->fcc_temp_lut->y[i],
				temp->y[i], ratio);
	}

	old = the_chip->adjusted_fcc_temp_lut;
	the_chip->adjusted_fcc_temp_lut = temp;
	kfree(old);
}

static int bms_last_real_fcc_set(const char *val,
				const struct kernel_param *kp)
{
	int rc = 0;

	if (last_real_fcc_mah == -EINVAL)
		rc = param_set_int(val, kp);
	if (rc) {
		pr_err("Failed to set last_real_fcc_mah rc=%d\n", rc);
		return rc;
	}
	if (last_real_fcc_batt_temp != -EINVAL)
		readjust_fcc_table();
	return rc;
}
static struct kernel_param_ops bms_last_real_fcc_param_ops = {
	.set = bms_last_real_fcc_set,
	.get = param_get_int,
};
module_param_cb(last_real_fcc_mah, &bms_last_real_fcc_param_ops,
					&last_real_fcc_mah, 0644);

static int bms_last_real_fcc_batt_temp_set(const char *val,
				const struct kernel_param *kp)
{
	int rc = 0;

	if (last_real_fcc_batt_temp == -EINVAL)
		rc = param_set_int(val, kp);
	if (rc) {
		pr_err("Failed to set last_real_fcc_batt_temp rc=%d\n", rc);
		return rc;
	}
	if (last_real_fcc_mah != -EINVAL)
		readjust_fcc_table();
	return rc;
}

static struct kernel_param_ops bms_last_real_fcc_batt_temp_param_ops = {
	.set = bms_last_real_fcc_batt_temp_set,
	.get = param_get_int,
};
module_param_cb(last_real_fcc_batt_temp, &bms_last_real_fcc_batt_temp_param_ops,
					&last_real_fcc_batt_temp, 0644);

static int pm_bms_get_rt_status(struct pm8921_bms_chip *chip, int irq_id)
{
	return pm8xxx_read_irq_stat(chip->dev->parent,
					chip->pmic_bms_irq[irq_id]);
}

static void pm8921_bms_enable_irq(struct pm8921_bms_chip *chip, int interrupt)
{
	if (!__test_and_set_bit(interrupt, chip->enabled_irqs)) {
		dev_dbg(chip->dev, "%s %d\n", __func__,
						chip->pmic_bms_irq[interrupt]);
		enable_irq(chip->pmic_bms_irq[interrupt]);
	}
}

static void pm8921_bms_disable_irq(struct pm8921_bms_chip *chip, int interrupt)
{
	if (__test_and_clear_bit(interrupt, chip->enabled_irqs)) {
		pr_debug("%d\n", chip->pmic_bms_irq[interrupt]);
		disable_irq_nosync(chip->pmic_bms_irq[interrupt]);
	}
}

static int pm_bms_masked_write(struct pm8921_bms_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = pm8xxx_readb(chip->dev->parent, addr, &reg);
	if (rc) {
		pr_err("read failed addr = %03X, rc = %d\n", addr, rc);
		return rc;
	}
	reg &= ~mask;
	reg |= val & mask;
	rc = pm8xxx_writeb(chip->dev->parent, addr, reg);
	if (rc) {
		pr_err("write failed addr = %03X, rc = %d\n", addr, rc);
		return rc;
	}
	return 0;
}

#define HOLD_OREG_DATA		BIT(1)
static int pm_bms_lock_output_data(struct pm8921_bms_chip *chip)
{
	int rc;

	rc = pm_bms_masked_write(chip, BMS_CONTROL, HOLD_OREG_DATA,
					HOLD_OREG_DATA);
	if (rc) {
		pr_err("couldnt lock bms output rc = %d\n", rc);
		return rc;
	}
	return 0;
}

static int pm_bms_unlock_output_data(struct pm8921_bms_chip *chip)
{
	int rc;

	rc = pm_bms_masked_write(chip, BMS_CONTROL, HOLD_OREG_DATA, 0);
	if (rc) {
		pr_err("fail to unlock BMS_CONTROL rc = %d\n", rc);
		return rc;
	}
	return 0;
}

#define SELECT_OUTPUT_DATA	0x1C
#define SELECT_OUTPUT_TYPE_SHIFT	2
#define OCV_FOR_RBATT		0x0
#define VSENSE_FOR_RBATT	0x1
#define VBATT_FOR_RBATT		0x2
#define CC_MSB			0x3
#define CC_LSB			0x4
#define LAST_GOOD_OCV_VALUE	0x5
#define VSENSE_AVG		0x6
#define VBATT_AVG		0x7

static int pm_bms_read_output_data(struct pm8921_bms_chip *chip, int type,
						int16_t *result)
{
	int rc;
	u8 reg;

	if (!result) {
		pr_err("result pointer null\n");
		return -EINVAL;
	}
	*result = 0;
	if (type < OCV_FOR_RBATT || type > VBATT_AVG) {
		pr_err("invalid type %d asked to read\n", type);
		return -EINVAL;
	}

	rc = pm_bms_masked_write(chip, BMS_CONTROL, SELECT_OUTPUT_DATA,
					type << SELECT_OUTPUT_TYPE_SHIFT);
	if (rc) {
		pr_err("fail to select %d type in BMS_CONTROL rc = %d\n",
						type, rc);
		return rc;
	}

	rc = pm8xxx_readb(chip->dev->parent, BMS_OUTPUT0, &reg);
	if (rc) {
		pr_err("fail to read BMS_OUTPUT0 for type %d rc = %d\n",
			type, rc);
		return rc;
	}
	*result = reg;
	rc = pm8xxx_readb(chip->dev->parent, BMS_OUTPUT1, &reg);
	if (rc) {
		pr_err("fail to read BMS_OUTPUT1 for type %d rc = %d\n",
			type, rc);
		return rc;
	}
	*result |= reg << 8;
	pr_debug("type %d result %x", type, *result);
	return 0;
}

#define V_PER_BIT_MUL_FACTOR	97656
#define V_PER_BIT_DIV_FACTOR	1000
#define XOADC_INTRINSIC_OFFSET	0x6000
static int xoadc_reading_to_microvolt(unsigned int a)
{
	if (a <= XOADC_INTRINSIC_OFFSET)
		return 0;

	return (a - XOADC_INTRINSIC_OFFSET)
			* V_PER_BIT_MUL_FACTOR / V_PER_BIT_DIV_FACTOR;
}

#define XOADC_CALIB_UV		625000
#define VBATT_MUL_FACTOR	3
static int adjust_xo_vbatt_reading(struct pm8921_bms_chip *chip,
							unsigned int uv)
{
	u64 numerator, denominator;

	if (uv == 0)
		return 0;

	numerator = ((u64)uv - chip->xoadc_v0625) * XOADC_CALIB_UV;
	denominator =  chip->xoadc_v125 - chip->xoadc_v0625;
	if (denominator == 0)
		return uv * VBATT_MUL_FACTOR;
	return (XOADC_CALIB_UV + div_u64(numerator, denominator))
						* VBATT_MUL_FACTOR;
}

#define CC_RESOLUTION_N_V1	1085069
#define CC_RESOLUTION_D_V1	100000
#define CC_RESOLUTION_N_V2	868056
#define CC_RESOLUTION_D_V2	10000
static s64 cc_to_microvolt_v1(s64 cc)
{
	return div_s64(cc * CC_RESOLUTION_N_V1, CC_RESOLUTION_D_V1);
}

static s64 cc_to_microvolt_v2(s64 cc)
{
	return div_s64(cc * CC_RESOLUTION_N_V2, CC_RESOLUTION_D_V2);
}

static s64 cc_to_microvolt(struct pm8921_bms_chip *chip, s64 cc)
{
	/*
	 * resolution (the value of a single bit) was changed after revision 2.0
	 * for more accurate readings
	 */
	return (chip->revision < PM8XXX_REVISION_8921_2p0) ?
				cc_to_microvolt_v1((s64)cc) :
				cc_to_microvolt_v2((s64)cc);
}

#define CC_READING_TICKS	56
#define SLEEP_CLK_HZ		32764
#define SECONDS_PER_HOUR	3600
/**
 * ccmicrovolt_to_nvh -
 * @cc_uv:  coulumb counter converted to uV
 *
 * RETURNS:	coulumb counter based charge in nVh
 *		(nano Volt Hour)
 */
static s64 ccmicrovolt_to_nvh(s64 cc_uv)
{
	return div_s64(cc_uv * CC_READING_TICKS * 1000,
			SLEEP_CLK_HZ * SECONDS_PER_HOUR);
}

/* returns the signed value read from the hardware */
static int read_cc(struct pm8921_bms_chip *chip, int *result)
{
	int rc;
	uint16_t msw, lsw;

	rc = pm_bms_read_output_data(chip, CC_LSB, &lsw);
	if (rc) {
		pr_err("fail to read CC_LSB rc = %d\n", rc);
		return rc;
	}
	rc = pm_bms_read_output_data(chip, CC_MSB, &msw);
	if (rc) {
		pr_err("fail to read CC_MSB rc = %d\n", rc);
		return rc;
	}
	*result = msw << 16 | lsw;
	pr_debug("msw = %04x lsw = %04x cc = %d\n", msw, lsw, *result);
	return 0;
}

static int adjust_xo_vbatt_reading_for_mbg(struct pm8921_bms_chip *chip,
						int result)
{
	int64_t numerator;
	int64_t denominator;

	if (chip->amux_2_trim_delta == 0)
		return result;

	numerator = (s64)result * 1000000;
	denominator = (1000000 + (410 * (s64)chip->amux_2_trim_delta));
	return div_s64(numerator, denominator);
}

static int convert_vbatt_raw_to_uv(struct pm8921_bms_chip *chip,
					uint16_t reading, int *result)
{
	*result = xoadc_reading_to_microvolt(reading);
	pr_debug("raw = %04x vbatt = %u\n", reading, *result);
	*result = adjust_xo_vbatt_reading(chip, *result);
	pr_debug("after adj vbatt = %u\n", *result);
	*result = adjust_xo_vbatt_reading_for_mbg(chip, *result);
	return 0;
}

static int convert_vsense_to_uv(struct pm8921_bms_chip *chip,
					int16_t reading, int *result)
{
	*result = pm8xxx_ccadc_reading_to_microvolt(chip->revision, reading);
	pr_debug("raw = %04x vsense = %d\n", reading, *result);
	*result = pm8xxx_cc_adjust_for_gain(*result);
	pr_debug("after adj vsense = %d\n", *result);
	return 0;
}

static int read_vsense_avg(struct pm8921_bms_chip *chip, int *result)
{
	int rc;
	int16_t reading;

	rc = pm_bms_read_output_data(chip, VSENSE_AVG, &reading);
	if (rc) {
		pr_err("fail to read VSENSE_AVG rc = %d\n", rc);
		return rc;
	}

	convert_vsense_to_uv(chip, reading, result);
	return 0;
}

static int linear_interpolate(int y0, int x0, int y1, int x1, int x)
{
	if (y0 == y1 || x == x0)
		return y0;
	if (x1 == x0 || x == x1)
		return y1;

	return y0 + ((y1 - y0) * (x - x0) / (x1 - x0));
}

static int interpolate_single_lut(struct single_row_lut *lut, int x)
{
	int i, result;

	if (x < lut->x[0]) {
		pr_debug("x %d less than known range return y = %d lut = %pS\n",
							x, lut->y[0], lut);
		return lut->y[0];
	}
	if (x > lut->x[lut->cols - 1]) {
		pr_debug("x %d more than known range return y = %d lut = %pS\n",
						x, lut->y[lut->cols - 1], lut);
		return lut->y[lut->cols - 1];
	}

	for (i = 0; i < lut->cols; i++)
		if (x <= lut->x[i])
			break;
	if (x == lut->x[i]) {
		result = lut->y[i];
	} else {
		result = linear_interpolate(
			lut->y[i - 1],
			lut->x[i - 1],
			lut->y[i],
			lut->x[i],
			x);
	}
	return result;
}

static int interpolate_fcc(struct pm8921_bms_chip *chip, int batt_temp)
{
	/* batt_temp is in tenths of degC - convert it to degC for lookups */
	batt_temp = batt_temp/10;
	return interpolate_single_lut(chip->fcc_temp_lut, batt_temp);
}

static int interpolate_fcc_adjusted(struct pm8921_bms_chip *chip, int batt_temp)
{
	/* batt_temp is in tenths of degC - convert it to degC for lookups */
	batt_temp = batt_temp/10;
	return interpolate_single_lut(chip->adjusted_fcc_temp_lut, batt_temp);
}

static int interpolate_scalingfactor_fcc(struct pm8921_bms_chip *chip,
								int cycles)
{
	/*
	 * sf table could be null when no battery aging data is available, in
	 * that case return 100%
	 */
	if (chip->fcc_sf_lut)
		return interpolate_single_lut(chip->fcc_sf_lut, cycles);
	else
		return 100;
}

static int interpolate_scalingfactor_pc(struct pm8921_bms_chip *chip,
				int cycles, int pc)
{
	int i, scalefactorrow1, scalefactorrow2, scalefactor;
	int rows, cols;
	int row1 = 0;
	int row2 = 0;

	/*
	 * sf table could be null when no battery aging data is available, in
	 * that case return 100%
	 */
	if (!chip->pc_sf_lut)
		return 100;

	rows = chip->pc_sf_lut->rows;
	cols = chip->pc_sf_lut->cols;
	if (pc > chip->pc_sf_lut->percent[0]) {
		pr_debug("pc %d greater than known pc ranges for sfd\n", pc);
		row1 = 0;
		row2 = 0;
	}
	if (pc < chip->pc_sf_lut->percent[rows - 1]) {
		pr_debug("pc %d less than known pc ranges for sf", pc);
		row1 = rows - 1;
		row2 = rows - 1;
	}
	for (i = 0; i < rows; i++) {
		if (pc == chip->pc_sf_lut->percent[i]) {
			row1 = i;
			row2 = i;
			break;
		}
		if (pc > chip->pc_sf_lut->percent[i]) {
			row1 = i - 1;
			row2 = i;
			break;
		}
	}

	if (cycles < chip->pc_sf_lut->cycles[0])
		cycles = chip->pc_sf_lut->cycles[0];
	if (cycles > chip->pc_sf_lut->cycles[cols - 1])
		cycles = chip->pc_sf_lut->cycles[cols - 1];

	for (i = 0; i < cols; i++)
		if (cycles <= chip->pc_sf_lut->cycles[i])
			break;
	if (cycles == chip->pc_sf_lut->cycles[i]) {
		scalefactor = linear_interpolate(
				chip->pc_sf_lut->sf[row1][i],
				chip->pc_sf_lut->percent[row1],
				chip->pc_sf_lut->sf[row2][i],
				chip->pc_sf_lut->percent[row2],
				pc);
		return scalefactor;
	}

	scalefactorrow1 = linear_interpolate(
				chip->pc_sf_lut->sf[row1][i - 1],
				chip->pc_sf_lut->cycles[i - 1],
				chip->pc_sf_lut->sf[row1][i],
				chip->pc_sf_lut->cycles[i],
				cycles);

	scalefactorrow2 = linear_interpolate(
				chip->pc_sf_lut->sf[row2][i - 1],
				chip->pc_sf_lut->cycles[i - 1],
				chip->pc_sf_lut->sf[row2][i],
				chip->pc_sf_lut->cycles[i],
				cycles);

	scalefactor = linear_interpolate(
				scalefactorrow1,
				chip->pc_sf_lut->percent[row1],
				scalefactorrow2,
				chip->pc_sf_lut->percent[row2],
				pc);

	return scalefactor;
}

static int is_between(int left, int right, int value)
{
	if (left >= right && left >= value && value >= right)
		return 1;
	if (left <= right && left <= value && value <= right)
		return 1;

	return 0;
}

/* get ocv given a soc  -- reverse lookup */
static int interpolate_ocv(struct pm8921_bms_chip *chip,
				int batt_temp_degc, int pc)
{
	int i, ocvrow1, ocvrow2, ocv;
	int rows, cols;
	int row1 = 0;
	int row2 = 0;

	rows = chip->pc_temp_ocv_lut->rows;
	cols = chip->pc_temp_ocv_lut->cols;
	if (pc > chip->pc_temp_ocv_lut->percent[0]) {
		pr_debug("pc %d greater than known pc ranges for sfd\n", pc);
		row1 = 0;
		row2 = 0;
	}
	if (pc < chip->pc_temp_ocv_lut->percent[rows - 1]) {
		pr_debug("pc %d less than known pc ranges for sf\n", pc);
		row1 = rows - 1;
		row2 = rows - 1;
	}
	for (i = 0; i < rows; i++) {
		if (pc == chip->pc_temp_ocv_lut->percent[i]) {
			row1 = i;
			row2 = i;
			break;
		}
		if (pc > chip->pc_temp_ocv_lut->percent[i]) {
			row1 = i - 1;
			row2 = i;
			break;
		}
	}

	if (batt_temp_degc < chip->pc_temp_ocv_lut->temp[0])
		batt_temp_degc = chip->pc_temp_ocv_lut->temp[0];
	if (batt_temp_degc > chip->pc_temp_ocv_lut->temp[cols - 1])
		batt_temp_degc = chip->pc_temp_ocv_lut->temp[cols - 1];

	for (i = 0; i < cols; i++)
		if (batt_temp_degc <= chip->pc_temp_ocv_lut->temp[i])
			break;
	if (batt_temp_degc == chip->pc_temp_ocv_lut->temp[i]) {
		ocv = linear_interpolate(
				chip->pc_temp_ocv_lut->ocv[row1][i],
				chip->pc_temp_ocv_lut->percent[row1],
				chip->pc_temp_ocv_lut->ocv[row2][i],
				chip->pc_temp_ocv_lut->percent[row2],
				pc);
		return ocv;
	}

	ocvrow1 = linear_interpolate(
				chip->pc_temp_ocv_lut->ocv[row1][i - 1],
				chip->pc_temp_ocv_lut->temp[i - 1],
				chip->pc_temp_ocv_lut->ocv[row1][i],
				chip->pc_temp_ocv_lut->temp[i],
				batt_temp_degc);

	ocvrow2 = linear_interpolate(
				chip->pc_temp_ocv_lut->ocv[row2][i - 1],
				chip->pc_temp_ocv_lut->temp[i - 1],
				chip->pc_temp_ocv_lut->ocv[row2][i],
				chip->pc_temp_ocv_lut->temp[i],
				batt_temp_degc);

	ocv = linear_interpolate(
				ocvrow1,
				chip->pc_temp_ocv_lut->percent[row1],
				ocvrow2,
				chip->pc_temp_ocv_lut->percent[row2],
				pc);

	return ocv;
}

static int interpolate_pc(struct pm8921_bms_chip *chip,
				int batt_temp, int ocv)
{
	int i, j, pcj, pcj_minus_one, pc;
	int rows = chip->pc_temp_ocv_lut->rows;
	int cols = chip->pc_temp_ocv_lut->cols;

	/* batt_temp is in tenths of degC - convert it to degC for lookups */
	batt_temp = batt_temp/10;

	if (batt_temp < chip->pc_temp_ocv_lut->temp[0]) {
		pr_debug("batt_temp %d < known temp range for pc\n", batt_temp);
		batt_temp = chip->pc_temp_ocv_lut->temp[0];
	}
	if (batt_temp > chip->pc_temp_ocv_lut->temp[cols - 1]) {
		pr_debug("batt_temp %d > known temp range for pc\n", batt_temp);
		batt_temp = chip->pc_temp_ocv_lut->temp[cols - 1];
	}

	for (j = 0; j < cols; j++)
		if (batt_temp <= chip->pc_temp_ocv_lut->temp[j])
			break;
	if (batt_temp == chip->pc_temp_ocv_lut->temp[j]) {
		/* found an exact match for temp in the table */
		if (ocv >= chip->pc_temp_ocv_lut->ocv[0][j])
			return chip->pc_temp_ocv_lut->percent[0];
		if (ocv <= chip->pc_temp_ocv_lut->ocv[rows - 1][j])
			return chip->pc_temp_ocv_lut->percent[rows - 1];
		for (i = 0; i < rows; i++) {
			if (ocv >= chip->pc_temp_ocv_lut->ocv[i][j]) {
				if (ocv == chip->pc_temp_ocv_lut->ocv[i][j])
					return
					chip->pc_temp_ocv_lut->percent[i];
				pc = linear_interpolate(
					chip->pc_temp_ocv_lut->percent[i],
					chip->pc_temp_ocv_lut->ocv[i][j],
					chip->pc_temp_ocv_lut->percent[i - 1],
					chip->pc_temp_ocv_lut->ocv[i - 1][j],
					ocv);
				return pc;
			}
		}
	}

	/*
	 * batt_temp is within temperature for
	 * column j-1 and j
	 */
	if (ocv >= chip->pc_temp_ocv_lut->ocv[0][j])
		return chip->pc_temp_ocv_lut->percent[0];
	if (ocv <= chip->pc_temp_ocv_lut->ocv[rows - 1][j - 1])
		return chip->pc_temp_ocv_lut->percent[rows - 1];

	pcj_minus_one = 0;
	pcj = 0;
	for (i = 0; i < rows-1; i++) {
		if (pcj == 0
			&& is_between(chip->pc_temp_ocv_lut->ocv[i][j],
				chip->pc_temp_ocv_lut->ocv[i+1][j], ocv)) {
			pcj = linear_interpolate(
				chip->pc_temp_ocv_lut->percent[i],
				chip->pc_temp_ocv_lut->ocv[i][j],
				chip->pc_temp_ocv_lut->percent[i + 1],
				chip->pc_temp_ocv_lut->ocv[i+1][j],
				ocv);
		}

		if (pcj_minus_one == 0
			&& is_between(chip->pc_temp_ocv_lut->ocv[i][j-1],
				chip->pc_temp_ocv_lut->ocv[i+1][j-1], ocv)) {

			pcj_minus_one = linear_interpolate(
				chip->pc_temp_ocv_lut->percent[i],
				chip->pc_temp_ocv_lut->ocv[i][j-1],
				chip->pc_temp_ocv_lut->percent[i + 1],
				chip->pc_temp_ocv_lut->ocv[i+1][j-1],
				ocv);
		}

		if (pcj && pcj_minus_one) {
			pc = linear_interpolate(
				pcj_minus_one,
				chip->pc_temp_ocv_lut->temp[j-1],
				pcj,
				chip->pc_temp_ocv_lut->temp[j],
				batt_temp);
			return pc;
		}
	}

	if (pcj)
		return pcj;

	if (pcj_minus_one)
		return pcj_minus_one;

	pr_debug("%d ocv wasn't found for temp %d in the LUT returning 100%%",
							ocv, batt_temp);
	return 100;
}

#define BMS_MODE_BIT	BIT(6)
#define EN_VBAT_BIT	BIT(5)
#define OVERRIDE_MODE_DELAY_MS	20
int pm8921_bms_get_simultaneous_battery_voltage_and_current(int *ibat_ua,
								int *vbat_uv)
{
	int16_t vsense_raw;
	int16_t vbat_raw;
	int vsense_uv;

	if (the_chip == NULL) {
		pr_err("Called to early\n");
		return -EINVAL;
	}

	mutex_lock(&the_chip->bms_output_lock);

	pm8xxx_writeb(the_chip->dev->parent, BMS_S1_DELAY, 0x00);
	pm_bms_masked_write(the_chip, BMS_CONTROL,
			BMS_MODE_BIT | EN_VBAT_BIT, BMS_MODE_BIT | EN_VBAT_BIT);

	msleep(OVERRIDE_MODE_DELAY_MS);

	pm_bms_lock_output_data(the_chip);
	pm_bms_read_output_data(the_chip, VSENSE_AVG, &vsense_raw);
	pm_bms_read_output_data(the_chip, VBATT_AVG, &vbat_raw);
	pm_bms_unlock_output_data(the_chip);
	pm_bms_masked_write(the_chip, BMS_CONTROL,
			BMS_MODE_BIT | EN_VBAT_BIT, 0);

	pm8xxx_writeb(the_chip->dev->parent, BMS_S1_DELAY, 0x0B);

	mutex_unlock(&the_chip->bms_output_lock);

	convert_vbatt_raw_to_uv(the_chip, vbat_raw, vbat_uv);
	convert_vsense_to_uv(the_chip, vsense_raw, &vsense_uv);
	*ibat_ua = vsense_uv * 1000 / (int)the_chip->r_sense;

	pr_debug("vsense_raw = 0x%x vbat_raw = 0x%x"
			" ibat_ua = %d vbat_uv = %d\n",
			(uint16_t)vsense_raw, (uint16_t)vbat_raw,
			*ibat_ua, *vbat_uv);
	return 0;
}
EXPORT_SYMBOL(pm8921_bms_get_simultaneous_battery_voltage_and_current);

static int read_rbatt_params_raw(struct pm8921_bms_chip *chip,
				struct pm8921_rbatt_params *raw)
{
#ifdef CONFIG_PM8921_TEST_OVERRIDE
	if (chip->user_override)
		return 0;
#endif

	mutex_lock(&chip->bms_output_lock);
	pm_bms_lock_output_data(chip);

	pm_bms_read_output_data(chip,
			OCV_FOR_RBATT, &raw->ocv_for_rbatt_raw);
	pm_bms_read_output_data(chip,
			VBATT_FOR_RBATT, &raw->vbatt_for_rbatt_raw);
	pm_bms_read_output_data(chip,
			VSENSE_FOR_RBATT, &raw->vsense_for_rbatt_raw);

	pm_bms_unlock_output_data(chip);
	mutex_unlock(&chip->bms_output_lock);

	convert_vbatt_raw_to_uv(chip,
			raw->vbatt_for_rbatt_raw, &raw->vbatt_for_rbatt_uv);
	convert_vbatt_raw_to_uv(chip,
			raw->ocv_for_rbatt_raw, &raw->ocv_for_rbatt_uv);

	convert_vsense_to_uv(chip, raw->vsense_for_rbatt_raw,
					&raw->vsense_for_rbatt_uv);

	pr_debug("vbatt_for_rbatt_raw = 0x%x, vbatt_for_rbatt= %duV\n",
			raw->vbatt_for_rbatt_raw, raw->vbatt_for_rbatt_uv);
	pr_debug("ocv_for_rbatt_raw = 0x%x, ocv_for_rbatt= %duV\n",
			raw->ocv_for_rbatt_raw, raw->ocv_for_rbatt_uv);
	pr_debug("vsense_for_rbatt_raw = 0x%x, vsense_for_rbatt= %duV\n",
			raw->vsense_for_rbatt_raw, raw->vsense_for_rbatt_uv);
	return 0;
}

#define MBG_TRANSIENT_ERROR_RAW 51
static void adjust_pon_ocv_raw(struct pm8921_bms_chip *chip,
				struct pm8921_soc_params *raw)
{
	/* in 8921 parts the PON ocv is taken when the MBG is not settled.
	 * decrease the pon ocv by 15mV raw value to account for it
	 * Since a 1/3rd  of vbatt is supplied to the adc the raw value
	 * needs to be adjusted by 5mV worth bits
	 */
	if (raw->last_good_ocv_raw >= MBG_TRANSIENT_ERROR_RAW)
		raw->last_good_ocv_raw -= MBG_TRANSIENT_ERROR_RAW;
}

static int read_soc_params_raw(struct pm8921_bms_chip *chip,
				struct pm8921_soc_params *raw)
{
	mutex_lock(&chip->bms_output_lock);
	pm_bms_lock_output_data(chip);

	pm_bms_read_output_data(chip,
			LAST_GOOD_OCV_VALUE, &raw->last_good_ocv_raw);
	read_cc(chip, &raw->cc);

	pm_bms_unlock_output_data(chip);
	mutex_unlock(&chip->bms_output_lock);

	if (chip->prev_last_good_ocv_raw == 0) {
		chip->prev_last_good_ocv_raw = raw->last_good_ocv_raw;
		adjust_pon_ocv_raw(chip, raw);
		convert_vbatt_raw_to_uv(chip,
			raw->last_good_ocv_raw, &raw->last_good_ocv_uv);
		last_ocv_uv = raw->last_good_ocv_uv;
		pr_debug("PON_OCV_UV = %d\n", last_ocv_uv);
	} else if (chip->prev_last_good_ocv_raw != raw->last_good_ocv_raw) {
		chip->prev_last_good_ocv_raw = raw->last_good_ocv_raw;
		convert_vbatt_raw_to_uv(chip,
			raw->last_good_ocv_raw, &raw->last_good_ocv_uv);
		last_ocv_uv = raw->last_good_ocv_uv;
#ifdef CONFIG_PM8921_EXTENDED_INFO
		chip->meter_offset = 0;
#endif
	} else {
		raw->last_good_ocv_uv = last_ocv_uv;
	}

	pr_debug("0p625 = %duV\n", chip->xoadc_v0625);
	pr_debug("1p25 = %duV\n", chip->xoadc_v125);
	pr_debug("last_good_ocv_raw= 0x%x, last_good_ocv_uv= %duV\n",
			raw->last_good_ocv_raw, raw->last_good_ocv_uv);
	pr_debug("cc_raw= 0x%x\n", raw->cc);
	return 0;
}

static int calculate_rbatt_resume(struct pm8921_bms_chip *chip,
				struct pm8921_rbatt_params *raw)
{
	unsigned int  r_batt;

	if (raw->ocv_for_rbatt_uv == 0
		|| raw->ocv_for_rbatt_uv == raw->vbatt_for_rbatt_uv
		|| raw->vsense_for_rbatt_raw == 0) {
		pr_debug("rbatt readings unavailable ocv = %d, vbatt = %d,"
					"vsen = %d\n",
					raw->ocv_for_rbatt_uv,
					raw->vbatt_for_rbatt_uv,
					raw->vsense_for_rbatt_raw);
		return -EINVAL;
	}
	r_batt = ((raw->ocv_for_rbatt_uv - raw->vbatt_for_rbatt_uv)
			* chip->r_sense) / raw->vsense_for_rbatt_uv;
	pr_debug("r_batt = %umilliOhms", r_batt);
	return r_batt;
}

static int calculate_fcc_uah(struct pm8921_bms_chip *chip, int batt_temp,
							int chargecycles)
{
	int initfcc, result, scalefactor = 0;
#ifdef CONFIG_PM8921_FLOAT_CHARGE
	chip->adjusted_fcc_temp_lut = NULL;
#endif
	if (chip->adjusted_fcc_temp_lut == NULL) {
		initfcc = interpolate_fcc(chip, batt_temp);

		scalefactor = interpolate_scalingfactor_fcc(chip, chargecycles);
#ifdef CONFIG_PM8921_FLOAT_CHARGE
		if (bms_aged_capacity <= 50) {
			if(bms_aged_capacity != 0)
				scalefactor = 50;
		} else if (bms_aged_capacity >= 100) {
			scalefactor = 100;
		} else {
			scalefactor = bms_aged_capacity;
		}
#endif
		/* Multiply the initial FCC value by the scale factor. */
		result = (initfcc * scalefactor * 1000) / 100;
		pr_debug("fcc = %d uAh\n", result);
		return result;
	} else {
		return 1000 * interpolate_fcc_adjusted(chip, batt_temp);
	}
}

static int get_battery_uvolts(struct pm8921_bms_chip *chip, int *uvolts)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(chip->vbat_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					chip->vbat_channel, rc);
		return rc;
	}
	pr_debug("mvolts phy = %lld meas = 0x%llx", result.physical,
						result.measurement);
	*uvolts = (int)result.physical;
	return 0;
}

static int adc_based_ocv(struct pm8921_bms_chip *chip, int *ocv)
{
	int vbatt, rbatt, ibatt_ua, rc;

	rc = get_battery_uvolts(chip, &vbatt);
	if (rc) {
		pr_err("failed to read vbatt from adc rc = %d\n", rc);
		return rc;
	}

	rc =  pm8921_bms_get_battery_current(&ibatt_ua);
	if (rc) {
		pr_err("failed to read batt current rc = %d\n", rc);
		return rc;
	}

	rbatt = (last_rbatt < 0) ? DEFAULT_RBATT_MOHMS : last_rbatt;
	*ocv = vbatt + (ibatt_ua * rbatt)/1000;
	return 0;
}

static int calculate_pc(struct pm8921_bms_chip *chip, int ocv_uv, int batt_temp,
							int chargecycles)
{
	int pc, scalefactor;

	pc = interpolate_pc(chip, batt_temp, ocv_uv / 1000);
	pr_debug("pc = %u for ocv = %dmicroVolts batt_temp = %d\n",
					pc, ocv_uv, batt_temp);

	scalefactor = interpolate_scalingfactor_pc(chip, chargecycles, pc);
	pr_debug("scalefactor = %u batt_temp = %d\n", scalefactor, batt_temp);

	/* Multiply the initial FCC value by the scale factor. */
	pc = (pc * scalefactor) / 100;
	return pc;
}

/**
 * calculate_cc_uah -
 * @chip:		the bms chip pointer
 * @cc:			the cc reading from bms h/w
 * @val:		return value
 * @coulumb_counter:	adjusted coulumb counter for 100%
 *
 * RETURNS: in val pointer coulumb counter based charger in uAh
 *          (micro Amp hour)
 */
static void calculate_cc_uah(struct pm8921_bms_chip *chip, int cc, int *val)
{
	int64_t cc_voltage_uv, cc_nvh, cc_uah;

	cc_voltage_uv = cc;
	cc_voltage_uv -= chip->cc_reading_at_100;
	pr_debug("cc = %d. after subtracting %d cc = %lld\n",
					cc, chip->cc_reading_at_100,
					cc_voltage_uv);
	cc_voltage_uv = cc_to_microvolt(chip, cc_voltage_uv);
	cc_voltage_uv = pm8xxx_cc_adjust_for_gain(cc_voltage_uv);
	pr_debug("cc_voltage_uv = %lld microvolts\n", cc_voltage_uv);
	cc_nvh = ccmicrovolt_to_nvh(cc_voltage_uv);
	pr_debug("cc_nvh = %lld nano_volt_hour\n", cc_nvh);
	cc_uah = div_s64(cc_nvh, chip->r_sense);
	*val = cc_uah;
}

static int calculate_unusable_charge_uah(struct pm8921_bms_chip *chip,
					struct pm8921_soc_params *raw,
				 int fcc_uah, int batt_temp, int chargecycles)
{
	int rbatt, voltage_unusable_uv, pc_unusable;

	rbatt = (last_rbatt < 0) ? DEFAULT_RBATT_MOHMS : last_rbatt;

	/* calculate unusable charge */
	voltage_unusable_uv = (rbatt * chip->i_test)
						+ (chip->v_failure * 1000);
	pc_unusable = calculate_pc(chip, voltage_unusable_uv,
						batt_temp, chargecycles);
	pr_debug("rbatt = %umilliOhms unusable_v =%d unusable_pc = %d\n",
			rbatt, voltage_unusable_uv, pc_unusable);
	return (fcc_uah * pc_unusable) / 100;
}

/* calculate remainging charge at the time of ocv */
static int calculate_remaining_charge_uah(struct pm8921_bms_chip *chip,
						struct pm8921_soc_params *raw,
						int fcc_uah, int batt_temp,
						int chargecycles)
{
	int  ocv, pc;

	/* calculate remainging charge */
	ocv = 0;
	if (chip->ocv_reading_at_100 != raw->last_good_ocv_raw) {
		chip->ocv_reading_at_100 = 0;
		chip->cc_reading_at_100 = 0;
		ocv = raw->last_good_ocv_uv;
	} else {
		/*
		 * force 100% ocv by selecting the highest voltage the
		 * battery could every reach
		 */
		ocv = chip->max_voltage_uv;
	}

	if (ocv == 0) {
		ocv = last_ocv_uv;
		pr_debug("ocv not available using last_ocv_uv=%d\n", ocv);
	}

	pc = calculate_pc(chip, ocv, batt_temp, chargecycles);
	pr_debug("ocv = %d pc = %d\n", ocv, pc);
	return (fcc_uah * pc) / 100;
}

static void calculate_soc_params(struct pm8921_bms_chip *chip,
						struct pm8921_soc_params *raw,
						int batt_temp, int chargecycles,
						int *fcc_uah,
						int *unusable_charge_uah,
						int *remaining_charge_uah,
						int *cc_uah)
{
	unsigned long flags;

	*fcc_uah = calculate_fcc_uah(chip, batt_temp, chargecycles);
	pr_debug("FCC = %uuAh batt_temp = %d, cycles = %d\n",
					*fcc_uah, batt_temp, chargecycles);

	*unusable_charge_uah = calculate_unusable_charge_uah(chip, raw,
					*fcc_uah, batt_temp, chargecycles);

	pr_debug("UUC = %uuAh\n", *unusable_charge_uah);

	spin_lock_irqsave(&chip->bms_100_lock, flags);
	/* calculate remainging charge */
	*remaining_charge_uah = calculate_remaining_charge_uah(chip, raw,
					*fcc_uah, batt_temp, chargecycles);
	pr_debug("RC = %uuAh\n", *remaining_charge_uah);

	/* calculate cc micro_volt_hour */
	calculate_cc_uah(chip, raw->cc, cc_uah);
	pr_debug("cc_uah = %duAh raw->cc = %x cc = %lld after subtracting %d\n",
				*cc_uah, raw->cc,
				(int64_t)raw->cc - chip->cc_reading_at_100,
				chip->cc_reading_at_100);
	spin_unlock_irqrestore(&chip->bms_100_lock, flags);
}

static int calculate_real_fcc_uah(struct pm8921_bms_chip *chip,
				struct pm8921_soc_params *raw,
				int batt_temp, int chargecycles,
				int *ret_fcc_uah)
{
	int fcc_uah, unusable_charge_uah;
	int remaining_charge_uah;
	int cc_uah;
	int real_fcc_uah;

	calculate_soc_params(chip, raw, batt_temp, chargecycles,
						&fcc_uah,
						&unusable_charge_uah,
						&remaining_charge_uah,
						&cc_uah);

	real_fcc_uah = remaining_charge_uah - cc_uah;
	*ret_fcc_uah = fcc_uah;
	pr_debug("real_fcc = %d, RC = %d CC = %d fcc = %d\n",
			real_fcc_uah, remaining_charge_uah, cc_uah, fcc_uah);
	return real_fcc_uah;
}

#define IGNORE_SOC_TEMP_DECIDEG		50
static void backup_soc(struct pm8921_bms_chip *chip, int batt_temp, int soc)
{
	int ret;

	/* Can't use 15 because old BL was writing 15 to this spot */
	if (soc == 15)
		soc = 16;

	if (soc == 0)
		soc = 0xFF;

	if (batt_temp < IGNORE_SOC_TEMP_DECIDEG)
		soc = 0;

	ret = pm8xxx_writeb(the_chip->dev->parent, TEMP_SOC_STORAGE, soc);
	if (ret)
		pr_err("pm8xxx_writeb errored = %d\n", ret);
}

static void read_shutdown_soc(struct pm8921_bms_chip *chip)
{
	int rc;
	u8 temp;

	rc = pm8xxx_readb(chip->dev->parent, TEMP_SOC_STORAGE, &temp);
	if (rc) {
		pr_err("failed to read addr = %d %d\n", TEMP_SOC_STORAGE, rc);
	} else {
		/* Can't use 15 because old BL was writing 15 to this spot */
		if (temp != 15)
			chip->shutdown_soc = temp;
	}
	pr_info("shutdown_soc = %d\n", chip->shutdown_soc);
}

void pm8921_bms_invalidate_shutdown_soc(void)
{
	pr_info("Invalidating shutdown soc - the battery was removed\n");
	mutex_lock(&soc_invalidation_mutex);
	shutdown_soc_invalid = 1;
	last_soc = -EINVAL;
	if (the_chip) {
		/* reset to pon ocv undoing what the adjusting did */
		if (the_chip->pon_ocv_uv) {
			last_ocv_uv = the_chip->pon_ocv_uv;
			pr_info("resetting ocv to pon_ocv = %d\n",
						the_chip->pon_ocv_uv);
		}
	}
	mutex_unlock(&soc_invalidation_mutex);
}
EXPORT_SYMBOL(pm8921_bms_invalidate_shutdown_soc);

static int adjust_remaining_charge_for_shutdown_soc(
						struct pm8921_bms_chip *chip,
						int batt_temp,
						int chargecycles,
						int fcc_uah,
						int uuc_uah,
						int cc_uah,
						int remaining_charge_uah)
{
	s64 rc;
	int pc;
	int ocv, ocv_uv;
	int batt_temp_degc = batt_temp / 10;
	int new_pc;
	int shutdown_soc;
	int prev_new_pc, prev_ocv;

	mutex_lock(&soc_invalidation_mutex);
	shutdown_soc = chip->shutdown_soc;
	/*
	 * value of zero means the shutdown soc should not be used, the battery
	 * was removed for extended period, the coincell capacitor could have
	 * drained
	 */
	if (shutdown_soc == 0) {
		rc = remaining_charge_uah;
		goto out;
	}

	/*
	 * shutdown_soc_invalid means the shutdown soc should not be used,
	 * the battery was removed for a small period
	 */
	if (shutdown_soc_invalid) {
		rc = remaining_charge_uah;
		goto out;
	}

	/* value of 0xFF means shutdown soc was 0% */
	if (shutdown_soc == 0xFF)
		shutdown_soc = 0;

	pr_info("shutdown_soc = %d forcing it now\n", shutdown_soc);

	rc = (s64)shutdown_soc * (fcc_uah - uuc_uah);
	rc = div_s64(rc, 100) + cc_uah + uuc_uah;
	pc = DIV_ROUND_CLOSEST((int)rc * 100, fcc_uah);
	pc = clamp(pc, 0, 100);

	ocv = interpolate_ocv(chip, batt_temp_degc, pc);

	pr_info("To force shutdown_soc = %d, rc = %d, pc = %d, ocv mv = %d\n",
				shutdown_soc, (int)rc, pc, ocv);
	new_pc = interpolate_pc(chip, batt_temp_degc, ocv);
	pr_info("test revlookup pc = %d for ocv = %d\n", new_pc, ocv);

	while (abs(new_pc - pc) > 1) {
		int delta_mv = 1;

		prev_new_pc = new_pc;
		prev_ocv = ocv;

		if (new_pc > pc)
			delta_mv = -1 * delta_mv;

		ocv = ocv + delta_mv;
		new_pc = interpolate_pc(chip, batt_temp_degc, ocv);
		pr_info("test revlookup pc = %d for ocv = %d\n", new_pc, ocv);

		/* Check that we crossed pc */
		if (((prev_new_pc > pc) && (new_pc < pc)) ||
		    ((prev_new_pc < pc) && (new_pc > pc))) {
			/* Select the value that is closer to pc */
			if (abs(new_pc - pc) > abs(prev_new_pc - pc)) {
				new_pc = prev_new_pc;
				ocv = prev_ocv;
				pr_info("Flip so use prev_new_pc\n");
			}
			break;
		}
	}

	ocv_uv = ocv * 1000;

	chip->pon_ocv_uv = last_ocv_uv;
	last_ocv_uv = ocv_uv;
out:
	mutex_unlock(&soc_invalidation_mutex);
	return (int)rc;
}

#define SHOW_SHUTDOWN_SOC_MS	30000
static void shutdown_soc_work(struct work_struct *work)
{
	struct pm8921_bms_chip *chip = container_of(work,
				struct pm8921_bms_chip, shutdown_soc_work.work);

	pr_info("not forcing shutdown soc anymore\n");
	/* it has been  30 seconds since init, no need to show shutdown soc */
	chip->shutdown_soc_timer_expired = 1;
}

#ifdef CONFIG_PM8921_EXTENDED_INFO
void pm8921_bms_voltage_based_capacity(int batt_mvolt,
				       int batt_mcurr,
				       int batt_temp)
{
	int charging_src;
	int volt_ocv_now = (batt_mvolt * 1000) +
		((batt_mcurr * the_chip->init_rbatt * the_chip->k_factor)) /
		100;
	int soc_adjusted = calculate_pc(the_chip, volt_ocv_now, batt_temp,
					last_chargecycles);

	pr_debug("volt_ocv_now = %d, soc_adjusted = %d, rbatt = %d, k = %d\n",
		 volt_ocv_now, soc_adjusted,the_chip->init_rbatt,
		 the_chip->k_factor);

	if (!pm8921_is_battery_charging(&charging_src)) {
		if (soc_adjusted <= START_METER_OFFSET_SOC)
			the_chip->meter_offset += 2;
	} else {
		if (the_chip->meter_offset >= 2)
			the_chip->meter_offset -= 2;
		else
			the_chip->meter_offset = 0;
	}

	pr_debug("meter_offset = %d\n", the_chip->meter_offset);
}
#endif
/*
 * Remaining Usable Charge = remaining_charge (charge at ocv instance)
 *				- coloumb counter charge
 *				- unusable charge (due to battery resistance)
 * SOC% = (remaining usable charge/ fcc - usable_charge);
 */
static int calculate_state_of_charge(struct pm8921_bms_chip *chip,
					struct pm8921_soc_params *raw,
					int batt_temp, int chargecycles)
{
	int remaining_usable_charge_uah, fcc_uah, unusable_charge_uah;
	int remaining_charge_uah, soc;
	int update_userspace = 1;
	int cc_uah;
	int charging_src = 0;
	static int first_time;

#ifdef CONFIG_PM8921_TEST_OVERRIDE
	if (chip->user_override)
		return last_soc;
#endif

	calculate_soc_params(chip, raw, batt_temp, chargecycles,
						&fcc_uah,
						&unusable_charge_uah,
						&remaining_charge_uah,
						&cc_uah);
	if (first_time == 0) {
		/*
		 * soc for the first time - use shutdown soc
		 * to adjust pon ocv
		 */
		remaining_charge_uah = adjust_remaining_charge_for_shutdown_soc(
						chip,
						batt_temp, chargecycles,
						fcc_uah, unusable_charge_uah,
						cc_uah, remaining_charge_uah);
		first_time = 1;
	}

	/* calculate remaining usable charge */
	remaining_usable_charge_uah = remaining_charge_uah
					- cc_uah
					- unusable_charge_uah;

	pr_debug("RUC = %duAh\n", remaining_usable_charge_uah);
	if (fcc_uah - unusable_charge_uah <= 0) {
		pr_warn("FCC = %duAh, UUC = %duAh forcing soc = 0\n",
						fcc_uah, unusable_charge_uah);
		soc = 0;
	} else {
		soc = (remaining_usable_charge_uah * 100)
			/ (fcc_uah - unusable_charge_uah);
	}

#ifdef CONFIG_PM8921_EXTENDED_INFO
	soc = soc - chip->meter_offset;
#endif

	/* Round up soc to account for remainder */
	if ((soc > 0) && (soc <= 99))
		soc += 1;

	if (soc > 100) {
		soc = 100;
		if (chip->start_percent == -EINVAL)
			pm8921_bms_charging_full();
	}
	pr_debug("SOC = %d%%\n", soc);

	if (bms_fake_battery != -EINVAL) {
		pr_debug("Returning Fake SOC = %d%%\n", bms_fake_battery);
		return bms_fake_battery;
	}

	if (soc < 0) {
		pr_err("bad rem_usb_chg = %d rem_chg %d,"
				"cc_uah %d, unusb_chg %d\n",
				remaining_usable_charge_uah,
				remaining_charge_uah,
				cc_uah, unusable_charge_uah);

		pr_err("for bad rem_usb_chg last_ocv_uv = %d"
				"chargecycles = %d, batt_temp = %d"
				"fcc = %d soc =%d\n",
				last_ocv_uv, chargecycles, batt_temp,
				fcc_uah, soc);
		update_userspace = 0;
		soc = 0;
	}

	if (last_soc == -EINVAL || soc <= last_soc) {
		last_soc = update_userspace ? soc : last_soc;
	}

	/*
	 * soc > last_soc
	 * the device must be charging for reporting a higher soc, if not ignore
	 * this soc and continue reporting the last_soc
	 */
	else if (pm8921_is_battery_charging(&charging_src)) {
		last_soc = soc;
	} else {
		pr_debug("soc = %d reporting last_soc = %d\n", soc, last_soc);
	}

	if (chip->shutdown_soc != 0
			&& !shutdown_soc_invalid
			&& !chip->shutdown_soc_timer_expired) {
		/*
		 * force shutdown soc if it is valid and the shutdown soc show
		 * timer has not expired
		 */
		if (chip->shutdown_soc != 0xFF)
			last_soc = chip->shutdown_soc;
		else
			last_soc = 0;

		pr_info("Forcing SHUTDOWN_SOC = %d\n", last_soc);
	}

	backup_soc(chip, batt_temp, last_soc);
	pr_debug("Reported SOC = %d\n", last_soc);

	return last_soc;
}

static void calib_hkadc(struct pm8921_bms_chip *chip)
{
	int voltage, rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(the_chip->ref1p25v_channel, &result);
	if (rc) {
		pr_err("ADC failed for 1.25volts rc = %d\n", rc);
		return;
	}
	voltage = xoadc_reading_to_microvolt(result.adc_code);

	pr_debug("result 1.25v = 0x%x, voltage = %duV adc_meas = %lld\n",
				result.adc_code, voltage, result.measurement);

	chip->xoadc_v125 = voltage;

	rc = pm8xxx_adc_read(the_chip->ref625mv_channel, &result);
	if (rc) {
		pr_err("ADC failed for 1.25volts rc = %d\n", rc);
		return;
	}
	voltage = xoadc_reading_to_microvolt(result.adc_code);
	pr_debug("result 0.625V = 0x%x, voltage = %duV adc_meas = %lld\n",
				result.adc_code, voltage, result.measurement);

	chip->xoadc_v0625 = voltage;
}

static void calibrate_hkadc_work(struct work_struct *work)
{
	struct pm8921_bms_chip *chip = container_of(work,
				struct pm8921_bms_chip, calib_hkadc_work);

	calib_hkadc(chip);
}

void pm8921_bms_calibrate_hkadc(void)
{
	schedule_work(&the_chip->calib_hkadc_work);
}

static void calibrate_ccadc_work(struct work_struct *work)
{
	struct pm8921_bms_chip *chip = container_of(work,
				struct pm8921_bms_chip, calib_ccadc_work.work);

	pm8xxx_calib_ccadc();
	calib_hkadc(chip);
	schedule_delayed_work(&chip->calib_ccadc_work,
			round_jiffies_relative(msecs_to_jiffies
			(chip->calib_delay_ms)));
}

int pm8921_bms_get_vsense_avg(int *result)
{
	int rc = -EINVAL;

	if (the_chip) {
		mutex_lock(&the_chip->bms_output_lock);
		pm_bms_lock_output_data(the_chip);
		rc = read_vsense_avg(the_chip, result);
		pm_bms_unlock_output_data(the_chip);
		mutex_unlock(&the_chip->bms_output_lock);
	}

	pr_err("called before initialization\n");
	return rc;
}
EXPORT_SYMBOL(pm8921_bms_get_vsense_avg);

int pm8921_bms_get_battery_current(int *result_ua)
{
	int vsense;

	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}
	if (the_chip->r_sense == 0) {
		pr_err("r_sense is zero\n");
		return -EINVAL;
	}

	mutex_lock(&the_chip->bms_output_lock);
	pm_bms_lock_output_data(the_chip);
	read_vsense_avg(the_chip, &vsense);
	pm_bms_unlock_output_data(the_chip);
	mutex_unlock(&the_chip->bms_output_lock);
	pr_debug("vsense=%duV\n", vsense);
	/* cast for signed division */
	*result_ua = vsense * 1000 / (int)the_chip->r_sense;
	pr_debug("ibat=%duA\n", *result_ua);
	return 0;
}
EXPORT_SYMBOL(pm8921_bms_get_battery_current);

int pm8921_bms_get_percent_charge(void)
{
	int batt_temp, rc;
	struct pm8xxx_adc_chan_result result;
	struct pm8921_soc_params raw;

	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}

	rc = pm8xxx_adc_read(the_chip->batt_temp_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					the_chip->batt_temp_channel, rc);
		return rc;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx", result.physical,
						result.measurement);
	batt_temp = (int)result.physical;

	read_soc_params_raw(the_chip, &raw);

	return calculate_state_of_charge(the_chip, &raw,
					batt_temp, last_chargecycles);
}
EXPORT_SYMBOL_GPL(pm8921_bms_get_percent_charge);

int pm8921_bms_get_cc_uah(int *result)
{
	struct pm8921_soc_params raw;

	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}
	if (the_chip->r_sense == 0) {
		pr_err("r_sense is zero\n");
		return -EINVAL;
	}

	read_soc_params_raw(the_chip, &raw);
	calculate_cc_uah(the_chip, raw.cc, result);

	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_bms_get_cc_uah);

int pm8921_bms_get_fcc(void)
{
	int batt_temp, rc;
	struct pm8xxx_adc_chan_result result;

	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}

	rc = pm8xxx_adc_read(the_chip->batt_temp_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					the_chip->batt_temp_channel, rc);
		return rc;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx", result.physical,
						result.measurement);
	batt_temp = (int)result.physical;
	return calculate_fcc_uah(the_chip, batt_temp, last_chargecycles);
}
EXPORT_SYMBOL_GPL(pm8921_bms_get_fcc);

int pm8921_bms_get_aged_capacity(int *result)
{
	*result = bms_aged_capacity;
	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_bms_get_aged_capacity);

#define IBAT_TOL_MASK		0x0F
#define OCV_TOL_MASK			0xF0
#define IBAT_TOL_DEFAULT	0x03
#define IBAT_TOL_NOCHG		0x0F
void pm8921_bms_charging_began(void)
{
	int batt_temp, rc;
	struct pm8xxx_adc_chan_result result;
	struct pm8921_soc_params raw;

#ifdef CONFIG_PM8921_TEST_OVERRIDE
	if (the_chip->user_override)
		return;
#endif

	rc = pm8xxx_adc_read(the_chip->batt_temp_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
				the_chip->batt_temp_channel, rc);
		return;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	batt_temp = (int)result.physical;

	read_soc_params_raw(the_chip, &raw);

	the_chip->start_percent = calculate_state_of_charge(the_chip, &raw,
					batt_temp, last_chargecycles);
	bms_start_percent = the_chip->start_percent;
#ifdef CONFIG_PM8921_EXTENDED_INFO
	bms_meter_offset = (int) the_chip->meter_offset;
#endif
	bms_start_ocv_uv = raw.last_good_ocv_uv;
	calculate_cc_uah(the_chip, raw.cc, &bms_start_cc_uah);
	pm_bms_masked_write(the_chip, BMS_TOLERANCES,
			IBAT_TOL_MASK, IBAT_TOL_DEFAULT);
	pr_debug("start_percent = %u%%\n", the_chip->start_percent);
}
EXPORT_SYMBOL_GPL(pm8921_bms_charging_began);

#define DELTA_FCC_PERCENT	3
void pm8921_bms_charging_end(int is_battery_full)
{
	int batt_temp, rc;
	struct pm8xxx_adc_chan_result result;
	struct pm8921_soc_params raw;

	if (the_chip == NULL)
		return;

#ifdef CONFIG_PM8921_TEST_OVERRIDE
	if (the_chip->user_override)
		goto calculate_percent;
#endif

	rc = pm8xxx_adc_read(the_chip->batt_temp_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
				the_chip->batt_temp_channel, rc);
		return;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	batt_temp = (int)result.physical;

	read_soc_params_raw(the_chip, &raw);

	calculate_cc_uah(the_chip, raw.cc, &bms_end_cc_uah);

	if (is_battery_full) {
		unsigned long flags;
		int fcc_uah, new_fcc_uah, delta_fcc_uah;

		new_fcc_uah = calculate_real_fcc_uah(the_chip, &raw,
						batt_temp, last_chargecycles,
						&fcc_uah);
		delta_fcc_uah = new_fcc_uah - fcc_uah;
		if (delta_fcc_uah < 0)
			delta_fcc_uah = -delta_fcc_uah;

		if (delta_fcc_uah * 100  <= (DELTA_FCC_PERCENT * fcc_uah)) {
			pr_debug("delta_fcc=%d < %d percent of fcc=%d\n",
				delta_fcc_uah, DELTA_FCC_PERCENT, fcc_uah);
			last_real_fcc_mah = new_fcc_uah/1000;
			last_real_fcc_batt_temp = batt_temp;
			readjust_fcc_table();
		} else {
			pr_debug("delta_fcc=%d > %d percent of fcc=%d"
				"will not update real fcc\n",
				delta_fcc_uah, DELTA_FCC_PERCENT, fcc_uah);
		}

		spin_lock_irqsave(&the_chip->bms_100_lock, flags);
		the_chip->ocv_reading_at_100 = raw.last_good_ocv_raw;
		the_chip->cc_reading_at_100 = raw.cc;
		spin_unlock_irqrestore(&the_chip->bms_100_lock, flags);
		pr_debug("EOC ocv_reading = 0x%x cc = %d\n",
				the_chip->ocv_reading_at_100,
				the_chip->cc_reading_at_100);
	}

	the_chip->end_percent = calculate_state_of_charge(the_chip, &raw,
					batt_temp, last_chargecycles);

	bms_end_percent = the_chip->end_percent;
	bms_end_ocv_uv = raw.last_good_ocv_uv;

#ifdef CONFIG_PM8921_TEST_OVERRIDE
calculate_percent:
#endif
	if (the_chip->end_percent > the_chip->start_percent) {
		last_charge_increase +=
			the_chip->end_percent - the_chip->start_percent;
		if (last_charge_increase > 100) {
			last_chargecycles++;
			last_charge_increase = last_charge_increase % 100;
		}
	}
	pr_debug("end_percent = %u%% last_charge_increase = %d"
			"last_chargecycles = %d\n",
			the_chip->end_percent,
			last_charge_increase,
			last_chargecycles);
	the_chip->start_percent = -EINVAL;
	the_chip->end_percent = -EINVAL;
}
EXPORT_SYMBOL_GPL(pm8921_bms_charging_end);

void pm8921_bms_no_external_accy(void)
{
	pm_bms_masked_write(the_chip, BMS_TOLERANCES,
				IBAT_TOL_MASK, IBAT_TOL_NOCHG);
}
EXPORT_SYMBOL_GPL(pm8921_bms_no_external_accy);

#ifdef CONFIG_PM8921_FLOAT_CHARGE
void pm8921_bms_charging_full(void)
{
	struct pm8921_soc_params raw;
	unsigned long flags;

	if (the_chip == NULL)
		return;

#ifdef CONFIG_PM8921_TEST_OVERRIDE
	if (the_chip->user_override)
		return;
#endif

	read_soc_params_raw(the_chip, &raw);

	spin_lock_irqsave(&the_chip->bms_100_lock, flags);
	the_chip->ocv_reading_at_100 = raw.last_good_ocv_raw;
	the_chip->cc_reading_at_100 = raw.cc;
	spin_unlock_irqrestore(&the_chip->bms_100_lock, flags);
	pr_debug("EOC ocv_reading = 0x%x cc = %d\n",
		 the_chip->ocv_reading_at_100,
		 the_chip->cc_reading_at_100);
}
#endif

static irqreturn_t pm8921_bms_sbi_write_ok_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_cc_thr_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_vsense_thr_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_vsense_for_r_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_ocv_for_r_handler(int irq, void *data)
{
	struct pm8921_bms_chip *chip = data;

	pr_debug("irq = %d triggered", irq);
	schedule_work(&chip->calib_hkadc_work);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_good_ocv_handler(int irq, void *data)
{
	struct pm8921_bms_chip *chip = data;

	pr_debug("irq = %d triggered", irq);
	schedule_work(&chip->calib_hkadc_work);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_vsense_avg_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

struct pm_bms_irq_init_data {
	unsigned int	irq_id;
	char		*name;
	unsigned long	flags;
	irqreturn_t	(*handler)(int, void *);
};

#define BMS_IRQ(_id, _flags, _handler) \
{ \
	.irq_id		= _id, \
	.name		= #_id, \
	.flags		= _flags, \
	.handler	= _handler, \
}

struct pm_bms_irq_init_data bms_irq_data[] = {
	BMS_IRQ(PM8921_BMS_SBI_WRITE_OK, IRQF_TRIGGER_RISING,
				pm8921_bms_sbi_write_ok_handler),
	BMS_IRQ(PM8921_BMS_CC_THR, IRQF_TRIGGER_RISING,
				pm8921_bms_cc_thr_handler),
	BMS_IRQ(PM8921_BMS_VSENSE_THR, IRQF_TRIGGER_RISING,
				pm8921_bms_vsense_thr_handler),
	BMS_IRQ(PM8921_BMS_VSENSE_FOR_R, IRQF_TRIGGER_RISING,
				pm8921_bms_vsense_for_r_handler),
	BMS_IRQ(PM8921_BMS_OCV_FOR_R, IRQF_TRIGGER_RISING,
				pm8921_bms_ocv_for_r_handler),
	BMS_IRQ(PM8921_BMS_GOOD_OCV, IRQF_TRIGGER_RISING,
				pm8921_bms_good_ocv_handler),
	BMS_IRQ(PM8921_BMS_VSENSE_AVG, IRQF_TRIGGER_RISING,
				pm8921_bms_vsense_avg_handler),
};

static void free_irqs(struct pm8921_bms_chip *chip)
{
	int i;

	for (i = 0; i < PM_BMS_MAX_INTS; i++)
		if (chip->pmic_bms_irq[i]) {
			free_irq(chip->pmic_bms_irq[i], NULL);
			chip->pmic_bms_irq[i] = 0;
		}
}

static int __devinit request_irqs(struct pm8921_bms_chip *chip,
					struct platform_device *pdev)
{
	struct resource *res;
	int ret, i;

	ret = 0;
	bitmap_fill(chip->enabled_irqs, PM_BMS_MAX_INTS);

	for (i = 0; i < ARRAY_SIZE(bms_irq_data); i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				bms_irq_data[i].name);
		if (res == NULL) {
			pr_err("couldn't find %s\n", bms_irq_data[i].name);
			goto err_out;
		}
		ret = request_irq(res->start, bms_irq_data[i].handler,
			bms_irq_data[i].flags,
			bms_irq_data[i].name, chip);
		if (ret < 0) {
			pr_err("couldn't request %d (%s) %d\n", res->start,
					bms_irq_data[i].name, ret);
			goto err_out;
		}
		chip->pmic_bms_irq[bms_irq_data[i].irq_id] = res->start;
		pm8921_bms_disable_irq(chip, bms_irq_data[i].irq_id);
	}
	return 0;

err_out:
	free_irqs(chip);
	return -EINVAL;
}

static int pm8921_bms_suspend(struct device *dev)
{
	int rc;
	struct pm8xxx_adc_chan_result result;
	struct pm8921_bms_chip *chip = dev_get_drvdata(dev);

	chip->batt_temp_suspend = 0;
	rc = pm8xxx_adc_read(chip->batt_temp_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					chip->batt_temp_channel, rc);
	}
	chip->batt_temp_suspend = (int)result.physical;
	return 0;
}

static int pm8921_bms_resume(struct device *dev)
{
	struct pm8921_rbatt_params raw;
	struct pm8921_bms_chip *chip = dev_get_drvdata(dev);
	int rbatt;

	read_rbatt_params_raw(chip, &raw);
	rbatt = calculate_rbatt_resume(chip, &raw);
	if (rbatt > 0) /* TODO Check for rbatt values bound based on cycles */
		last_rbatt = rbatt;
	chip->batt_temp_suspend = -EINVAL;
	return 0;
}

static const struct dev_pm_ops pm8921_pm_ops = {
	.suspend	= pm8921_bms_suspend,
	.resume		= pm8921_bms_resume,
};
#define EN_BMS_BIT	BIT(7)
#define EN_PON_HS_BIT	BIT(0)
static int __devinit pm8921_bms_hw_init(struct pm8921_bms_chip *chip)
{
	int rc;

	rc = pm_bms_masked_write(chip, BMS_CONTROL,
			EN_BMS_BIT | EN_PON_HS_BIT, EN_BMS_BIT | EN_PON_HS_BIT);
	if (rc) {
		pr_err("failed to enable pon and bms addr = %d %d",
				BMS_CONTROL, rc);
	}

	/* The charger will call start charge later if usb is present */
	pm_bms_masked_write(chip, BMS_TOLERANCES,
				IBAT_TOL_MASK, IBAT_TOL_NOCHG);
	return 0;
}

static void check_initial_ocv(struct pm8921_bms_chip *chip)
{
	int ocv_uv, rc;
	int16_t ocv_raw;

	/*
	 * Check if a ocv is available in bms hw,
	 * if not compute it here at boot time and save it
	 * in the last_ocv_uv.
	 */
	ocv_uv = 0;
	pm_bms_read_output_data(chip, LAST_GOOD_OCV_VALUE, &ocv_raw);
	rc = convert_vbatt_raw_to_uv(chip, ocv_raw, &ocv_uv);
	if (rc || ocv_uv == 0) {
		rc = adc_based_ocv(chip, &ocv_uv);
		if (rc) {
			pr_err("failed to read adc based ocv_uv rc = %d\n", rc);
			ocv_uv = DEFAULT_OCV_MICROVOLTS;
		}
		last_ocv_uv = ocv_uv;
	}
	pr_debug("ocv_uv = %d last_ocv_uv = %d\n", ocv_uv, last_ocv_uv);
}

static int64_t read_battery_id(struct pm8921_bms_chip *chip)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(chip->batt_id_channel, &result);
	if (rc) {
		pr_err("error reading batt id channel = %d, rc = %d\n",
					chip->vbat_channel, rc);
		return rc;
	}
	pr_debug("batt_id phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	return result.physical;
}

#define PALLADIUM_ID_MIN  2500
#define PALLADIUM_ID_MAX  4000
static int set_battery_data(struct pm8921_bms_chip *chip)
{
	int64_t battery_id;
#ifdef CONFIG_PM8921_EXTENDED_INFO
	int64_t batt_valid;
	struct pm8921_bms_battery_data batt_data;
	struct pm8921_bms_platform_data *pdata = chip->dev->platform_data;
#endif

	battery_id = read_battery_id(chip);

#ifdef CONFIG_PM8921_EXTENDED_INFO
	if (pdata->get_batt_info) {
		batt_valid = pdata->get_batt_info(battery_id, &batt_data);
		chip->fcc = batt_data.fcc;
		chip->fcc_temp_lut = batt_data.fcc_temp_lut;
		chip->fcc_sf_lut = batt_data.fcc_sf_lut;
		chip->pc_temp_ocv_lut = batt_data.pc_temp_ocv_lut;
		chip->pc_sf_lut = batt_data.pc_sf_lut;
		chip->init_rbatt = batt_data.rbatt;
		chip->k_factor = batt_data.k_factor;
		return 0;
	}
#endif
	if (battery_id < 0) {
		pr_err("cannot read battery id err = %lld\n", battery_id);
		return battery_id;
	}

	if (is_between(PALLADIUM_ID_MIN, PALLADIUM_ID_MAX, battery_id)) {
		chip->fcc = palladium_1500_data.fcc;
		chip->fcc_temp_lut = palladium_1500_data.fcc_temp_lut;
		chip->fcc_sf_lut = palladium_1500_data.fcc_sf_lut;
		chip->pc_temp_ocv_lut = palladium_1500_data.pc_temp_ocv_lut;
		chip->pc_sf_lut = palladium_1500_data.pc_sf_lut;
		return 0;
	} else {
		pr_warn("invalid battery id, palladium 1500 assumed\n");
		chip->fcc = palladium_1500_data.fcc;
		chip->fcc_temp_lut = palladium_1500_data.fcc_temp_lut;
		chip->fcc_sf_lut = palladium_1500_data.fcc_sf_lut;
		chip->pc_temp_ocv_lut = palladium_1500_data.pc_temp_ocv_lut;
		chip->pc_sf_lut = palladium_1500_data.pc_sf_lut;
		return 0;
	}
}

enum bms_request_operation {
	CALC_RBATT,
	CALC_FCC,
	CALC_PC,
	CALC_SOC,
	CALIB_HKADC,
	CALIB_CCADC,
	GET_VBAT_VSENSE_SIMULTANEOUS,
};

static int test_batt_temp = 5;
static int test_chargecycle = 150;
static int test_ocv = 3900000;
enum {
	TEST_BATT_TEMP,
	TEST_CHARGE_CYCLE,
	TEST_OCV,
};
static int get_test_param(void *data, u64 * val)
{
	switch ((int)data) {
	case TEST_BATT_TEMP:
		*val = test_batt_temp;
		break;
	case TEST_CHARGE_CYCLE:
		*val = test_chargecycle;
		break;
	case TEST_OCV:
		*val = test_ocv;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static int set_test_param(void *data, u64  val)
{
	switch ((int)data) {
	case TEST_BATT_TEMP:
		test_batt_temp = (int)val;
		break;
	case TEST_CHARGE_CYCLE:
		test_chargecycle = (int)val;
		break;
	case TEST_OCV:
		test_ocv = (int)val;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(temp_fops, get_test_param, set_test_param, "%llu\n");

static int get_calc(void *data, u64 * val)
{
	int param = (int)data;
	int ret = 0;
	int ibat_ua, vbat_uv;
	struct pm8921_soc_params raw;
	struct pm8921_rbatt_params rraw;

	read_soc_params_raw(the_chip, &raw);
	read_rbatt_params_raw(the_chip, &rraw);

	*val = 0;

	/* global irq number passed in via data */
	switch (param) {
	case CALC_RBATT:
		*val = calculate_rbatt_resume(the_chip, &rraw);
		break;
	case CALC_FCC:
		*val = calculate_fcc_uah(the_chip, test_batt_temp,
							test_chargecycle);
		break;
	case CALC_PC:
		*val = calculate_pc(the_chip, test_ocv, test_batt_temp,
							test_chargecycle);
		break;
	case CALC_SOC:
		*val = calculate_state_of_charge(the_chip, &raw,
					test_batt_temp, test_chargecycle);
		break;
	case CALIB_HKADC:
		/* reading this will trigger calibration */
		*val = 0;
		calib_hkadc(the_chip);
		break;
	case CALIB_CCADC:
		/* reading this will trigger calibration */
		*val = 0;
		pm8xxx_calib_ccadc();
		break;
	case GET_VBAT_VSENSE_SIMULTANEOUS:
		/* reading this will call simultaneous vbat and vsense */
		*val =
		pm8921_bms_get_simultaneous_battery_voltage_and_current(
			&ibat_ua,
			&vbat_uv);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(calc_fops, get_calc, NULL, "%llu\n");

static int get_reading(void *data, u64 * val)
{
	int param = (int)data;
	int ret = 0;
	struct pm8921_soc_params raw;
	struct pm8921_rbatt_params rraw;

	read_soc_params_raw(the_chip, &raw);
	read_rbatt_params_raw(the_chip, &rraw);

	*val = 0;

	switch (param) {
	case CC_MSB:
	case CC_LSB:
		*val = raw.cc;
		break;
	case LAST_GOOD_OCV_VALUE:
		*val = raw.last_good_ocv_uv;
		break;
	case VBATT_FOR_RBATT:
		*val = rraw.vbatt_for_rbatt_uv;
		break;
	case VSENSE_FOR_RBATT:
		*val = rraw.vsense_for_rbatt_uv;
		break;
	case OCV_FOR_RBATT:
		*val = rraw.ocv_for_rbatt_uv;
		break;
	case VSENSE_AVG:
		read_vsense_avg(the_chip, (uint *)val);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(reading_fops, get_reading, NULL, "%lld\n");

static int get_rt_status(void *data, u64 * val)
{
	int i = (int)data;
	int ret;

	/* global irq number passed in via data */
	ret = pm_bms_get_rt_status(the_chip, i);
	*val = ret;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rt_fops, get_rt_status, NULL, "%llu\n");

static int get_reg(void *data, u64 * val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	ret = pm8xxx_readb(the_chip->dev->parent, addr, &temp);
	if (ret) {
		pr_err("pm8xxx_readb to %x value = %d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	temp = (u8) val;
	ret = pm8xxx_writeb(the_chip->dev->parent, addr, temp);
	if (ret) {
		pr_err("pm8xxx_writeb to %x value = %d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

static void create_debugfs_entries(struct pm8921_bms_chip *chip)
{
	int i;

	chip->dent = debugfs_create_dir("pm8921-bms", NULL);

	if (IS_ERR(chip->dent)) {
		pr_err("pmic bms couldnt create debugfs dir\n");
		return;
	}

	debugfs_create_file("BMS_CONTROL", 0644, chip->dent,
			(void *)BMS_CONTROL, &reg_fops);
	debugfs_create_file("BMS_OUTPUT0", 0644, chip->dent,
			(void *)BMS_OUTPUT0, &reg_fops);
	debugfs_create_file("BMS_OUTPUT1", 0644, chip->dent,
			(void *)BMS_OUTPUT1, &reg_fops);
	debugfs_create_file("BMS_TEST1", 0644, chip->dent,
			(void *)BMS_TEST1, &reg_fops);

	debugfs_create_file("test_batt_temp", 0644, chip->dent,
				(void *)TEST_BATT_TEMP, &temp_fops);
	debugfs_create_file("test_chargecycle", 0644, chip->dent,
				(void *)TEST_CHARGE_CYCLE, &temp_fops);
	debugfs_create_file("test_ocv", 0644, chip->dent,
				(void *)TEST_OCV, &temp_fops);

	debugfs_create_file("read_cc", 0644, chip->dent,
				(void *)CC_MSB, &reading_fops);
	debugfs_create_file("read_last_good_ocv", 0644, chip->dent,
				(void *)LAST_GOOD_OCV_VALUE, &reading_fops);
	debugfs_create_file("read_vbatt_for_rbatt", 0644, chip->dent,
				(void *)VBATT_FOR_RBATT, &reading_fops);
	debugfs_create_file("read_vsense_for_rbatt", 0644, chip->dent,
				(void *)VSENSE_FOR_RBATT, &reading_fops);
	debugfs_create_file("read_ocv_for_rbatt", 0644, chip->dent,
				(void *)OCV_FOR_RBATT, &reading_fops);
	debugfs_create_file("read_vsense_avg", 0644, chip->dent,
				(void *)VSENSE_AVG, &reading_fops);

	debugfs_create_file("show_rbatt", 0644, chip->dent,
				(void *)CALC_RBATT, &calc_fops);
	debugfs_create_file("show_fcc", 0644, chip->dent,
				(void *)CALC_FCC, &calc_fops);
	debugfs_create_file("show_pc", 0644, chip->dent,
				(void *)CALC_PC, &calc_fops);
	debugfs_create_file("show_soc", 0644, chip->dent,
				(void *)CALC_SOC, &calc_fops);
	debugfs_create_file("calib_hkadc", 0644, chip->dent,
				(void *)CALIB_HKADC, &calc_fops);
	debugfs_create_file("calib_ccadc", 0644, chip->dent,
				(void *)CALIB_CCADC, &calc_fops);

	debugfs_create_file("simultaneous", 0644, chip->dent,
			(void *)GET_VBAT_VSENSE_SIMULTANEOUS, &calc_fops);

	for (i = 0; i < ARRAY_SIZE(bms_irq_data); i++) {
		if (chip->pmic_bms_irq[bms_irq_data[i].irq_id])
			debugfs_create_file(bms_irq_data[i].name, 0444,
				chip->dent,
				(void *)bms_irq_data[i].irq_id,
				&rt_fops);
	}
}

#define REG_SBI_CONFIG		0x04F
#define PAGE3_ENABLE_MASK	0x6
#define PROGRAM_REV_MASK	0x0F
#define PROGRAM_REV		0x9
static int read_ocv_trim(struct pm8921_bms_chip *chip)
{
	int rc;
	u8 reg, sbi_config;

	rc = pm8xxx_readb(chip->dev->parent, REG_SBI_CONFIG, &sbi_config);
	if (rc) {
		pr_err("error = %d reading sbi config reg\n", rc);
		return rc;
	}

	reg = sbi_config | PAGE3_ENABLE_MASK;
	rc = pm8xxx_writeb(chip->dev->parent, REG_SBI_CONFIG, reg);
	if (rc) {
		pr_err("error = %d writing sbi config reg\n", rc);
		return rc;
	}

	rc = pm8xxx_readb(chip->dev->parent, TEST_PROGRAM_REV, &reg);
	if (rc)
		pr_err("Error %d reading %d addr %d\n",
			rc, reg, TEST_PROGRAM_REV);
	pr_err("program rev reg is 0x%x\n", reg);
	reg &= PROGRAM_REV_MASK;

	/* If the revision is equal or higher do not adjust trim delta */
	if (reg >= PROGRAM_REV) {
		chip->amux_2_trim_delta = 0;
		goto restore_sbi_config;
	}

	rc = pm8xxx_readb(chip->dev->parent, AMUX_TRIM_2, &reg);
	if (rc) {
		pr_err("error = %d reading trim reg\n", rc);
		return rc;
	}

	pr_err("trim reg is 0x%x\n", reg);
	chip->amux_2_trim_delta = abs(0x49 - reg);
	pr_err("trim delta is %d\n", chip->amux_2_trim_delta);

restore_sbi_config:
	rc = pm8xxx_writeb(chip->dev->parent, REG_SBI_CONFIG, sbi_config);
	if (rc) {
		pr_err("error = %d writing sbi config reg\n", rc);
		return rc;
	}

	return 0;
}

#ifdef CONFIG_PM8921_TEST_OVERRIDE
struct pm8921_override_user_data {
	int is_charging;
	int soc;
	int cc_uah;
	int real_fcc_batt_temp;
	int real_fcc;
	int ocv;
	int rbatt;
};

int pm8921_override_get_charge_status(int *status)
{
	if (!the_chip->user_override)
		return 0;

	*status = (the_chip->user_override_is_chg == 0) ?
		POWER_SUPPLY_STATUS_DISCHARGING : POWER_SUPPLY_STATUS_CHARGING;
	return 1;
}
EXPORT_SYMBOL(pm8921_override_get_charge_status);

static ssize_t pm8921_override_write(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct platform_device *pdev =
		to_platform_device(container_of(kobj, struct device, kobj));
	struct pm8921_bms_chip *chip = platform_get_drvdata(pdev);
	struct pm8921_override_user_data usr_data;

	if (off != 0)
		return -EIO;

	if (count != sizeof(usr_data))
		return -EIO;

	memcpy(&usr_data, buf, sizeof(usr_data));

	if (usr_data.soc != -EINVAL)
		last_soc = usr_data.soc;

	if (usr_data.real_fcc_batt_temp != -EINVAL)
		last_real_fcc_batt_temp = usr_data.real_fcc_batt_temp;

	if (usr_data.rbatt != -EINVAL)
		last_rbatt = usr_data.rbatt;

	if (usr_data.real_fcc != -EINVAL)
		last_real_fcc_mah = usr_data.real_fcc;

	if (usr_data.ocv != -EINVAL)
		last_ocv_uv = usr_data.ocv;

	if (!chip->user_override_is_chg && usr_data.is_charging) {
		chip->user_override_is_chg = 1;
		the_chip->start_percent = usr_data.soc;
		bms_start_percent = usr_data.soc;
#ifdef CONFIG_PM8921_EXTENDED_INFO
		bms_meter_offset = 0;
#endif
		bms_start_ocv_uv = last_ocv_uv;
		bms_start_cc_uah = usr_data.cc_uah;
	} else if (chip->user_override_is_chg && !usr_data.is_charging) {
		chip->user_override_is_chg = 0;
		the_chip->end_percent = usr_data.soc;
		bms_end_percent = usr_data.soc;
		bms_end_ocv_uv = last_ocv_uv;
		bms_end_cc_uah = usr_data.cc_uah;
		pm8921_bms_charging_end(0);
	}

	pm8921_override_force_battery_update();
	return count;
}

static struct bin_attribute pm8921_override_attr = {
	.attr = {
		.name = "override.bin",
		.mode = 0666,
	},
	.size = sizeof(struct pm8921_override_user_data),
	.write = pm8921_override_write
};
#endif

static int __devinit pm8921_bms_probe(struct platform_device *pdev)
{
	int rc = 0;
	int vbatt = 0;
	struct pm8921_bms_chip *chip;
	const struct pm8921_bms_platform_data *pdata
				= pdev->dev.platform_data;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct pm8921_bms_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("Cannot allocate pm_bms_chip\n");
		return -ENOMEM;
	}
	mutex_init(&chip->bms_output_lock);
	spin_lock_init(&chip->bms_100_lock);
	chip->dev = &pdev->dev;
	chip->r_sense = pdata->r_sense;
	chip->i_test = pdata->i_test;
	chip->v_failure = pdata->v_failure;
	chip->calib_delay_ms = pdata->calib_delay_ms;
	chip->max_voltage_uv = pdata->max_voltage_uv;
	chip->start_percent = -EINVAL;
	chip->end_percent = -EINVAL;
	rc = set_battery_data(chip);
	if (rc) {
		pr_err("%s bad battery data %d\n", __func__, rc);
		goto free_chip;
	}

	chip->batt_temp_channel = pdata->bms_cdata.batt_temp_channel;
	chip->vbat_channel = pdata->bms_cdata.vbat_channel;
	chip->ref625mv_channel = pdata->bms_cdata.ref625mv_channel;
	chip->ref1p25v_channel = pdata->bms_cdata.ref1p25v_channel;
	chip->batt_id_channel = pdata->bms_cdata.batt_id_channel;
	chip->revision = pm8xxx_get_revision(chip->dev->parent);
	INIT_WORK(&chip->calib_hkadc_work, calibrate_hkadc_work);

	rc = request_irqs(chip, pdev);
	if (rc) {
		pr_err("couldn't register interrupts rc = %d\n", rc);
		goto free_chip;
	}

	rc = pm8921_bms_hw_init(chip);
	if (rc) {
		pr_err("couldn't init hardware rc = %d\n", rc);
		goto free_irqs;
	}

#ifdef CONFIG_PM8921_TEST_OVERRIDE
	chip->user_override = 1;
	rc = sysfs_create_bin_file(&chip->dev->kobj,
				&pm8921_override_attr);
	if (rc) {
		pr_err("couldn't create override attribute rc = %d\n",
			rc);
		goto free_irqs;
	}
#endif
#ifdef CONFIG_PM8921_EXTENDED_INFO
	chip->meter_offset = 0;
#endif
	read_shutdown_soc(chip);

	platform_set_drvdata(pdev, chip);
	the_chip = chip;
	create_debugfs_entries(chip);

	rc = read_ocv_trim(chip);
	if (rc) {
		pr_err("couldn't adjust ocv_trim rc= %d\n", rc);
		goto free_irqs;
	}
	check_initial_ocv(chip);

	INIT_DELAYED_WORK(&chip->calib_ccadc_work, calibrate_ccadc_work);
	/* begin calibration only on chips > 2.0 */
	if (chip->revision >= PM8XXX_REVISION_8921_2p0)
		schedule_delayed_work(&chip->calib_ccadc_work, 0);

	/* initial hkadc calibration */
	schedule_work(&chip->calib_hkadc_work);
	/* enable the vbatt reading interrupts for scheduling hkadc calib */
	pm8921_bms_enable_irq(chip, PM8921_BMS_GOOD_OCV);
	pm8921_bms_enable_irq(chip, PM8921_BMS_OCV_FOR_R);

	get_battery_uvolts(chip, &vbatt);
	pr_info("OK battery_capacity_at_boot=%d volt = %d ocv = %d\n",
				pm8921_bms_get_percent_charge(),
				vbatt, last_ocv_uv);

	INIT_DELAYED_WORK(&chip->shutdown_soc_work, shutdown_soc_work);
	schedule_delayed_work(&chip->shutdown_soc_work,
			round_jiffies_relative(msecs_to_jiffies
			(SHOW_SHUTDOWN_SOC_MS)));

	return 0;

free_irqs:
	free_irqs(chip);
free_chip:
	kfree(chip);
	return rc;
}

static int __devexit pm8921_bms_remove(struct platform_device *pdev)
{
	struct pm8921_bms_chip *chip = platform_get_drvdata(pdev);

#ifdef CONFIG_PM8921_TEST_OVERRIDE
	sysfs_remove_bin_file(&chip->dev->kobj, &pm8921_override_attr);
#endif
	free_irqs(chip);
	kfree(chip->adjusted_fcc_temp_lut);
	platform_set_drvdata(pdev, NULL);
	the_chip = NULL;
	kfree(chip);
	return 0;
}

static struct platform_driver pm8921_bms_driver = {
	.probe	= pm8921_bms_probe,
	.remove	= __devexit_p(pm8921_bms_remove),
	.driver	= {
		.name	= PM8921_BMS_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &pm8921_pm_ops
	},
};

static int __init pm8921_bms_init(void)
{
	return platform_driver_register(&pm8921_bms_driver);
}

static void __exit pm8921_bms_exit(void)
{
	platform_driver_unregister(&pm8921_bms_driver);
}

late_initcall(pm8921_bms_init);
module_exit(pm8921_bms_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8921 bms driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8921_BMS_DEV_NAME);
