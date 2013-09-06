/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/i2c/sx150x.h>
#include <linux/i2c/isl9519.h>
#include <linux/gpio.h>
#include <linux/msm_ssbi.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/slimbus/slimbus.h>
#include <linux/bootmem.h>
#include <mach/kgsl.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <linux/cyttsp.h>
#include <linux/dma-mapping.h>
#include <linux/platform_data/qcom_crypto_device.h>
#include <linux/platform_data/qcom_wcnss_device.h>
#include <linux/leds.h>
#include <linux/leds-pm8xxx.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/msm_tsens.h>
#include <linux/ks8851.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/hardware/gic.h>
#include <asm/mach/mmc.h>
#include <asm/bootinfo.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_spi.h>
#ifdef CONFIG_USB_MSM_OTG_72K
#include <mach/msm_hsusb.h>
#else
#include <linux/usb/msm_hsusb.h>
#endif
#include <linux/usb/android.h>
#include <mach/usbdiag.h>
#include <mach/socinfo.h>
#include <mach/rpm.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/dma.h>
#include <mach/msm_dsps.h>
#include <mach/msm_xo.h>
#include <mach/restart.h>

#ifdef CONFIG_WCD9310_CODEC
#include <linux/slimbus/slimbus.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#endif

#include <linux/ion.h>
#include <mach/ion.h>

#ifdef CONFIG_MACH_MSM8960_MMI
#include <linux/power/mmi-battery.h>
#include "board-mmi.h"
#endif

extern void mmi_buzz_blip(void);
extern void mmi_sw_ap_reset(void);

#include "timer.h"
#include "devices.h"
#include "devices-msm8x60.h"
#include "spm.h"
#include "board-8960.h"
#include "pm.h"
#include <mach/cpuidle.h>
#include "rpm_resources.h"
#include "mpm.h"
#include "acpuclock.h"
#include "rpm_log.h"
#include "smd_private.h"
#include "pm-boot.h"

static struct pm8xxx_irq_platform_data pm8xxx_irq_pdata __devinitdata = {
	.irq_base	= PM8921_IRQ_BASE,
	.devirq		= MSM_GPIO_TO_INT(104),
	.irq_trigger_flag	= IRQF_TRIGGER_LOW,
};

static struct pm8xxx_gpio_platform_data pm8xxx_gpio_pdata __devinitdata = {
	.gpio_base	= PM8921_GPIO_PM_TO_SYS(1),
};

static struct pm8xxx_mpp_platform_data pm8xxx_mpp_pdata __devinitdata = {
	.mpp_base	= PM8921_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_rtc_platform_data pm8xxx_rtc_pdata __devinitdata = {
#ifdef CONFIG_MMI_ENABLE_RTC
	.rtc_write_enable       = true,
#else
	.rtc_write_enable       = false,
#endif
	.rtc_alarm_powerup	= false,
};

static struct pm8xxx_pwrkey_platform_data pm8xxx_pwrkey_pdata = {
	.pull_up		= 1,
	.kpd_trigger_delay_us	= 15625,
	.wakeup			= 1,
	.buzz			= mmi_buzz_blip,
	.reboot			= mmi_sw_ap_reset,
};

static int pm8921_therm_mitigation[] = {
	1100,
	700,
	600,
	325,
};

static const struct pm8xxx_adc_map_pt adcmap_batt_therm[] = {
	{-400,	1512},
	{-390,	1509},
	{-380,	1506},
	{-370,	1503},
	{-360,	1499},
	{-350,	1494},
	{-340,	1491},
	{-330,	1487},
	{-320,	1482},
	{-310,	1477},
	{-300,	1471},
	{-290,	1467},
	{-280,	1462},
	{-270,	1456},
	{-260,	1450},
	{-250,	1443},
	{-240,	1437},
	{-230,	1431},
	{-220,	1424},
	{-210,	1416},
	{-200,	1407},
	{-190,	1400},
	{-180,	1393},
	{-170,	1384},
	{-160,	1375},
	{-150,	1365},
	{-140,	1356},
	{-130,	1347},
	{-120,	1337},
	{-110,	1327},
	{-100,	1315},
	{-90,	1305},
	{-80,	1295},
	{-70,	1283},
	{-60,	1271},
	{-50,	1258},
	{-40,	1247},
	{-30,	1235},
	{-20,	1223},
	{-10,	1209},
	{-0,	1194},
	{10,	1182},
	{20,	1170},
	{30,	1156},
	{40,	1141},
	{50,	1126},
	{60,	1113},
	{70,	1100},
	{80,	1085},
	{90,	1070},
	{100,	1054},
	{110,	1041},
	{120,	1027},
	{130,	1013},
	{140,	997},
	{150,	981},
	{160,	968},
	{170,	954},
	{180,	940},
	{190,	924},
	{200,	909},
	{210,	896},
	{220,	882},
	{230,	868},
	{240,	854},
	{250,	839},
	{260,	826},
	{270,	813},
	{280,	800},
	{290,	787},
	{300,	772},
	{310,	761},
	{320,	749},
	{330,	737},
	{340,	724},
	{350,	711},
	{360,	701},
	{370,	690},
	{380,	679},
	{390,	668},
	{400,	656},
	{410,	646},
	{420,	637},
	{430,	627},
	{440,	617},
	{450,	606},
	{460,	598},
	{470,	589},
	{480,	580},
	{490,	571},
	{500,	562},
	{510,	555},
	{520,	547},
	{530,	540},
	{540,	532},
	{550,	524},
	{560,	517},
	{570,	511},
	{580,	504},
	{590,	497},
	{600,	491},
	{610,	485},
	{620,	479},
	{630,	473},
	{640,	468},
	{650,	462},
	{660,	457},
	{670,	452},
	{680,	447},
	{690,	442},
	{700,	437},
	{710,	433},
	{720,	429},
	{730,	424},
	{740,	420},
	{750,	416},
	{760,	412},
	{770,	409},
	{780,	405},
	{790,	402},
	{800,	398},
	{810,	395},
	{820,	392},
	{830,	389},
	{840,	386},
	{850,	382},
	{860,	380},
	{870,	377},
	{880,	375},
	{890,	372},
	{900,	369},
	{950,	358},
	{1000,	348},
	{1050,	340},
	{1100,	333},
	{1150,	327},
	{1200,	322},
	{1250,	317}
};

static struct pm8xxx_adc_scale_tbl adc_scale_tbls[ADC_SCALE_NONE] = {
	[ADC_SCALE_BATT_THERM] = {
		.scale_lu_tbl = adcmap_batt_therm,
		.scale_lu_tbl_size = ARRAY_SIZE(adcmap_batt_therm),
	},
};

#ifdef CONFIG_MACH_MSM8960_MMI
static struct regulator *therm_reg;
static int therm_reg_enable;

void pm8921_chg_force_therm_bias(struct device *dev, int enable)
{
	int rc = 0;

	if (!therm_reg) {
		therm_reg = regulator_get(dev, "8921_l14");
		therm_reg_enable = 0;
	}

	if (enable && !therm_reg_enable) {
		if (!IS_ERR_OR_NULL(therm_reg)) {
			rc = regulator_set_voltage(therm_reg, 1800000, 1800000);
			if (!rc)
				rc = regulator_enable(therm_reg);
			if (rc)
				pr_err("L14 failed to set voltage 8921_l14\n");
			else
				therm_reg_enable = 1;
		}
	} else if(!enable && therm_reg_enable) {
		if (!IS_ERR_OR_NULL(therm_reg)) {
			rc = regulator_disable(therm_reg);
			if (rc)
				pr_err("L14 unable to disable 8921_l14\n");
			else
				therm_reg_enable = 0;
		}
	}
}

static int battery_timeout;

int find_mmi_battery(struct mmi_battery_cell *cell_info)
{
	int i;

	if (cell_info->cell_id != OLD_EPROM_CELL_ID) {
		/* Search Based off Cell ID */
		for (i = (MMI_BATTERY_DEFAULT + 1); i < MMI_BATTERY_NUM; i++) {
			if (cell_info->cell_id ==
			    mmi_batts.cell_list[i]->cell_id) {
				pr_info("Battery Cell ID Found: %d\n", i);
				return i;
			}
		}
	}

	/* Search based off Typical Values */
	for (i = (MMI_BATTERY_DEFAULT + 1); i < MMI_BATTERY_NUM; i++) {
		if (cell_info->capacity == mmi_batts.cell_list[i]->capacity) {
			if (cell_info->peak_voltage ==
			    mmi_batts.cell_list[i]->peak_voltage) {
				if (cell_info->dc_impedance ==
				   mmi_batts.cell_list[i]->dc_impedance) {
					pr_info("Battery Match Found: %d\n", i);
					return i;
				}
			}
		}
	}

	pr_err("Battery Match Not Found!\n");
	return MMI_BATTERY_DEFAULT;
}

int64_t read_mmi_battery_chrg(int64_t battery_id,
			      struct pm8921_charger_battery_data *data)
{
	struct mmi_battery_cell *cell_info;
	int i = 0;
	int ret;
	size_t len = sizeof(struct pm8921_charger_battery_data);

	for (i = 0; i < 50; i++) {
		if (battery_timeout)
			break;
		cell_info = mmi_battery_get_info();
		if (cell_info) {
			if (cell_info->batt_valid != MMI_BATTERY_UNKNOWN) {
				ret = find_mmi_battery(cell_info);
				memcpy(data, mmi_batts.chrg_list[ret], len);
				return (int64_t) cell_info->batt_valid;
			}
		}
		msleep(500);
	}

	battery_timeout = 1;
	memcpy(data, mmi_batts.chrg_list[MMI_BATTERY_DEFAULT], len);
	return 0;
}

int64_t read_mmi_battery_bms(int64_t battery_id,
			     struct pm8921_bms_battery_data *data)
{
	struct mmi_battery_cell *cell_info;
	int i = 0;
	int ret;
	size_t len = sizeof(struct pm8921_bms_battery_data);

	for (i = 0; i < 50; i++) {
		if (battery_timeout)
			break;
		cell_info = mmi_battery_get_info();
		if (cell_info) {
			if (cell_info->batt_valid != MMI_BATTERY_UNKNOWN) {
				ret = find_mmi_battery(cell_info);
				memcpy(data, mmi_batts.bms_list[ret], len);
				return (int64_t) cell_info->batt_valid;
			}
		}
		msleep(500);
	}

	battery_timeout = 1;
	memcpy(data, mmi_batts.bms_list[MMI_BATTERY_DEFAULT], len);
	return 0;
}

static enum pm8921_btm_state btm_state;

#define TEMP_HYSTERISIS_DEGC 2
#define TEMP_OVERSHOOT 10
#define TEMP_HOT 60
#define TEMP_COLD -20
static int64_t temp_range_check(int batt_temp, int batt_mvolt,
				struct pm8921_charger_battery_data *data,
				int64_t *enable, enum pm8921_btm_state *state)
{
	int64_t chrg_enable = 0;
	int64_t btm_change = 0;

	if (!enable || !data)
		return btm_change;

	/* Check for a State Change */
	if (btm_state == BTM_NORM) {
		if (batt_temp >= (signed int)(data->warm_temp)) {
			data->cool_temp =
				data->warm_temp - TEMP_HYSTERISIS_DEGC;
			data->warm_temp = TEMP_HOT - data->hot_temp_offset;
			if (batt_mvolt >= data->warm_bat_voltage) {
				btm_state = BTM_WARM_HV;
				chrg_enable = 0;
			} else {
				btm_state = BTM_WARM_LV;
				data->max_voltage = data->warm_bat_voltage;
				chrg_enable = 1;
			}
			btm_change = 1;
		} else if (batt_temp <= (signed int)(data->cool_temp)) {
			data->warm_temp =
				data->cool_temp + TEMP_HYSTERISIS_DEGC;
			data->cool_temp = TEMP_COLD;
			if (batt_mvolt >= data->cool_bat_voltage) {
				btm_state = BTM_COOL_HV;
				chrg_enable = 0;
			} else {
				btm_state = BTM_COOL_LV;
				data->max_voltage = data->cool_bat_voltage;
				chrg_enable = 1;
			}
			btm_change = 1;
		}
	} else if (btm_state == BTM_COLD) {
		if (batt_temp >= TEMP_COLD + TEMP_HYSTERISIS_DEGC) {
			data->warm_temp =
				data->cool_temp + TEMP_HYSTERISIS_DEGC;
			data->cool_temp = TEMP_COLD;
			if (batt_mvolt >= data->cool_bat_voltage) {
				btm_state = BTM_COOL_HV;
				chrg_enable = 0;
			} else {
				btm_state = BTM_COOL_LV;
				data->max_voltage = data->cool_bat_voltage;
				chrg_enable = 1;
			}
			btm_change = 1;
		}
	} else if (btm_state == BTM_COOL_LV) {
		if (batt_temp <= TEMP_COLD) {
			btm_state = BTM_COLD;
			data->cool_temp = TEMP_COLD - TEMP_OVERSHOOT;
			data->warm_temp = TEMP_COLD + TEMP_HYSTERISIS_DEGC;
			chrg_enable = 0;
			btm_change = 1;
		} else if (batt_temp >=
			   ((signed int)(data->cool_temp) +
			    TEMP_HYSTERISIS_DEGC)) {
			btm_state = BTM_NORM;
			chrg_enable = 1;
			btm_change = 1;
		} else if (batt_mvolt >= data->cool_bat_voltage) {
			btm_state = BTM_COOL_HV;
			data->warm_temp =
				data->cool_temp + TEMP_HYSTERISIS_DEGC;
			data->cool_temp = TEMP_COLD;
			data->max_voltage = data->cool_bat_voltage;
			chrg_enable = 0;
			btm_change = 1;
		}
	} else if (btm_state == BTM_COOL_HV) {
		if (batt_temp <= TEMP_COLD) {
			btm_state = BTM_COLD;
			data->cool_temp = TEMP_COLD - TEMP_OVERSHOOT;
			data->warm_temp = TEMP_COLD + TEMP_HYSTERISIS_DEGC;
			chrg_enable = 0;
			btm_change = 1;
		} else if (batt_temp >=
			   ((signed int)(data->cool_temp) +
			    TEMP_HYSTERISIS_DEGC)) {
			btm_state = BTM_NORM;
			chrg_enable = 1;
			btm_change = 1;
		} else if (batt_mvolt < data->cool_bat_voltage) {
			btm_state = BTM_COOL_LV;
			data->warm_temp =
				data->cool_temp + TEMP_HYSTERISIS_DEGC;
			data->cool_temp = TEMP_COLD;
			data->max_voltage = data->cool_bat_voltage;
			chrg_enable = 1;
			btm_change = 1;
		}
	} else if (btm_state == BTM_WARM_LV) {
		if (batt_temp >= TEMP_HOT) {
			btm_state = BTM_HOT;
			data->cool_temp = TEMP_HOT - TEMP_HYSTERISIS_DEGC
				- data->hot_temp_offset;
			data->warm_temp = TEMP_HOT + TEMP_OVERSHOOT
				- data->hot_temp_offset;
			chrg_enable = 0;
			btm_change = 1;
		} else if (batt_temp <=
			   ((signed int)(data->warm_temp) -
			    TEMP_HYSTERISIS_DEGC)) {
			btm_state = BTM_NORM;
			chrg_enable = 1;
			btm_change = 1;
		} else if (batt_mvolt >= data->warm_bat_voltage) {
			btm_state = BTM_WARM_HV;
			data->cool_temp =
				data->warm_temp - TEMP_HYSTERISIS_DEGC;
			data->warm_temp = TEMP_HOT
				- data->hot_temp_offset;
			data->max_voltage = data->warm_bat_voltage;
			chrg_enable = 0;
			btm_change = 1;
		}
	} else if (btm_state == BTM_WARM_HV) {
		if (batt_temp >= TEMP_HOT) {
			btm_state = BTM_HOT;
			data->cool_temp = TEMP_HOT - TEMP_HYSTERISIS_DEGC
				- data->hot_temp_offset;
			data->warm_temp = TEMP_HOT + TEMP_OVERSHOOT
				- data->hot_temp_offset;
			chrg_enable = 0;
			btm_change = 1;
		} else if (batt_temp <=
			   ((signed int)(data->warm_temp) -
			    TEMP_HYSTERISIS_DEGC)) {
			btm_state = BTM_NORM;
			chrg_enable = 1;
			btm_change = 1;
		} else if (batt_mvolt < data->warm_bat_voltage) {
			btm_state = BTM_WARM_LV;
			data->cool_temp =
				data->warm_temp - TEMP_HYSTERISIS_DEGC;
			data->warm_temp = TEMP_HOT
				- data->hot_temp_offset;
			data->max_voltage = data->warm_bat_voltage;
			chrg_enable = 1;
			btm_change = 1;
		}
	} else if (btm_state == BTM_HOT) {
		if (batt_temp <= TEMP_HOT - TEMP_HYSTERISIS_DEGC) {
			data->cool_temp =
				data->warm_temp - TEMP_HYSTERISIS_DEGC;
			data->warm_temp = TEMP_HOT
				- data->hot_temp_offset;
			if (batt_mvolt >= data->warm_bat_voltage) {
				btm_state = BTM_WARM_HV;
				chrg_enable = 0;
			} else {
				btm_state = BTM_WARM_LV;
				data->max_voltage = data->warm_bat_voltage;
				chrg_enable = 1;
			}
			btm_change = 1;
		}
	}

	*enable = chrg_enable;
	*state = btm_state;

	return btm_change;
}

#define MAX_VOLTAGE_MV		4350
static struct pm8921_charger_platform_data pm8921_chg_pdata __devinitdata = {
	.safety_time		= 512,
	.update_time		= 60000,
	.ttrkl_time		= 64,
	.max_voltage		= MAX_VOLTAGE_MV,
	.min_voltage		= 3200,
	.resume_voltage_delta	= 100,
	.term_current		= 100,
	.cool_temp		= 0,
	.warm_temp		= 45,
	.temp_check_period	= 1,
	.max_bat_chg_current	= 2000,
	.cool_bat_chg_current	= 1500,
	.warm_bat_chg_current	= 1500,
	.cool_bat_voltage	= 3800,
	.warm_bat_voltage	= 3800,
	.batt_id_min            = 0,
	.batt_id_max            = 1,
	.thermal_mitigation	= pm8921_therm_mitigation,
	.thermal_levels		= ARRAY_SIZE(pm8921_therm_mitigation),
	.cold_thr               = PM_SMBC_BATT_TEMP_COLD_THR__HIGH,
	.hot_thr                = PM_SMBC_BATT_TEMP_HOT_THR__LOW,
#ifdef CONFIG_PM8921_EXTENDED_INFO
	.get_batt_info          = read_mmi_battery_chrg,
	.temp_range_cb          = temp_range_check,
	.batt_alarm_delta	= 100,
	.lower_battery_threshold = 3400,
	.force_therm_bias	= pm8921_chg_force_therm_bias,
#endif
};
#else
static struct pm8921_charger_platform_data pm8921_chg_pdata __devinitdata = {
	.safety_time		= 180,
	.update_time		= 60000,
	.max_voltage		= 4200,
	.min_voltage		= 3200,
	.resume_voltage_delta	= 100,
	.term_current		= 100,
	.cool_temp		= 10,
	.warm_temp		= 40,
	.temp_check_period	= 1,
	.max_bat_chg_current	= 1100,
	.cool_bat_chg_current	= 350,
	.warm_bat_chg_current	= 350,
	.cool_bat_voltage	= 4100,
	.warm_bat_voltage	= 4100,
	.thermal_mitigation	= pm8921_therm_mitigation,
	.thermal_levels		= ARRAY_SIZE(pm8921_therm_mitigation),
};
#endif

static struct pm8xxx_misc_platform_data pm8xxx_misc_pdata = {
	.priority		= 0,
};

#ifdef CONFIG_MACH_MSM8960_MMI
static struct pm8921_bms_platform_data pm8921_bms_pdata __devinitdata = {
	.r_sense		= 10,
	.i_test			= 0,
	.v_failure		= 3200,
	.calib_delay_ms		= 600000,
	.max_voltage_uv		= MAX_VOLTAGE_MV * 1000,
#ifdef CONFIG_PM8921_EXTENDED_INFO
	.get_batt_info          = read_mmi_battery_bms,
#endif
};
#else
static struct pm8921_bms_platform_data pm8921_bms_pdata __devinitdata = {
	.r_sense		= 10,
	.i_test			= 2500,
	.v_failure		= 3000,
	.calib_delay_ms		= 600000,
	.max_voltage_uv		= MAX_VOLTAGE_MV * 1000,
};
#endif

#define	PM8921_LC_LED_MAX_CURRENT	4	/* I = 4mA */
#define PM8XXX_LED_PWM_PERIOD		1000
#define PM8XXX_LED_PWM_DUTY_MS		20
/**
 * PM8XXX_PWM_CHANNEL_NONE shall be used when LED shall not be
 * driven using PWM feature.
 */
#define PM8XXX_PWM_CHANNEL_NONE		-1

static struct led_info pm8921_led_info[] = {
	[0] = {
		.name			= "charging",
	},
	[1] = {
		.name			= "reserved",
	},
};

static struct led_platform_data pm8921_led_core_pdata = {
	.num_leds = ARRAY_SIZE(pm8921_led_info),
	.leds = pm8921_led_info,
};

static int pm8921_led0_pwm_duty_pcts[56] = {
		1, 4, 8, 12, 16, 20, 24, 28, 32, 36,
		40, 44, 46, 52, 56, 60, 64, 68, 72, 76,
		80, 84, 88, 92, 96, 100, 100, 100, 98, 95,
		92, 88, 84, 82, 78, 74, 70, 66, 62, 58,
		58, 54, 50, 48, 42, 38, 34, 30, 26, 22,
		14, 10, 6, 4, 1
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS,
	.start_idx = 0,
};

#ifdef CONFIG_MACH_MSM8960_MMI

#define ATC_LED_SRC 0x216
#define ATC_LED_SRC_MASK 0x30
void pm8xxx_atc_led_ctrl(struct device *dev, unsigned on)
{
	u8 val;

	if (on) {
		pm8xxx_readb(dev, ATC_LED_SRC, &val);
		val |= ATC_LED_SRC_MASK;
		pm8xxx_writeb(dev, ATC_LED_SRC, val);
	} else {
		pm8xxx_readb(dev, ATC_LED_SRC, &val);
		val &= ~ATC_LED_SRC_MASK;
		pm8xxx_writeb(dev, ATC_LED_SRC, val);
	}
}
#endif

static struct pm8xxx_led_config pm8921_led_configs[] = {
	[0] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_duty_cycles,
#ifdef CONFIG_MACH_MSM8960_MMI
		.led_ctrl = pm8xxx_atc_led_ctrl,
#endif
	},
	[1] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = 8,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
	},
};

static struct pm8xxx_led_platform_data pm8xxx_leds_pdata = {
		.led_core = &pm8921_led_core_pdata,
		.configs = pm8921_led_configs,
		.num_configs = ARRAY_SIZE(pm8921_led_configs),
};

static struct pm8xxx_ccadc_platform_data pm8xxx_ccadc_pdata = {
	.r_sense		= 10,
};

static struct pm8xxx_adc_amux pm8921_adc_channels_data[] = {
	{"vcoin", CHANNEL_VCOIN, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vbat", CHANNEL_VBAT, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"dcin", CHANNEL_DCIN, CHAN_PATH_SCALING4, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"ichg", CHANNEL_ICHG, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vph_pwr", CHANNEL_VPH_PWR, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"ibat", CHANNEL_IBAT, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"batt_therm", CHANNEL_BATT_THERM, CHAN_PATH_SCALING1, AMUX_RSV2,
		ADC_DECIMATION_TYPE2, ADC_SCALE_BATT_THERM},
	{"batt_id", CHANNEL_BATT_ID, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"usbin", CHANNEL_USBIN, CHAN_PATH_SCALING3, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"pmic_therm", CHANNEL_DIE_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PMIC_THERM},
	{"625mv", CHANNEL_625MV, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"125v", CHANNEL_125V, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"chg_temp", CHANNEL_CHG_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"pa_therm1", ADC_MPP_1_AMUX8, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PA_THERM},
	{"xo_therm", CHANNEL_MUXOFF, CHAN_PATH_SCALING1, AMUX_RSV0,
		ADC_DECIMATION_TYPE2, ADC_SCALE_XOTHERM},
	{"pa_therm0", ADC_MPP_1_AMUX3, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PA_THERM},
	{"mpp_2", CHANNEL_MPP_2, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
};

static struct pm8xxx_adc_properties pm8921_adc_data = {
	.adc_vdd_reference	= 1800, /* milli-voltage for this adc */
	.bitresolution		= 15,
	.bipolar                = 0,
};

static struct pm8xxx_adc_platform_data pm8921_adc_pdata = {
	.adc_channel		= pm8921_adc_channels_data,
	.adc_num_board_channel	= ARRAY_SIZE(pm8921_adc_channels_data),
	.adc_prop		= &pm8921_adc_data,
	.adc_mpp_base		= PM8921_MPP_PM_TO_SYS(1),
	.scale_tbls		= adc_scale_tbls,
};

struct pm8921_platform_data pm8921_platform_data __devinitdata = {
	.irq_pdata		= &pm8xxx_irq_pdata,
	.gpio_pdata		= &pm8xxx_gpio_pdata,
	.mpp_pdata		= &pm8xxx_mpp_pdata,
	.rtc_pdata              = &pm8xxx_rtc_pdata,
	.pwrkey_pdata		= &pm8xxx_pwrkey_pdata,
	.keypad_pdata		= NULL, /* to be passed pm8921_init_keypad */
	.misc_pdata		= &pm8xxx_misc_pdata,
	.regulator_pdatas	= msm_pm8921_regulator_pdata,
	.charger_pdata		= &pm8921_chg_pdata,
	.bms_pdata		= &pm8921_bms_pdata,
	.adc_pdata		= &pm8921_adc_pdata,
	.leds_pdata		= &pm8xxx_leds_pdata,
	.ccadc_pdata		= &pm8xxx_ccadc_pdata,
};

struct msm_ssbi_platform_data msm8960_ssbi_pm8921_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name			= "pm8921-core",
		.platform_data		= &pm8921_platform_data,
	},
};

struct msm_cpuidle_state msm_cstates[] __initdata = {
	{0, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{0, 1, "C1", "STANDALONE_POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE},

	{0, 2, "C2", "POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE},

	{1, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{1, 1, "C1", "STANDALONE_POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE},
};

struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR * 2] = {
	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_POWER_COLLAPSE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
		.residency = 0,
	},

	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},

	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 1,
		.suspend_enabled = 1,
	},

	[MSM_PM_MODE(1, MSM_PM_SLEEP_MODE_POWER_COLLAPSE)] = {
		.idle_supported = 0,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
		.residency = 0,
	},

	[MSM_PM_MODE(1, MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},

	[MSM_PM_MODE(1, MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT)] = {
		.idle_supported = 1,
		.suspend_supported = 0,
		.idle_enabled = 1,
		.suspend_enabled = 0,
	},
};

int pm8xxx_set_led_info(unsigned index, struct led_info *linfo)
{
	if (index >= ARRAY_SIZE(pm8921_led_info)) {
		pr_err("%s: index %d out of bounds\n", __func__, index);
		return -EINVAL;
	}
	if (!linfo) {
		pr_err("%s: invalid led_info pointer\n", __func__);
		return -EINVAL;
	}
	memcpy(&pm8921_led_info[index], linfo, sizeof(struct led_info));
	return 0;
}

void __init pm8921_gpio_mpp_init(struct pm8xxx_gpio_init *pm8921_gpios,
									unsigned gpio_size,
									struct pm8xxx_mpp_init *pm8921_mpps,
									unsigned mpp_size)
{
	int i, rc;

	for (i = 0; i < gpio_size; i++) {
		rc = pm8xxx_gpio_config(pm8921_gpios[i].gpio,
					&pm8921_gpios[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_gpio_config: rc=%d\n", __func__, rc);
			break;
		}
	}

	for (i = 0; i < mpp_size; i++) {
		rc = pm8xxx_mpp_config(pm8921_mpps[i].mpp,
					&pm8921_mpps[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_mpp_config: rc=%d\n", __func__, rc);
			break;
		}
	}
}

void __init msm8960_pm_init(unsigned wakeup_irq)
{
	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	msm_pm_set_rpm_wakeup_irq(wakeup_irq);
	msm_cpuidle_set_states(msm_cstates, ARRAY_SIZE(msm_cstates),
				msm_pm_data);
}

void __init pm8921_init(struct pm8xxx_keypad_platform_data *keypad,
			int mode, int cool_temp, int warm_temp,
			void *cb, int lock, int hot_temp, int hot_temp_offset,
			int hot_temp_pcb, signed char hot_temp_pcb_offset)
{
	msm8960_device_ssbi_pmic.dev.platform_data =
				&msm8960_ssbi_pm8921_pdata;
	pm8921_platform_data.num_regulators = msm_pm8921_regulator_pdata_len;

	pm8921_platform_data.charger_pdata->cool_temp	= cool_temp,
	pm8921_platform_data.charger_pdata->warm_temp	= warm_temp,
	pm8921_platform_data.keypad_pdata = keypad;

#ifdef CONFIG_MACH_MSM8960_MMI
	pm8921_platform_data.charger_pdata->factory_mode = mode;
	battery_timeout = mode;
	pm8921_platform_data.charger_pdata->meter_lock = lock;
#ifdef CONFIG_PM8921_EXTENDED_INFO
	pm8921_platform_data.charger_pdata->hot_temp = hot_temp;
	pm8921_platform_data.charger_pdata->hot_temp_offset = hot_temp_offset;
	pm8921_platform_data.charger_pdata->hot_temp_pcb = hot_temp_pcb;
	pm8921_platform_data.charger_pdata->hot_temp_pcb_offset =
		hot_temp_pcb_offset;
#endif
#ifdef CONFIG_PM8921_FACTORY_SHUTDOWN
	pm8921_platform_data.charger_pdata->arch_reboot_cb = cb;
#endif
#endif
}
