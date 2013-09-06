/* Copyright (c) 2011-2012, Motorola Mobility. All rights reserved.
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
#include <linux/leds-pwm-gpio.h>
#include <linux/msm_ssbi.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/slimbus/slimbus.h>
#include <linux/bootmem.h>
#include <mach/kgsl.h>
#include <linux/mmc/msm_sdcc_raw.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif

#ifdef CONFIG_TOUCHSCREEN_CYTTSP3
#include <linux/input/cyttsp3_core.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS100_TS
#include <linux/melfas100_ts.h>
#endif

#include <linux/dma-mapping.h>
#include <linux/platform_data/qcom_crypto_device.h>
#include <linux/platform_data/qcom_wcnss_device.h>
#include <linux/leds.h>
#include <linux/leds-pm8xxx.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/msm_tsens.h>
#include <linux/ks8851.h>
#include <linux/gpio_event.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/hardware/gic.h>
#include <asm/mach/mmc.h>
#include <linux/usb/android.h>
#ifdef CONFIG_USB_ANDROID_DIAG
#include <mach/usbdiag.h>
#endif

#include <mach/mmi_emu_det.h>
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
#include <mach/system.h>

#include <linux/ct406.h>

#ifdef CONFIG_BACKLIGHT_LM3532
#include <linux/i2c/lm3532.h>
#endif

#ifdef CONFIG_WCD9310_CODEC
#include <linux/slimbus/slimbus.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#endif

#ifdef CONFIG_LEDS_LM3559
#include <linux/leds-lm3559.h>
#endif
#ifdef CONFIG_LEDS_LM3556
#include <linux/leds-lm3556.h>
#endif

#include <linux/ion.h>
#include <mach/ion.h>
#include <linux/w1-gpio.h>

#include "timer.h"
#include "devices.h"
#include "devices-msm8x60.h"
#include "spm.h"
#include "board-8960.h"
#include "board-mmi.h"
#include "pm.h"
#include <mach/cpuidle.h>
#include "rpm_resources.h"
#include "mpm.h"
#include "acpuclock.h"
#include "clock-local.h"
#include "rpm_log.h"
#include "smd_private.h"
#include "pm-boot.h"
#include "msm_watchdog.h"
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/apanic_mmc.h>
#include <linux/platform_data/ram_console.h>

#define PM8XXX_GPIO_OUTPUT_STRENGTH(_gpio, _val, _out_strength) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			_out_strength, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

/* Initial PM8921 GPIO configurations vanquish, quinara */
static struct pm8xxx_gpio_init pm8921_gpios_vanquish[] = {
	PM8XXX_GPIO_DISABLE(6),				 			/* Disable unused */
	PM8XXX_GPIO_DISABLE(7),				 			/* Disable NFC */
	PM8XXX_GPIO_INPUT(16,	    PM_GPIO_PULL_UP_30), /* SD_CARD_WP */
	PM8XXX_GPIO_PAIRED_OUT_VIN(21, PM_GPIO_VIN_L17), /* Whisper TX 2.7V */
	PM8XXX_GPIO_PAIRED_IN_VIN(22,  PM_GPIO_VIN_S4),  /* Whisper TX 1.8V */
	PM8XXX_GPIO_OUTPUT_FUNC(24, 0, PM_GPIO_FUNC_2),	 /* Red LED */
	PM8XXX_GPIO_OUTPUT_FUNC(25, 0, PM_GPIO_FUNC_2),	 /* Green LED */
	PM8XXX_GPIO_OUTPUT_FUNC(26, 0, PM_GPIO_FUNC_2),	 /* Blue LED */
	PM8XXX_GPIO_INPUT(20,	    PM_GPIO_PULL_UP_30), /* SD_CARD_DET_N */
	PM8XXX_GPIO_PAIRED_IN_VIN(41,  PM_GPIO_VIN_L17), /* Whisper RX 2.7V */
	PM8XXX_GPIO_PAIRED_OUT_VIN(42, PM_GPIO_VIN_S4),  /* Whisper RX 1.8V */
	PM8XXX_GPIO_OUTPUT(43,	    PM_GPIO_PULL_UP_1P5), /* DISP_RESET_N */
	PM8XXX_GPIO_OUTPUT_VIN(37, PM_GPIO_PULL_UP_1P5,
			PM_GPIO_VIN_L17),	/* DISP_RESET_N on P1C+ */
	/* TABLA CODEC RESET */
	PM8XXX_GPIO_OUTPUT_STRENGTH(34, 0, PM_GPIO_STRENGTH_MED),
};

/* Initial PM8921 MPP configurations */
static struct pm8xxx_mpp_init pm8921_mpps[] __initdata = {
	/* External 5V regulator enable; shared by HDMI and USB_OTG switches. */
	PM8XXX_MPP_INIT(7, D_OUTPUT, PM8921_MPP_DIG_LEVEL_S4, DOUT_CTRL_HIGH),
	PM8XXX_MPP_INIT(PM8XXX_AMUX_MPP_8, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH8,
								DOUT_CTRL_LOW),
	PM8XXX_MPP_INIT(11, D_BI_DIR, PM8921_MPP_DIG_LEVEL_S4, BI_PULLUP_1KOHM),
	PM8XXX_MPP_INIT(12, D_BI_DIR, PM8921_MPP_DIG_LEVEL_L17, BI_PULLUP_OPEN),
};

static u32 fdt_start_address; /* flattened device tree address */
static u32 fdt_size;

extern unsigned int k_atag_tcmd_raw_cid[4];
extern unsigned int k_atag_tcmd_raw_csd[4];
extern unsigned char k_atag_tcmd_raw_ecsd[512];

extern char pmic_hw_rev_txt_version[8];
extern unsigned char pmic_hw_rev_txt_rev1;
extern unsigned char pmic_hw_rev_txt_rev2;

extern unsigned short display_hw_rev_txt_manufacturer;
extern unsigned short display_hw_rev_txt_controller;
extern unsigned short display_hw_rev_txt_controller_drv;

static struct pm8xxx_gpio_init *pm8921_gpios = pm8921_gpios_vanquish;
static unsigned pm8921_gpios_size = ARRAY_SIZE(pm8921_gpios_vanquish);
static struct pm8xxx_keypad_platform_data *keypad_data = &mmi_keypad_data;
static int keypad_mode = MMI_KEYPAD_RESET;
/* Motorola ULPI default register settings
 * TXPREEMPAMPTUNE[5:4] = 11 (3x preemphasis current)
 * TXVREFTUNE[3:0] = 1111 increasing the DC level
 */
static int phy_settings[] = {0x34, 0x82, 0x3f, 0x81, -1};

static bool	bare_board;
bool camera_single_mclk;

static int  mmi_feature_hdmi = 1;

/*
 * HACK: Ideally all clocks would be configured directly from the device tree.
 * Don't use this as a template for future device tree changes.
 */
static void __init config_camera_single_mclk_from_dt(void)
{
	struct device_node *chosen;
	int len = 0;
	const void *prop;

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto out;

	prop = of_get_property(chosen, "camera_single_mclk", &len);
	if (prop && (len == sizeof(u8)))
		camera_single_mclk = *(u8 *)prop;

	of_node_put(chosen);

out:
	return;
}

#define BOOT_MODE_MAX_LEN 64
static char boot_mode[BOOT_MODE_MAX_LEN + 1];
int __init board_boot_mode_init(char *s)
{
	strncpy(boot_mode, s, BOOT_MODE_MAX_LEN);
	boot_mode[BOOT_MODE_MAX_LEN] = '\0';
	return 1;
}
__setup("androidboot.mode=", board_boot_mode_init);

#define SERIALNO_MAX_LEN 64
static char serialno[SERIALNO_MAX_LEN + 1];
int __init board_serialno_init(char *s)
{
	strncpy(serialno, s, SERIALNO_MAX_LEN);
	serialno[SERIALNO_MAX_LEN] = '\0';
	return 1;
}
__setup("androidboot.serialno=", board_serialno_init);

static char carrier[CARRIER_MAX_LEN + 1];
int __init board_carrier_init(char *s)
{
	strncpy(carrier, s, CARRIER_MAX_LEN);
	carrier[CARRIER_MAX_LEN] = '\0';
	return 1;
}
__setup("androidboot.carrier=", board_carrier_init);

static char baseband[BASEBAND_MAX_LEN + 1];
int __init board_baseband_init(char *s)
{
	strncpy(baseband, s, BASEBAND_MAX_LEN);
	baseband[BASEBAND_MAX_LEN] = '\0';
	return 1;
}
__setup("androidboot.baseband=", board_baseband_init);


#define BATTERY_DATA_MAX_LEN 32
static char battery_data[BATTERY_DATA_MAX_LEN+1];
int __init board_battery_data_init(char *s)
{
	strncpy(battery_data, s, BATTERY_DATA_MAX_LEN);
	battery_data[BATTERY_DATA_MAX_LEN] = '\0';
	return 1;
}
__setup("battery=", board_battery_data_init);

static int battery_data_is_meter_locked(void)
{
	return !strncmp(battery_data, "meter_lock", BATTERY_DATA_MAX_LEN);
}

static int boot_mode_is_factory(void)
{
	return !strncmp(boot_mode, "factory", BOOT_MODE_MAX_LEN);
}

int mot_panel_is_factory_mode(void)
{
	return boot_mode_is_factory();
}

static void emu_mux_ctrl_config_pin(int io_num, int value);
static int emu_det_enable_5v(int on);
/*
  MUX switch GPIOs in case EMU detection is disabled
*/
#define GPIO_MUX_CTRL0	107
#define GPIO_MUX_CTRL1	96

#define IC_EMU_POWER		0xA0
#define IC_HONEY_BADGER		0xA1

#define UART_GSBI12		12

#define GPIO_IS_MSM		1
#define GPIO_IS_PMIC		2
#define GPIO_IS_PMIC_MPP	3

#define GPIO_STD_CFG		0
#define GPIO_ALT_CFG		1

#define EMU_DET_RESOURCE_MAX	EMU_DET_GPIO_MAX

struct mmi_emu_det_gpio_data {
	int gpio;
	int owner;
	int num_cfg;
	void *gpio_configs;
};

struct emu_det_dt_data {
	int ic_type;
	int uart_gsbi;
	int accy_support;
	char *vdd_vreg;
	char *usb_otg_vreg;
	unsigned int vdd_voltage;
};

#define DECLARE(_id, _opt, _cfgs)	#_id
static const char *mmi_emu_det_io_map[EMU_DET_GPIO_MAX] = EMU_DET_IOs;
#undef DECLARE

static __init int mmi_emu_det_index_by_name(const char *name)
{
	int ret = -1, idx;
	for (idx = 0; idx < EMU_DET_GPIO_MAX; idx++)
		if (!strncmp(mmi_emu_det_io_map[idx],
			name, strlen(mmi_emu_det_io_map[idx]))) {
			ret = idx;
			break;
		}
	return ret;
}

static int get_hot_temp_dt(void)
{
	struct device_node *parent;
	int len = 0;
	const void *prop;
	u8 hot_temp = 0;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent) {
		pr_info("Parent Not Found\n");
		return 0;
	}
	prop = of_get_property(parent, "chg-hot-temp", &len);
	if (prop && (len == sizeof(u8)))
		hot_temp = *(u8 *)prop;

	of_node_put(parent);
	pr_info("DT Hot Temp = %d\n", hot_temp);
	return hot_temp;
}

static int get_hot_offset_dt(void)
{
	struct device_node *parent;
	int len = 0;
	const void *prop;
	u8 hot_temp_off = 0;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent) {
		pr_info("Parent Not Found\n");
		return 0;
	}

	prop = of_get_property(parent, "chg-hot-temp-offset", &len);
	if (prop && (len == sizeof(u8)))
		hot_temp_off = *(u8 *)prop;

	of_node_put(parent);
	pr_info("DT Hot Temp Offset = %d\n", hot_temp_off);
	return hot_temp_off;
}

static int get_hot_temp_pcb_dt(void)
{
	struct device_node *parent;
	int len = 0;
	const void *prop;
	u8 hot_temp_pcb = 0;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent) {
		pr_info("Parent Not Found\n");
		return 0;
	}
	prop = of_get_property(parent, "chg-hot-temp-pcb", &len);
	if (prop && (len == sizeof(u8)))
		hot_temp_pcb = *(u8 *)prop;

	of_node_put(parent);
	pr_info("DT Hot Temp PCB = %d\n", hot_temp_pcb);
	return hot_temp_pcb;
}

static signed char get_hot_pcb_offset_dt(void)
{
	struct device_node *parent;
	int len = 0;
	const void *prop;
	signed char hot_temp_pcb_off = 0;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent) {
		pr_info("Parent Not Found\n");
		return 0;
	}

	prop = of_get_property(parent, "chg-hot-temp-pcb-offset", &len);
	if (prop && (len == sizeof(u8)))
		hot_temp_pcb_off = *(signed char *)prop;

	of_node_put(parent);

	pr_info("DT Hot Temp Offset PCB = %d\n", (int)hot_temp_pcb_off);
	return hot_temp_pcb_off;
}

static struct emu_det_dt_data	emu_det_dt_data = {
	.ic_type	= IC_EMU_POWER,
	.uart_gsbi	= UART_GSBI12,
	.vdd_vreg	= "EMU_POWER",
	.usb_otg_vreg	= "8921_usb_otg",
	.vdd_voltage	= 2650000,
};

static int get_l17_voltage(void)
{
	return emu_det_dt_data.vdd_voltage;
}

static u8 uart_over_gsbi12;
static bool core_power_init, enable_5v_init;
static struct clk		*iface_clock;
static struct platform_device	emu_det_device;
static struct resource		mmi_emu_det_resources[EMU_DET_RESOURCE_MAX];
static struct msm_otg_platform_data	*otg_control_data = &msm_otg_pdata;
static struct mmi_emu_det_platform_data mmi_emu_det_data;
static struct mmi_emu_det_gpio_data	mmi_emu_det_gpio[EMU_DET_GPIO_MAX] = {
	{
		.gpio = GPIO_MUX_CTRL0,
	},
	{
		.gpio = GPIO_MUX_CTRL1,
	},
};

static void emu_det_hb_powerup(void)
{
	static bool init_done;
	if (!init_done) {
		if (emu_det_dt_data.ic_type == IC_HONEY_BADGER) {
			struct pm8xxx_adc_chan_result res;
			memset(&res, 0, sizeof(res));
			if (!pm8xxx_adc_read(CHANNEL_USBIN, &res) &&
				res.physical < 4000000) {
				pr_info("Powering up HoneyBadger\n");
				emu_det_enable_5v(1);
				msleep(50);
				emu_det_enable_5v(0);
			}
		}
		init_done = true;
	}
}

static int emu_det_core_power(int on)
{
	int rc = 0;
	static struct regulator *emu_vdd;

	if (!core_power_init) {
		emu_vdd = regulator_get(&emu_det_device.dev,
				emu_det_dt_data.vdd_vreg);
		if (IS_ERR(emu_vdd)) {
			pr_err("unable to get EMU_POWER reg\n");
			return PTR_ERR(emu_vdd);
		}
		rc = regulator_set_voltage(emu_vdd,
				emu_det_dt_data.vdd_voltage,
				emu_det_dt_data.vdd_voltage);
		if (rc) {
			pr_err("unable to set voltage %s reg\n",
				emu_det_dt_data.vdd_vreg);
			goto put_vdd;
		}
		core_power_init = true;
	}

	if (on) {
		rc = regulator_enable(emu_vdd);
		if (rc)
			pr_err("failed to enable %s reg\n",
				emu_det_dt_data.vdd_vreg);
		else
			emu_det_hb_powerup();
	} else {
		rc = regulator_disable(emu_vdd);
		if (rc)
			pr_err("failed to disable %s reg\n",
				emu_det_dt_data.vdd_vreg);
	}
	return rc;

put_vdd:
	if (emu_vdd)
		regulator_put(emu_vdd);
	emu_vdd = NULL;
	return rc;
}

static int emu_det_enable_5v(int on)
{
	int rc = 0;
	static struct regulator *emu_ext_5v;

	if (!enable_5v_init) {
		emu_ext_5v = regulator_get(&emu_det_device.dev,
				emu_det_dt_data.usb_otg_vreg);
		if (IS_ERR(emu_ext_5v)) {
			pr_err("unable to get %s reg\n",
				emu_det_dt_data.usb_otg_vreg);
			return PTR_ERR(emu_ext_5v);
		}
		enable_5v_init = true;
	}

	if (on) {
		rc = regulator_enable(emu_ext_5v);
		if (rc)
			pr_err("failed to enable %s reg\n",
				emu_det_dt_data.usb_otg_vreg);
	} else {
		rc = regulator_disable(emu_ext_5v);
		if (rc)
			pr_err("failed to disable %s reg\n",
				emu_det_dt_data.usb_otg_vreg);
	}
	return rc;
}

static int emu_det_configure_gpio(
		struct mmi_emu_det_gpio_data *gpio_data, int alt)
{
	int ret = -1;

	if (gpio_data->gpio_configs && gpio_data->num_cfg) {
		struct pm8xxx_mpp_config_data *mpp;
		struct msm_gpiomux_config gmux;
		struct gpiomux_setting *ms;
		struct pm_gpio *pm;

		switch (gpio_data->owner) {
		case GPIO_IS_MSM:
			memset(&gmux, 0, sizeof(gmux));
			ms = gpio_data->gpio_configs;
			if (alt)
				ms++;
			gmux.gpio = gpio_data->gpio;
			gmux.settings[GPIOMUX_ACTIVE] = ms;
			gmux.settings[GPIOMUX_SUSPENDED] = ms;
			msm_gpiomux_install(&gmux, 1);
			ret = 0;
				break;
		case GPIO_IS_PMIC:
			pm = (struct pm_gpio *)gpio_data->gpio_configs;
			if (alt)
				pm++;
			ret = pm8xxx_gpio_config(gpio_data->gpio, pm);
				break;
		case GPIO_IS_PMIC_MPP:
			mpp = gpio_data->gpio_configs;
			if (alt)
				mpp++;
			ret = pm8xxx_mpp_config(gpio_data->gpio, mpp);
				break;
		}
	} else
		pr_err("EMU missing gpio %d data\n", gpio_data->gpio);
	return ret;
}

static void emu_det_dp_dm_mode(int mode)
{
	int alt = !!mode;
	struct pm_gpio rx_gpio;
	struct pm_gpio *cfg_rx_gpio = (struct pm_gpio *)
			mmi_emu_det_gpio[DPLUS_GPIO].gpio_configs;
	if (alt)
		cfg_rx_gpio++;

	rx_gpio = *cfg_rx_gpio;

	if (mode == GPIO_MODE_ALTERNATE_2)
		rx_gpio.pull = PM_GPIO_PULL_DN;

	emu_det_configure_gpio(&mmi_emu_det_gpio[RX_PAIR_GPIO], alt);
	pm8xxx_gpio_config(mmi_emu_det_gpio[DPLUS_GPIO].gpio, &rx_gpio);

	emu_det_configure_gpio(&mmi_emu_det_gpio[TX_PAIR_GPIO], alt);
	emu_det_configure_gpio(&mmi_emu_det_gpio[DMINUS_GPIO],  alt);
}

static int emu_det_id_protect(int on)
{
	int ret = 0;
	int alt = !on;
	struct mmi_emu_det_gpio_data *gpio_data =
				&mmi_emu_det_gpio[EMU_ID_EN_GPIO];
	if (gpio_data->gpio)
		ret = emu_det_configure_gpio(gpio_data, alt);
	return ret;
}

static int emu_det_alt_mode(int on)
{
	int ret = 0;
	struct mmi_emu_det_gpio_data *gpio_data =
				&mmi_emu_det_gpio[SEMU_ALT_MODE_EN_GPIO];
	if (gpio_data->gpio)
		ret = emu_det_configure_gpio(gpio_data, on);
	return ret;
}

static void emu_det_gpio_mode(int mode)
{
	struct mmi_emu_det_gpio_data *tx_gpio_data =
				&mmi_emu_det_gpio[WHISPER_UART_TX_GPIO];
	struct mmi_emu_det_gpio_data *rx_gpio_data =
				&mmi_emu_det_gpio[WHISPER_UART_RX_GPIO];

	if (tx_gpio_data->gpio && rx_gpio_data->gpio) {
		emu_det_configure_gpio(tx_gpio_data, mode);
		emu_det_configure_gpio(rx_gpio_data, mode);
	}
}

static int emu_det_adc_id(void)
{
	struct pm8xxx_adc_chan_result res;
	memset(&res, 0, sizeof(res));
	if (pm8xxx_adc_read(CHANNEL_MPP_2, &res))
		pr_err("CHANNEL_MPP_2 ADC read error\n");

	return (int)res.physical/1000;
}

#define MSM_UART_NAME		"msm_serial_hs"
#define MSM_I2C_NAME            "qup_i2c"

#define MSM_GSBI12_PHYS		0x12480000
#define MSM_GSBI4_PHYS		0x16300000

#define GSBI_PROTOCOL_UNDEFINED	0x70
#define GSBI_PROTOCOL_I2C_UART	0x60

static void configure_gsbi_ctrl(int restore)
{
	void *gsbi_ctrl;
	uint32_t new;
	static uint32_t value;
	static bool stored;
	unsigned gsbi_phys = MSM_GSBI12_PHYS;

	if (!uart_over_gsbi12)
		gsbi_phys = MSM_GSBI4_PHYS;
	if (iface_clock)
		clk_enable(iface_clock);

	gsbi_ctrl = ioremap_nocache(gsbi_phys, 4);
	if (IS_ERR_OR_NULL(gsbi_ctrl)) {
		pr_err("cannot map GSBI ctrl reg\n");
		return;
	} else {
		if (restore) {
			if (stored) {
				writel_relaxed(value, gsbi_ctrl);
				stored = false;
				pr_info("GSBI reg 0x%x restored\n",
					value);
			}
		} else {
			new = value = readl_relaxed(gsbi_ctrl);
			stored = true;
			new &= ~GSBI_PROTOCOL_UNDEFINED;
			new |= GSBI_PROTOCOL_I2C_UART;
			writel_relaxed(new, gsbi_ctrl);
			mb();
			pr_info("GSBI reg 0x%x updated, "
				"stored value 0x%x\n", new, value);
		}
	}

	if (!uart_over_gsbi12 && iface_clock)
		clk_disable(iface_clock);

	iounmap(gsbi_ctrl);
}

static void configure_l2_error(int enable)
{
	if (enable)
		enable_msm_l2_erp_irq();
	else
		disable_msm_l2_erp_irq();
}

static struct mmi_emu_det_platform_data mmi_emu_det_data = {
	.enable_5v = emu_det_enable_5v,
	.core_power = emu_det_core_power,
	.id_protect = emu_det_id_protect,
	.alt_mode = emu_det_alt_mode,
	.gpio_mode = emu_det_gpio_mode,
	.adc_id = emu_det_adc_id,
	.dp_dm_mode = emu_det_dp_dm_mode,
	.gsbi_ctrl = configure_gsbi_ctrl,
	.cfg_l2_err = configure_l2_error,
};

static struct platform_device emu_det_device = {
	.name		= "emu_det",
	.id		= -1,
	.num_resources	= EMU_DET_RESOURCE_MAX,
	.resource	= mmi_emu_det_resources,
	.dev.platform_data = &mmi_emu_det_data,
};

#define MSM_UART12DM_PHYS	(MSM_GSBI12_PHYS + 0x10000)
#define MSM_UART4DM_PHYS	(MSM_GSBI4_PHYS + 0x40000)

static struct resource resources_uart_gsbi[] = {
	{
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "uartdm_channels",
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "uartdm_crci",
		.flags	= IORESOURCE_DMA,
	},
};

static u64 msm_uart_dmov_dma_mask = DMA_BIT_MASK(32);
static struct platform_device msm8960_device_uart_gsbi = {
	.name	= MSM_UART_NAME,
	.id	= 1,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi),
	.resource	= resources_uart_gsbi,
	.dev	= {
		.dma_mask		= &msm_uart_dmov_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static __init void mot_setup_whisper_uart(void)
{
	struct resource *res;

	res = platform_get_resource(&msm8960_device_uart_gsbi,
					IORESOURCE_IRQ, 0);
	res->start = res->end = uart_over_gsbi12 ?
				GSBI12_UARTDM_IRQ :
				GSBI4_UARTDM_IRQ;

	res = platform_get_resource(&msm8960_device_uart_gsbi,
					IORESOURCE_MEM, 0);
	res->start = uart_over_gsbi12 ?
				MSM_UART12DM_PHYS :
				MSM_UART4DM_PHYS;
	res->end = res->start + PAGE_SIZE - 1;

	res = platform_get_resource(&msm8960_device_uart_gsbi,
					IORESOURCE_DMA, 0);
	res->start = DMOV_WHISPER_TX_CHAN;
	res->end   = DMOV_WHISPER_RX_CHAN;

	res = platform_get_resource(&msm8960_device_uart_gsbi,
					IORESOURCE_DMA, 1);
	res->start = uart_over_gsbi12 ?
				DMOV_WHISPER_TX_CRCI_GSBI12 :
				DMOV_WHISPER_TX_CRCI_GSBI4;
	res->end = uart_over_gsbi12 ?
				DMOV_WHISPER_RX_CRCI_GSBI12 :
				DMOV_WHISPER_RX_CRCI_GSBI4;
}

static __init void mot_set_gsbi_clk(const char *con_id,
		struct clk *clk_entity, const char *dev_id)
{
	int num_lookups;
	struct clk_lookup *lk;
	if (!msm_clocks_8960_v1_info(&lk, &num_lookups)) {
		int i;
		for (i = 0; i < num_lookups; i++) {
			if (!strncmp((lk+i)->con_id, con_id,
				strlen((lk+i)->con_id)) &&
				((lk+i)->clk == clk_entity)) {
				(lk+i)->dev_id = dev_id;
				break;
			}
		}
	}
}

static __init void mot_setup_whisper_clk(void)
{
	struct clk *clk;

	if (uart_over_gsbi12) {
		if (!msm_gsbi12_uart_clk_ptr(&clk))
			mot_set_gsbi_clk("core_clk", clk,
					MSM_UART_NAME ".1");
	} else {
		if (!msm_gsbi4_uart_clk_ptr(&clk))
			mot_set_gsbi_clk("core_clk", clk,
					MSM_UART_NAME ".1");
	}
}

static __init void emu_det_gpio_init(void)
{
	int i;
	for (i = 0; i <  EMU_DET_GPIO_MAX; i++) {
		struct mmi_emu_det_gpio_data *gpio_data =
						&mmi_emu_det_gpio[i];
		if (gpio_data->gpio)
			emu_det_configure_gpio(gpio_data, GPIO_MODE_STANDARD);
	}
}

static void emu_mux_ctrl_config_pin(int io_num, int value)
{
	int rc, gpio;
	const char *name;
	struct resource *res;

	if (!(io_num >= 0 && io_num < EMU_DET_GPIO_MAX)) {
		pr_err("EMU invalid gpio index %d\n", io_num);
		return;
	}

	res = platform_get_resource_byname(&emu_det_device,
			IORESOURCE_IO, mmi_emu_det_io_map[io_num]);
	if (!res) {
		gpio = mmi_emu_det_gpio[io_num].gpio;
		name = mmi_emu_det_io_map[io_num];
	} else {
		gpio = res->start;
		name = res->name;
	}
	rc = gpio_request(gpio, name);
	if (rc) {
		pr_err("Could not request %s for GPIO %d (should only reach here"
			" if factory shutdown)\n", name, gpio);
	}
	rc = gpio_direction_output(gpio, value);
	if (rc) {
		pr_err("Could not set %s for GPIO %d\n", name, gpio);
		gpio_free(gpio);
		return;
	}
}

static __init void mot_init_emu_detection(
			struct msm_otg_platform_data *ctrl_data)
{
	if (ctrl_data) {
		ctrl_data->otg_control = OTG_ACCY_CONTROL;
		ctrl_data->pmic_id_irq = 0;
		ctrl_data->accy_pdev = &emu_det_device;

		iface_clock = clk_get_sys(uart_over_gsbi12 ?
			MSM_I2C_NAME ".12" : MSM_I2C_NAME ".4", "iface_clk");
		if (IS_ERR(iface_clock))
			pr_err("error getting GSBI iface clk\n");

		emu_det_gpio_init();
		mot_setup_whisper_uart();

	} else {
		/* If platform data is not set, safely drive the MUX
		 * CTRL pins to the USB configuration.
		 */
		emu_mux_ctrl_config_pin(EMU_MUX_CTRL0_GPIO, 1);
		emu_mux_ctrl_config_pin(EMU_MUX_CTRL1_GPIO, 0);
	}
}

static void __init config_ulpi_from_dt(void)
{
	struct device_node *chosen;
	int len;
	const void *prop;

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto out;

	/*
	 * the phy init sequence read from the device tree should be a
	 * sequence of value/register pairs
	 */
	prop = of_get_property(chosen, "ulpi_phy_init_seq", &len);
	if (prop && !(len % 2)) {
		int i;
		u8* prop_val;

		msm_otg_pdata.phy_init_seq = kzalloc(sizeof(int)*(len+1),
							GFP_KERNEL);
		if (!msm_otg_pdata.phy_init_seq) {
			msm_otg_pdata.phy_init_seq = phy_settings;
			goto put_node;
		}

		msm_otg_pdata.phy_init_seq[len] = -1;
		prop_val = (u8 *)prop;

		for (i = 0; i < len; i+=2) {
			msm_otg_pdata.phy_init_seq[i] = prop_val[i];
			msm_otg_pdata.phy_init_seq[i+1] = prop_val[i+1];
		}
	} else
		msm_otg_pdata.phy_init_seq = phy_settings;

put_node:
	of_node_put(chosen);

out:
	return;
}

/* defaulting to qinara, atag parser will override */
/* todo: finalize the names, move display related stuff to board-msm8960-panel.c */
#if defined(CONFIG_FB_MSM_MIPI_MOT_CMD_HD_PT)
#define DEFAULT_PANEL_NAME "mipi_mot_cmd_auo_hd_450"
#elif defined(CONFIG_FB_MSM_MIPI_MOT_VIDEO_HD_PT)
#define DEFAULT_PANEL_NAME "mipi_mot_video_smd_hd_465"
#elif defined(CONFIG_FB_MSM_MIPI_MOT_CMD_QHD_PT)
#define DEFAULT_PANEL_NAME "mipi_mot_cmd_auo_qhd_430"
#else
#define DEFAULT_PANEL_NAME ""
#endif

static char panel_name[PANEL_NAME_MAX_LEN + 1] = DEFAULT_PANEL_NAME;
static char extended_baseband[BASEBAND_MAX_LEN+1] = "\0";

static int is_smd_hd_465(void)
{
	return !strncmp(panel_name, "mipi_mot_video_smd_hd_465",
				PANEL_NAME_MAX_LEN);
}
static int is_smd_qhd_429(void)
{
	return !strncmp(panel_name, "mipi_mot_cmd_smd_qhd_429",
				PANEL_NAME_MAX_LEN);
}
static int is_auo_hd_450(void)
{
	return !strncmp(panel_name, "mipi_mot_cmd_auo_hd_450",
							PANEL_NAME_MAX_LEN);
}
/* on vanquish, met mipi read issue after panel resume at very rare rate.
 * The issue was caused the noise that VDDIO coupled into VCI during resume,
 * the noise would get into IC logic block that caused mipi block to be
 * unstable. Having VDDIO comes before VCI for 18ms avoided the high
 * spike on VCI during resume
 */
static bool is_defered_vci_en(void)
{
	bool ret = false;

	if (is_smd_hd_465())
		ret = true;

	return ret;
}

static struct regulator *reg_vci;

/*
 * This voltage used to enable in mipi_dsi_panel_power() but since SOL
 * smooth transition between boot loader and kernel, this API will be called
 * much later on. The panel needs to have this voltage to run when kernel starts
 */
static __init int enable_ext_5v_reg(void)
{
	static struct regulator *ext_5v_vreg;
	int rc;

	ext_5v_vreg = regulator_get(NULL, "ext_5v");
	if (IS_ERR(ext_5v_vreg)) {
		pr_err("could not get 8921_ext_5v, rc = %ld\n",
							PTR_ERR(ext_5v_vreg));
		rc = -ENODEV;
		goto end;
	}

	rc = regulator_enable(ext_5v_vreg);
	if (rc)
		pr_err("regulator enable failed, rc=%d\n", rc);
end:
	return rc;
}

/*
 * HACK: Ideally instead of basing code decisions on a string specifying the
 * name of the device, the device tree would contain a structure composed of
 * individual configuratble items that could be use in code to make decisions.
 * Don't use this as a template for future device tree changes.
 */
static __init void load_panel_name_from_dt(void)
{
	struct device_node *chosen;
	int len = 0;
	const void *prop;

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto out;

	prop = of_get_property(chosen, "panel_name", &len);
	if (prop && len) {
		strlcpy(panel_name, (const char *)prop,
				len > PANEL_NAME_MAX_LEN + 1
				? PANEL_NAME_MAX_LEN + 1 : len);
	}

	of_node_put(chosen);

out:
	return;
}

/* TODO. This is part of QCOM changes, but don't know if MOT need this */
#if 0
static void mipi_dsi_panel_pwm_cfg(void)
{
	int rc;
	static int mipi_dsi_panel_gpio_configured;
	static struct pm_gpio pwm_enable = {
		.direction        = PM_GPIO_DIR_OUT,
		.output_buffer    = PM_GPIO_OUT_BUF_CMOS,
		.output_value     = 1,
		.pull             = PM_GPIO_PULL_NO,
		.vin_sel          = PM_GPIO_VIN_VPH,
		.out_strength     = PM_GPIO_STRENGTH_HIGH,
		.function         = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol      = 0,
		.disable_pin      = 0,
	};
	static struct pm_gpio pwm_mode = {
		.direction        = PM_GPIO_DIR_OUT,
		.output_buffer    = PM_GPIO_OUT_BUF_CMOS,
		.output_value     = 0,
		.pull             = PM_GPIO_PULL_NO,
		.vin_sel          = PM_GPIO_VIN_S4,
		.out_strength     = PM_GPIO_STRENGTH_HIGH,
		.function         = PM_GPIO_FUNC_2,
		.inv_int_pol      = 0,
		.disable_pin      = 0,
	};

	if (mipi_dsi_panel_gpio_configured == 0) {
		/* pm8xxx: gpio-21, Backlight Enable */
		rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(21), &pwm_enable);
		if (rc != 0)
			pr_err("%s: pwm_enabled failed\n", __func__);

		/* pm8xxx: gpio-24, Bl: Off, PWM mode */
		rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(24), &pwm_mode);
		if (rc != 0)
			pr_err("%s: pwm_mode failed\n", __func__);

		mipi_dsi_panel_gpio_configured++;
	}
}
#endif

static bool use_mdp_vsync = MDP_VSYNC_ENABLED;

bool mipi_mot_panel_is_cmd_mode(void);
int mipi_panel_power_en(int on);
static int mipi_dsi_power(int on)
{
	static struct regulator *reg_l23, *reg_l2;
	int rc;

	pr_debug("%s (%d) is called\n", __func__, on);

	if (!(reg_l23 || reg_l2)) {

		/*
		 * This is a HACK for SOL smooth transtion: Because QCOM can
		 * not keep the MIPI lines at LP11 start when the kernel start
		 * so, when the first update, we need to reset the panel to
		 * raise the err detection from panel due to the MIPI lines
		 * drop.
		 * - With Video mode, this will be call mdp4_dsi_video_on()
		 *   before it disable the timing generator. This will prevent
		 *   the image to fade away.
		 * - Command mode, then we have to reset in the first call
		 *   to trun on the power
		 */
		if (mipi_mot_panel_is_cmd_mode())
			mipi_panel_power_en(0);

		reg_l23 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_vddio");
		if (IS_ERR(reg_l23)) {
			pr_err("could not get 8921_l23, rc = %ld\n",
							PTR_ERR(reg_l23));
			rc = -ENODEV;
			goto end;
		}

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
							PTR_ERR(reg_l2));
			rc = -ENODEV;
			goto end;
		}

		rc = regulator_set_voltage(reg_l23, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage l23 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}
	}

	if (on) {
		rc = regulator_set_optimum_mode(reg_l23, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}

		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}

		rc = regulator_enable(reg_l23);
		if (rc) {
			pr_err("enable l23 failed, rc=%d\n", rc);
			rc =  -ENODEV;
			goto end;
		}

		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		mdelay(10);
	} else {

		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2 failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		rc = regulator_disable(reg_l23);
		if (rc) {
			pr_err("disable reg_l23 failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		rc = regulator_set_optimum_mode(reg_l23, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}

		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}
	}
	rc = 0;
end:
	return rc;
}

int panel_turn_on_vci(struct regulator *reg_vci)
{
	int rc = 0;

	if (NULL != reg_vci) {
		rc = regulator_set_optimum_mode(reg_vci, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode vci failed, rc=%d\n", rc);
			rc = -EINVAL;
		}

		rc = regulator_enable(reg_vci);
		if (rc) {
			pr_err("enable vci failed, rc=%d\n", rc);
			rc = -ENODEV;
		}
	}

	return rc;
}

int mipi_panel_power_en(int on)
{
	int rc;
	static int disp_5v_en, lcd_reset;
	static int lcd_reset1; /* this is a hacked for vanquish phone */

	pr_debug("%s (%d) is called\n", __func__, on);

	if (lcd_reset == 0) {
		/*
		 * This is a work around for Vanquish P1C HW ONLY.
		 * There are 2 HW versions of vanquish P1C, wing board phone and
		 * normal phone. The wing P1C phone will use GPIO_PM 43 and
		 * normal P1C phone will use GPIO_PM 37  but both of them will
		 * have the same HWREV.
		 * To make both of them to work, then if HWREV=P1C, then we
		 * will toggle both GPIOs 43 and 37, but there will be one to
		 * be used, and there will be no harm if another doesn't use.
		 */
		if (is_smd_hd_465()) {
			if (system_rev == HWREV_P1C) {
				lcd_reset = PM8921_GPIO_PM_TO_SYS(43);
				lcd_reset1 = PM8921_GPIO_PM_TO_SYS(37);
			} else if (system_rev > HWREV_P1C)
				lcd_reset = PM8921_GPIO_PM_TO_SYS(37);
			else
				lcd_reset = PM8921_GPIO_PM_TO_SYS(43);
		} else if (is_smd_qhd_429())
			lcd_reset = PM8921_GPIO_PM_TO_SYS(37);
		else
			lcd_reset = PM8921_GPIO_PM_TO_SYS(43);

		rc = gpio_request(lcd_reset, "disp_rst_n");
		if (rc) {
			pr_err("request lcd_reset failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		if (is_smd_hd_465() && lcd_reset1 != 0) {
			rc = gpio_request(lcd_reset1, "disp_rst_1_n");
			if (rc) {
				pr_err("request lcd_reset1 failed, rc=%d\n",
									rc);
				rc = -ENODEV;
				goto end;
			}
		}

		disp_5v_en = 13;
		rc = gpio_request(disp_5v_en, "disp_5v_en");
		if (rc) {
			pr_err("request disp_5v_en failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		rc = gpio_direction_output(disp_5v_en, 1);
		if (rc) {
			pr_err("set output disp_5v_en failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		if ((is_smd_hd_465() && system_rev < HWREV_P1) ||
		    is_smd_qhd_429()) {
			rc = gpio_request(12, "disp_3_3");
			if (rc) {
				pr_err("%s: unable to request gpio %d (%d)\n",
							__func__, 12, rc);
				rc = -ENODEV;
				goto end;
			}

			rc = gpio_direction_output(12, 1);
			if (rc) {
				pr_err("%s: Unable to set direction\n",
								__func__);
				rc = -EINVAL;
				goto end;
			}
		}

		if (is_smd_hd_465() && system_rev >= HWREV_P1) {
			rc = gpio_request(0, "dsi_vci_en");
			if (rc) {
				pr_err("%s: unable to request gpio %d (%d)\n",
							__func__, 0, rc);
				rc = -ENODEV;
				goto end;
			}

			rc = gpio_direction_output(0, 1);
			if (rc) {
				pr_err("%s: Unable to set direction\n",
								__func__);
				rc = -EINVAL;
				goto end;
			}
		}
	}

	if (on) {
		gpio_set_value(disp_5v_en, 1);
		if (is_smd_hd_465())
			mdelay(5);

		if ((is_smd_hd_465() && system_rev < HWREV_P1) ||
		    is_smd_qhd_429())
			gpio_set_value_cansleep(12, 1);
		if (is_smd_hd_465() && system_rev >= HWREV_P1)
			gpio_set_value_cansleep(0, 1);

		if (is_defered_vci_en() == true) {
			/* defer to turn on VCI after vddio on for 18ms*/
			mdelay(18);
			rc = panel_turn_on_vci(reg_vci);
			if (rc)
				goto end;
		}

		if (is_smd_hd_465())
			mdelay(30);
		else if (is_smd_qhd_429())
			mdelay(25);
		else
			mdelay(10);

		gpio_set_value_cansleep(lcd_reset, 1);

		if (is_smd_hd_465() && lcd_reset1 != 0)
			gpio_set_value_cansleep(lcd_reset1, 1);

		mdelay(20);
	} else {
		gpio_set_value_cansleep(lcd_reset, 0);

		if (is_smd_hd_465() && lcd_reset1 != 0)
			gpio_set_value_cansleep(lcd_reset1, 0);

		mdelay(10);

		if (is_auo_hd_450()) {
			/* There is a HW issue of qinara P1, that if we release
			 * reg_5V during suspend, then we will have problem to
			 * turn it back on during resume
			 */
			if (system_rev >= HWREV_P2)
				gpio_set_value(disp_5v_en, 0);
		} else
			gpio_set_value(disp_5v_en, 0);

		if ((is_smd_hd_465() && system_rev < HWREV_P1) ||
		   is_smd_qhd_429())
			gpio_set_value_cansleep(12, 0);

		if (is_smd_hd_465() && system_rev >= HWREV_P1)
			gpio_set_value_cansleep(0, 0);
	}

	rc = 0;
end:
	return rc;
}


static int mipi_panel_power(int on)
{
	static bool panel_power_on;
	static struct regulator *reg_vddio;
	int rc;

	pr_debug("%s (%d) is called\n", __func__, on);

	if (!panel_power_on) {
		if (is_smd_hd_465() && system_rev >= HWREV_P2) {
			/* Vanquish P2 is not using VREG_L17 */
			reg_vddio = NULL;
		} else if (is_smd_hd_465() && system_rev >= HWREV_P1) {
				reg_vddio = regulator_get(
						&msm_mipi_dsi1_device.dev,
						"disp_vddio");
		} else if (is_smd_qhd_429())
			/* TODO: Need to check qinara P2 */
			reg_vddio = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_s4");
		else
			reg_vddio = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdc");

		if (IS_ERR(reg_vddio)) {
			pr_err("could not get 8921_vddio/vdc, rc = %ld\n",
				PTR_ERR(reg_vddio));
			rc = -ENODEV;
			goto end;
		}

		if ((is_smd_hd_465() && system_rev >= HWREV_P1) ||
		    is_smd_qhd_429()) {
			reg_vci = regulator_get(&msm_mipi_dsi1_device.dev,
					"disp_vci");
			if (IS_ERR(reg_vci)) {
				pr_err("could not get disp_vci, rc = %ld\n",
					PTR_ERR(reg_vci));
				rc = -ENODEV;
				goto end;
			}
		}

		if (NULL != reg_vddio) {
			if (is_smd_qhd_429())
				/* TODO: Need to check qinara P2 */
				rc = regulator_set_voltage(reg_vddio, 1800000,
							 1800000);
			else
				rc = regulator_set_voltage(reg_vddio,
						get_l17_voltage(), 2850000);

			if (rc) {
				pr_err("set_voltage l17 failed, rc=%d\n", rc);
				rc = -EINVAL;
				goto end;
			}
		}

		if (NULL != reg_vci) {
			rc = regulator_set_voltage(reg_vci, 3100000, 3100000);
			if (rc) {
				pr_err("set_voltage vci failed, rc=%d\n", rc);
				rc = -EINVAL;
				goto end;
			}
		}

		panel_power_on = true;
	}
	if (on) {
		if (NULL != reg_vddio) {
			rc = regulator_set_optimum_mode(reg_vddio, 100000);
			if (rc < 0) {
				pr_err("set_optimum_mode l8 failed, rc=%d\n",
						rc);
				rc = -EINVAL;
				goto end;
			}

			rc = regulator_enable(reg_vddio);
			if (rc) {
				pr_err("enable l8 failed, rc=%d\n", rc);
				rc = -ENODEV;
				goto end;
			}
		}

		if (is_defered_vci_en() == false) { /* turn on VCI now */
			rc = panel_turn_on_vci(reg_vci);
			if (rc)
				goto end;
		}

		mipi_panel_power_en(1);

	} else {
		mipi_panel_power_en(0);

		if (NULL != reg_vddio) {
			rc = regulator_disable(reg_vddio);
			if (rc) {
				pr_err("disable reg_l8 failed, rc=%d\n", rc);
				rc = -ENODEV;
				goto end;
			}

			rc = regulator_set_optimum_mode(reg_vddio, 100);
			if (rc < 0) {
				pr_err("set_optimum_mode l8 failed, rc=%d\n",
									rc);
				rc = -EINVAL;
				goto end;
			}

		}
		if (NULL != reg_vci) {
			rc = regulator_disable(reg_vci);
			if (rc) {
				pr_err("disable reg_vci failed, rc=%d\n", rc);
				rc = -ENODEV;
				goto end;
			}

			rc = regulator_set_optimum_mode(reg_vci, 100);
			if (rc < 0) {
				pr_err("set_optimum_mode vci failed, rc=%d\n",
									rc);
				rc = -EINVAL;
				goto end;
			}
		}
	}

	rc = 0;
end:
	return rc;
}

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.vsync_gpio = MDP_VSYNC_GPIO,
	.dsi_power_save = mipi_dsi_power,
	.panel_power_save = mipi_panel_power,
	.panel_power_force_off = mipi_panel_power_en,
};

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
#ifdef CONFIG_MSM_BUS_SCALING
	msm_fb_register_device("dtv", &dtv_pdata);
#endif
}

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL

static int hdmi_enable_5v(int on);
static int hdmi_core_power(int on, int show);
static int hdmi_cec_power(int on);
#ifdef CONFIG_DEBUG_FS
static void hdmi_test(int en);
#endif

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
#ifdef CONFIG_DEBUG_FS
	.test = hdmi_test,
#endif

};

#ifdef CONFIG_DEBUG_FS
static int block_5v_enable;
static void hdmi_test(int en)
{
	block_5v_enable = en;
	hdmi_enable_5v((en) ? 0 : 1);
}
#endif

static int hdmi_enable_5v(int on)
{
	/* TBD: PM8921 regulator instead of 8901 */
	static struct regulator *reg_8921_hdmi_mvs;	/* HDMI_5V */
	static int prev_on;
	int rc;

#ifdef CONFIG_DEBUG_FS
	if (block_5v_enable && on)
		return 0;
#endif

	if (on == prev_on)
		return 0;

	if (!reg_8921_hdmi_mvs)
		reg_8921_hdmi_mvs = regulator_get(&hdmi_msm_device.dev,
			"hdmi_mvs");

	if (on) {
		rc = regulator_enable(reg_8921_hdmi_mvs);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"8921_hdmi_mvs", rc);
			return rc;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		rc = regulator_disable(reg_8921_hdmi_mvs);
		if (rc)
			pr_warning("'%s' regulator disable failed, rc=%d\n",
				"8921_hdmi_mvs", rc);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
}

static int hdmi_core_power(int on, int show)
{
	static struct regulator *reg_8921_l23, *reg_8921_s4;
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	/* TBD: PM8921 regulator instead of 8901 */
	if (!reg_8921_l23) {
		reg_8921_l23 = regulator_get(&hdmi_msm_device.dev, "hdmi_avdd");
		if (IS_ERR(reg_8921_l23)) {
			pr_err("could not get reg_8921_l23, rc = %ld\n",
				PTR_ERR(reg_8921_l23));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_8921_l23, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage failed for 8921_l23, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	if (!reg_8921_s4) {
		reg_8921_s4 = regulator_get(&hdmi_msm_device.dev, "hdmi_vcc");
		if (IS_ERR(reg_8921_s4)) {
			pr_err("could not get reg_8921_s4, rc = %ld\n",
				PTR_ERR(reg_8921_s4));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_8921_s4, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage failed for 8921_s4, rc=%d\n", rc);
			return -EINVAL;
		}
	}

	if (on) {
		rc = regulator_set_optimum_mode(reg_8921_l23, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_8921_l23);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_avdd", rc);
			return rc;
		}
		rc = regulator_enable(reg_8921_s4);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_vcc", rc);
			return rc;
		}
		rc = gpio_request(100, "HDMI_DDC_CLK");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_CLK", 100, rc);
			goto error1;
		}
		rc = gpio_request(101, "HDMI_DDC_DATA");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_DATA", 101, rc);
			goto error2;
		}
		rc = gpio_request(102, "HDMI_HPD");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_HPD", 102, rc);
			goto error3;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(100);
		gpio_free(101);
		gpio_free(102);

		rc = regulator_disable(reg_8921_l23);
		if (rc) {
			pr_err("disable reg_8921_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_8921_s4);
		if (rc) {
			pr_err("disable reg_8921_s4 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_set_optimum_mode(reg_8921_l23, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;

error3:
	gpio_free(101);
error2:
	gpio_free(100);
error1:
	regulator_disable(reg_8921_l23);
	regulator_disable(reg_8921_s4);
	return rc;
}

static int hdmi_cec_power(int on)
{
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(99, "HDMI_CEC_VAR");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_CEC_VAR", 99, rc);
			goto error;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(99);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
error:
	return rc;
}
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#ifdef CONFIG_LEDS_LM3559
static struct lm3559_platform_data camera_flash_3559;

static struct lm3559_platform_data camera_flash_3559 = {
	.flags = (LM3559_PRIVACY | LM3559_TORCH |
			LM3559_FLASH | LM3559_FLASH_LIGHT |
			LM3559_MSG_IND | LM3559_ERROR_CHECK),
	.enable_reg_def = 0x00,
	.gpio_reg_def = 0x00,
	.adc_delay_reg_def = 0xc0,
	.vin_monitor_def = 0xff,
	.torch_brightness_def = 0x5b,
	.flash_brightness_def = 0xaa,
	.flash_duration_def = 0x0f,
	.flag_reg_def = 0x00,
	.config_reg_1_def = 0x6a,
	.config_reg_2_def = 0x00,
	.privacy_reg_def = 0x10,
	.msg_ind_reg_def = 0x00,
	.msg_ind_blink_reg_def = 0x1f,
	.pwm_reg_def = 0x00,
	.torch_enable_val = 0x1a,
	.flash_enable_val = 0x1b,
	.privacy_enable_val = 0x19,
	.pwm_val = 0x02,
	.msg_ind_val = 0xa0,
	.msg_ind_blink_val = 0x1f,
	.hw_enable = 0,
};
#endif /* CONFIG_LEDS_LM3559 */

#ifdef CONFIG_LEDS_LM3556
static struct lm3556_platform_data cam_flash_3556;

static struct lm3556_platform_data cam_flash_3556 = {
	.flags = (LM3556_TORCH | LM3556_FLASH |
			LM3556_ERROR_CHECK),
	.si_rev_filter_time_def = 0x00,
	.ivfm_reg_def = 0x80,
	.ntc_reg_def = 0x12,
	.ind_ramp_time_reg_def = 0x00,
	.ind_blink_reg_def = 0x00,
	.ind_period_cnt_reg_def = 0x00,
	.torch_ramp_time_reg_def = 0x00,
	.config_reg_def = 0x7f,
	.flash_features_reg_def = 0xd2,
	.current_cntrl_reg_def = 0x2b,
	.torch_brightness_def = 0x10,
	.enable_reg_def = 0x40,
	.flag_reg_def = 0x00,
	.torch_enable_val = 0x42,
	.flash_enable_val = 0x43,
	.hw_enable = 0,
};
#endif /* CONFIG_LEDS_LM3556 */

#ifdef CONFIG_MSM_CAMERA

#ifdef CONFIG_MT9M114
static struct msm_camera_sensor_flash_data flash_mt9m114 = {
	.flash_type = MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_mt9m114 = {
	.mount_angle    = 270,
	.sensor_reset   = 76,
	.analog_en      = 82,
	.digital_en     = 89,
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9m114_data = {
	.sensor_name          = "mt9m114",
	.pdata                = &msm_camera_csi_device_data[1],
	.flash_data           = &flash_mt9m114,
	.sensor_platform_info = &sensor_board_info_mt9m114,
	.gpio_conf            = &msm_camif_gpio_conf_mclk1,
	.csi_if               = 1,
	.camera_type          = FRONT_CAMERA_2D,
};

#endif

#ifdef CONFIG_S5K5B3G
static struct msm_camera_sensor_flash_data flash_s5k5b3g = {
	.flash_type = MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5k5b3g = {
	.mount_angle    = 90,
	.sensor_reset   = 76,
	.analog_en      = 82,
	.digital_en     = 89,
	.reg_1p2        = "8921_l12",
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k5b3g_data = {
	.sensor_name          = "s5k5b3g",
	.pdata                = &msm_camera_csi_device_data[1],
	.flash_data           = &flash_s5k5b3g,
	.sensor_platform_info = &sensor_board_info_s5k5b3g,
	.gpio_conf            = &msm_camif_gpio_conf_mclk1,
	.csi_if               = 1,
	.camera_type          = FRONT_CAMERA_2D,
};

#endif

#ifdef CONFIG_DW9714_ACT
static struct i2c_board_info dw9714_actuator_i2c_info = {
	I2C_BOARD_INFO("dw9714_act", 0x18),
};

static struct msm_actuator_info dw9714_actuator_info = {
	.board_info = &dw9714_actuator_i2c_info,
	.bus_id = MSM_8960_GSBI4_QUP_I2C_BUS_ID,
	.vcm_pwd = 0,
	.vcm_enable = 1,
};
#endif

#ifdef CONFIG_OV8820

#ifdef CONFIG_MSM_CAMERA_FLASH
static struct msm_camera_sensor_flash_src flash_src_ov8820 = {
	.flash_sr_type  = MSM_CAMERA_FLASH_SRC_LED,
	._fsrc  = {
		.led_src = {
#ifdef CONFIG_LEDS_LM3556
			.led_name = LM3556_NAME,
			.led_name_len = LM3556_NAME_LEN,
#endif
		},
	},
};
#endif

static struct msm_camera_sensor_flash_data flash_ov8820 = {
#ifdef CONFIG_MSM_CAMERA_FLASH
	.flash_type     = MSM_CAMERA_FLASH_LED,
	.flash_src      = &flash_src_ov8820,
#else
	.flash_type	= MSM_CAMERA_FLASH_NONE,
#endif
};

static struct msm_camera_sensor_platform_info sensor_board_info_ov8820 = {
	.mount_angle	= 90,
	.sensor_reset   = 97,
	.sensor_pwd     = 95,
	.analog_en      = 54,
	.digital_en     = 58,
	.reg_1p8        = "8921_l29",
	.reg_2p8        = "8921_l16",
};

static struct msm_camera_sensor_info msm_camera_sensor_ov8820_data = {
	.sensor_name          = "ov8820",
	.pdata                = &msm_camera_csi_device_data[0],
	.flash_data           = &flash_ov8820,
	.sensor_platform_info = &sensor_board_info_ov8820,
	.gpio_conf            = &msm_camif_gpio_conf_mclk0,
	.csi_if               = 1,
	.camera_type          = BACK_CAMERA_2D,
#ifdef CONFIG_DW9714_ACT
	.actuator_info        = &dw9714_actuator_info,
#endif
};

#endif

#ifdef CONFIG_OV7736
static struct msm_camera_sensor_flash_data flash_ov7736 = {
	.flash_type = MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_ov7736 = {
	.mount_angle  = 270,
	.sensor_reset = 76,
	.sensor_pwd   = 89,
	.analog_en    = 82,
	.reg_1p8      = "8921_l29",
};

static struct msm_camera_sensor_info msm_camera_sensor_ov7736_data = {
	.sensor_name          = "ov7736",
	.pdata                = &msm_camera_csi_device_data[1],
	.flash_data           = &flash_ov7736,
	.sensor_platform_info = &sensor_board_info_ov7736,
	.gpio_conf            = &msm_camif_gpio_conf_mclk1,
	.csi_if               = 1,
	.camera_type          = FRONT_CAMERA_2D,
};
#endif

void __init msm8960_init_cam(void)
{
	int i;
	struct msm_camera_sensor_info *cam_data[] = {
#ifdef CONFIG_MT9M114
		&msm_camera_sensor_mt9m114_data,
#endif
#ifdef CONFIG_S5K5B3G
		&msm_camera_sensor_s5k5b3g_data,
#endif
#ifdef CONFIG_OV8820
		&msm_camera_sensor_ov8820_data,
#endif
#ifdef CONFIG_OV7736
		&msm_camera_sensor_ov7736_data,
#endif
	};

	for (i = 0; i < ARRAY_SIZE(cam_data); i++) {
		struct msm_camera_sensor_info *s_info;
		s_info = cam_data[i];
		if (camera_single_mclk &&
				s_info->camera_type == FRONT_CAMERA_2D) {
			if (s_info->gpio_conf->cam_gpio_tbl_size != 1)
				pr_err("unexpected camera gpio "
						"configuration\n");
			else {
				pr_info("%s using gpio 5\n",
						s_info->sensor_name);
				s_info->gpio_conf->cam_gpio_tbl[0] = 5;
			}
		}
	}

	platform_device_register(&msm8960_device_csiphy0);
	platform_device_register(&msm8960_device_csiphy1);
	platform_device_register(&msm8960_device_csid0);
	platform_device_register(&msm8960_device_csid1);
	platform_device_register(&msm8960_device_ispif);
	platform_device_register(&msm8960_device_vfe);
	platform_device_register(&msm8960_device_vpe);
}
#endif

/*
 * - This is a work around for the panel SOL mooth transition feature
 * QCOm has provided patch to make this feature to work for video mode panel
 * but this will break MOT's feature for command mode.
 * - While waiting for QCOm to delivery a patch that can make this feature
 * to work for both pane; types. We will use this in the mean time
 * - Make sure to remove the FB_MSM_BOOTLOADER_INIT for command mode
 */
bool mipi_mot_panel_is_cmd_mode(void)
{
	bool ret = true;

	if (!strncmp("mipi_mot_video_smd_hd_465", panel_name,
							PANEL_NAME_MAX_LEN))
		ret = false;

	return ret;
}


static int msm_fb_detect_panel(const char *name)
{
	if (!strncmp(name, panel_name, PANEL_NAME_MAX_LEN)) {
		pr_info("%s: detected %s\n", __func__, name);
		return 0;
	}
	/*
	 * This is a HACK for SOL smooth transition, will need to clean up
	 * to use device tree for this purpose.
	 * with the Video mode, it uses this MDP_VSYNC_GPIO, 0 GPIO to
	 * enable the VDDIO. Because of the SOL smooth transition, this gpio
	 * must configure "high" to keep the VDDIO on
	 */
	else if (mipi_mot_panel_is_cmd_mode() != true)
		use_mdp_vsync = MDP_VSYNC_DISABLED;
	else
		use_mdp_vsync = MDP_VSYNC_ENABLED;

	if (mmi_feature_hdmi && !strncmp(name, HDMI_PANEL_NAME,
		strnlen(HDMI_PANEL_NAME,PANEL_NAME_MAX_LEN)))
		return 0;

	pr_warning("%s: not supported '%s'", __func__, name);
	return -ENODEV;
}

static struct led_pwm_gpio pm8xxx_pwm_gpio_leds[] = {
	[0] = {
		.name			= "red",
		.default_trigger	= "none",
		.pwm_id = 0,
		.gpio = PM8921_GPIO_PM_TO_SYS(24),
		.active_low = 0,
		.retain_state_suspended = 1,
		.default_state = 0,
	},
	[1] = {
		.name			= "green",
		.default_trigger	= "none",
		.pwm_id = 1,
		.gpio = PM8921_GPIO_PM_TO_SYS(25),
		.active_low = 0,
		.retain_state_suspended = 1,
		.default_state = 0,
	},
	[2] = {
		.name			= "blue",
		.default_trigger	= "none",
		.pwm_id = 2,
		.gpio = PM8921_GPIO_PM_TO_SYS(26),
		.active_low = 0,
		.retain_state_suspended = 1,
		.default_state = 0,
	},
};

static struct led_pwm_gpio_platform_data pm8xxx_rgb_leds_pdata = {
	.num_leds = ARRAY_SIZE(pm8xxx_pwm_gpio_leds),
	.leds = pm8xxx_pwm_gpio_leds,
	.max_brightness = LED_FULL,
};

static struct platform_device pm8xxx_rgb_leds_device = {
	.name	= "pm8xxx_rgb_leds",
	.id	= -1,
	.dev	= {
		.platform_data = &pm8xxx_rgb_leds_pdata,
	},
};

static void w1_gpio_enable_regulators(int enable);

#define BATT_EPROM_GPIO 93

static struct w1_gpio_platform_data msm8960_w1_gpio_device_pdata = {
	.pin = BATT_EPROM_GPIO,
	.is_open_drain = 0,
	.enable_external_pullup = w1_gpio_enable_regulators,
};

static struct platform_device msm8960_w1_gpio_device = {
	.name	= "w1-gpio",
	.dev	= {
		.platform_data = &msm8960_w1_gpio_device_pdata,
	},
};

static void w1_gpio_enable_regulators(int enable)
{
	static struct regulator *vdd1;
	static struct regulator *vdd2;
	int rc;

	if (!vdd1)
		vdd1 = regulator_get(&msm8960_w1_gpio_device.dev, "8921_l7");

	if (!vdd2)
		vdd2 = regulator_get(&msm8960_w1_gpio_device.dev, "8921_l17");

	if (enable) {
		if (!IS_ERR_OR_NULL(vdd1)) {
			rc = regulator_set_voltage(vdd1, 1850000, 2950000);
			if (!rc)
				rc = regulator_enable(vdd1);
			if (rc)
				pr_err("w1_gpio failed to set voltage "\
								"8921_l7\n");
		}
		if (!IS_ERR_OR_NULL(vdd2)) {
			rc = regulator_set_voltage(vdd2,
						get_l17_voltage(), 2850000);
			if (!rc) {
				rc = regulator_enable(vdd2);
			}
			if (rc)
				pr_err("w1_gpio failed to set voltage "\
								"8921_l17\n");
		}
	} else {
		if (!IS_ERR_OR_NULL(vdd1)) {
			rc = regulator_disable(vdd1);
			if (rc)
				pr_err("w1_gpio unable to disable 8921_l7\n");
		}
		if (!IS_ERR_OR_NULL(vdd2)) {
			rc = regulator_disable(vdd2);
			if (rc)
				pr_err("w1_gpio unable to disable 8921_l17\n");
		}
	}
}

struct mmi_unit_info_v1 mmi_unit_info_v1 = {
	.machine = "dummy_mach",
	.barcode = "dummy_barcode",
	.carrier = "dummy_carrier",
	.baseband = "dummy_basesband"
};

struct platform_device msm_device_smd = {
	.name	= "msm_smd",
	.id	= -1,
	.dev	= {
		.platform_data = &mmi_unit_info_v1,
	},
};

static void init_mmi_unit_info(void){
	struct mmi_unit_info_v1 *mui;
	mui = &mmi_unit_info_v1;
	mui->version = 1;
	mui->system_rev = system_rev;
	mui->system_serial_low = system_serial_low;
	mui->system_serial_high = system_serial_high;
	strncpy(mui->machine, machine_desc->name, MACHINE_MAX_LEN);
	strncpy(mui->barcode, serialno, BARCODE_MAX_LEN);
	strncpy(mui->baseband, extended_baseband, BASEBAND_MAX_LEN);
	strncpy(mui->carrier, carrier, CARRIER_MAX_LEN);

	pr_err("mmi_unit_info (SMEM): version = 0x%02x, system_rev = 0x%08x, "
		"system_serial = 0x%08x%08x, machine = '%s', barcode = '%s', "
		"baseband = '%s', carrier = '%s'\n",
		mui->version, mui->system_rev, mui->system_serial_high,
		mui->system_serial_low, mui->machine, mui->barcode,
		mui->baseband, mui->carrier);
}

static ssize_t hw_rev_txt_pmic_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
/* Format: TYPE:VENDOR:HWREV:DATE:FIRMWARE_REV:INFO  */
	return snprintf(buf, PAGE_SIZE,
			"PMIC:QUALCOMM-PM8921:%s:::rev1=0x%02X,rev2=0x%02X\n",
			pmic_hw_rev_txt_version,
			pmic_hw_rev_txt_rev1,
			pmic_hw_rev_txt_rev2);
}

static ssize_t hw_rev_txt_display_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"Display:0x%02X:0x%02X::0x%02X:\n",
			display_hw_rev_txt_manufacturer,
			display_hw_rev_txt_controller,
			display_hw_rev_txt_controller_drv);
}

static struct kobj_attribute hw_rev_txt_pmic_attribute =
	__ATTR(pmic, 0444, hw_rev_txt_pmic_show, NULL);

static struct kobj_attribute hw_rev_txt_display_attribute =
	__ATTR(display, 0444, hw_rev_txt_display_show, NULL);

static struct attribute *hw_rev_txt_attrs[] = {
	&hw_rev_txt_pmic_attribute.attr,
	&hw_rev_txt_display_attribute.attr,
	NULL
};

static struct attribute_group hw_rev_txt_attr_group = {
	.attrs = hw_rev_txt_attrs,
};

static int hw_rev_txt_init(void)
{
	static struct kobject *hw_rev_txt_kobj;
	int retval;

	hw_rev_txt_kobj = kobject_create_and_add("hardware_revisions", NULL);
	if (!hw_rev_txt_kobj) {
		pr_err("%s: failed to create /sys/hardware_revisions\n",
		       __func__);
		return -ENOMEM;
	}

	retval = sysfs_create_group(hw_rev_txt_kobj, &hw_rev_txt_attr_group);
	if (retval)
		pr_err("%s: failed for entries in /sys/hardware_revisions\n",
		       __func__);

	return retval;
}

static ssize_t cid_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08X 0x%08X 0x%08X 0x%08X\n",
		       k_atag_tcmd_raw_cid[0], k_atag_tcmd_raw_cid[1],
		       k_atag_tcmd_raw_cid[2], k_atag_tcmd_raw_cid[3]);
}

static ssize_t csd_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08X 0x%08X 0x%08X 0x%08X\n",
		       k_atag_tcmd_raw_csd[0], k_atag_tcmd_raw_csd[1],
		       k_atag_tcmd_raw_csd[2], k_atag_tcmd_raw_csd[3]);
}

static ssize_t ecsd_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	char *d = buf;
	char b[8];
	int i = 0;

	while (i < 512) {
		snprintf(b, 8, "%02X", k_atag_tcmd_raw_ecsd[i]);
		*d++ = b[0];
		*d++ = b[1];
		*d++ = ' ';
		i++;
	}
	*d++ = 10;
	*d = 0;

	return (512*3) + 1;
}

static struct kobj_attribute cid_attribute =
	__ATTR(cid, 0444, cid_show, NULL);

static struct kobj_attribute csd_attribute =
	__ATTR(csd, 0444, csd_show, NULL);

static struct kobj_attribute ecsd_attribute =
	__ATTR(ecsd, 0444, ecsd_show, NULL);

static struct attribute *emmc_attrs[] = {
	&cid_attribute.attr,
	&csd_attribute.attr,
	&ecsd_attribute.attr,
	NULL
};

static struct attribute_group emmc_attr_group = {
	.attrs = emmc_attrs,
};

static int emmc_version_init(void)
{
	static struct kobject *emmc_kobj;
	int retval;

	emmc_kobj = kobject_create_and_add("emmc", NULL);
	if (!emmc_kobj) {
		pr_err("%s: failed to create /sys/emmc\n", __func__);
		return -ENOMEM;
	}

	retval = sysfs_create_group(emmc_kobj, &emmc_attr_group);
	if (retval)
		pr_err("%s: failed for entries under /sys/emmc\n", __func__);

	return retval;
}

static struct {
	unsigned mr5;
	unsigned mr6;
	unsigned mr7;
	unsigned mr8;
	unsigned ramsize;
} *smem_ddr_info;
static char sysfsram_type_name[20] = "unknown";
static char sysfsram_vendor_name[20] = "unknown";
static uint32_t sysfsram_ramsize;

static ssize_t sysfsram_mr_register_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	uint32_t val = 0;
	const char *name = attr->attr.name;

	if (smem_ddr_info != NULL &&
		strnlen(name, 4) == 3 && name[0] == 'm' && name[1] == 'r')
	{
		switch (name[2]) {
		case '5': val = smem_ddr_info->mr5; break;
		case '6': val = smem_ddr_info->mr6; break;
		case '7': val = smem_ddr_info->mr7; break;
		case '8': val = smem_ddr_info->mr8; break;
		}
	}

	return snprintf(buf, 6, "0x%02x\n", val);
}

static ssize_t sysfsram_size_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 12, "%u\n", sysfsram_ramsize);
}

static ssize_t sysfsram_info_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 60, "%s:%s:%uMB\n",
			sysfsram_vendor_name,
			sysfsram_type_name,
			sysfsram_ramsize);
}

static ssize_t sysfsram_type_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%s\n", sysfsram_type_name);
}

static struct kobj_attribute ddr_mr5_register_attr =
	__ATTR(mr5, 0444, sysfsram_mr_register_show, NULL);

static struct kobj_attribute ddr_mr6_register_attr =
	__ATTR(mr6, 0444, sysfsram_mr_register_show, NULL);

static struct kobj_attribute ddr_mr7_register_attr =
	__ATTR(mr7, 0444, sysfsram_mr_register_show, NULL);

static struct kobj_attribute ddr_mr8_register_attr =
	__ATTR(mr8, 0444, sysfsram_mr_register_show, NULL);

static struct kobj_attribute ddr_size_attr =
	__ATTR(size, 0444, sysfsram_size_show, NULL);

static struct kobj_attribute ddr_type_attr =
	__ATTR(type, 0444, sysfsram_type_show, NULL);

static struct kobj_attribute ddr_info_attr =
	__ATTR(info, 0444, sysfsram_info_show, NULL);

static struct attribute *ram_info_properties_attrs[] = {
	&ddr_mr5_register_attr.attr,
	&ddr_mr6_register_attr.attr,
	&ddr_mr7_register_attr.attr,
	&ddr_mr8_register_attr.attr,
	&ddr_size_attr.attr,
	&ddr_type_attr.attr,
	&ddr_info_attr.attr,
	NULL
};

static struct attribute_group ram_info_properties_attr_group = {
	.attrs = ram_info_properties_attrs,
};

static void init_mmi_ram_info(void){
	int rc = 0;
	static struct kobject *ram_info_properties_kobj;
	uint32_t vid, tid;
	const char *tname = "unknown";
	const char *vname = "unknown";
	static const char *vendors[] = {
		"unknown",
		"Samsung",
		"Qimonda",
		"Elpida",
		"Etron",
		"Nanya",
		"Hynix",
		"Mosel",
		"Winbond",
		"ESMT",
		"unknown",
		"Spansion",
		"SST",
		"ZMOS",
		"Intel"
	};
	static const char *types[] = {
		"S4 SDRAM",
		"S2 SDRAM",
		"N NVM",
		"Reserved"
	};

	smem_ddr_info = smem_alloc(SMEM_SDRAM_INFO, sizeof(*smem_ddr_info));

	if (smem_ddr_info != NULL) {
		char apanic_annotation[128];

		/* identify vendor */
		vid = smem_ddr_info->mr5 & 0xFF;
		if (vid < (sizeof(vendors)/sizeof(vendors[0])))
			vname = vendors[vid];
		else if (vid == 0xFE)
			vname = "Numonyx";
		else if (vid == 0xFF)
			vname = "Micron";

		snprintf(sysfsram_vendor_name, sizeof(sysfsram_vendor_name),
			"%s", vname);

		/* identify type */
		tid = smem_ddr_info->mr8 & 0x03;
		if (tid < (sizeof(types)/sizeof(types[0])))
			tname = types[tid];

		snprintf(sysfsram_type_name, sizeof(sysfsram_type_name),
			"%s", tname);

		/* extract size */
		sysfsram_ramsize = smem_ddr_info->ramsize;

		snprintf(apanic_annotation, sizeof(apanic_annotation),
			"RAM: %s, %s, %u MB, MR5:0x%02X, MR6:0x%02X, "
			"MR7:0x%02X, MR8:0x%02X\n",
			vname, tname, smem_ddr_info->ramsize,
			smem_ddr_info->mr5, smem_ddr_info->mr6,
			smem_ddr_info->mr7, smem_ddr_info->mr8);
		apanic_mmc_annotate(apanic_annotation);
	}
	else
		pr_err("%s: failed to access DDR info in SMEM\n", __func__);

	/* create sysfs object */
	ram_info_properties_kobj = kobject_create_and_add("ram", NULL);

	if (ram_info_properties_kobj)
		rc = sysfs_create_group(ram_info_properties_kobj,
			&ram_info_properties_attr_group);

	if (!ram_info_properties_kobj || rc)
		pr_err("%s: failed to create /sys/ram\n", __func__);
}

static ssize_t
sysfs_extended_baseband_show(struct sys_device *dev,
			struct sysdev_attribute *attr,
			char *buf)
{
	if (!strnlen(extended_baseband, BASEBAND_MAX_LEN)) {
		pr_err("%s: No extended_baseband available!\n", __func__);
		return 0;
	}
	return snprintf(buf, BASEBAND_MAX_LEN, "%s\n", extended_baseband);
}


static struct sysdev_attribute baseband_files[] = {
	_SYSDEV_ATTR(extended_baseband, 0444,
			sysfs_extended_baseband_show, NULL),
};

static struct sysdev_class baseband_sysdev_class = {
	.name = "baseband",
};

static struct sys_device baseband_sys_device = {
	.id = 0,
	.cls = &baseband_sysdev_class,
};

static void init_sysfs_extended_baseband(void){
	int err;

	if (!strnlen(extended_baseband, BASEBAND_MAX_LEN)) {
		pr_err("%s: No extended_baseband available!\n", __func__);
		return;
	}

	err = sysdev_class_register(&baseband_sysdev_class);
	if (err) {
		pr_err("%s: sysdev_class_register fail (%d)\n",
		       __func__, err);
		return;
	}

	err = sysdev_register(&baseband_sys_device);
	if (err) {
		pr_err("%s: sysdev_register fail (%d)\n",
		       __func__, err);
		return;
	}

	err = sysdev_create_file(&baseband_sys_device, &baseband_files[0]);
	if (err) {
		pr_err("%s: sysdev_create_file(%s)=%d\n",
		       __func__, baseband_files[0].attr.name, err);
		return;
	}
}

static struct platform_device *mmi_devices[] __initdata = {
	&msm8960_w1_gpio_device,
	&msm_8960_q6_lpass,
	&msm_8960_q6_mss_fw,
	&msm_8960_q6_mss_sw,
	&msm_8960_riva,
	&msm_pil_vidc,
	&msm_pil_tzapps,
	&msm8960_device_otg,
	&msm8960_device_gadget_peripheral,
	&msm_device_hsusb_host,
	&android_usb_device,
	&msm_pcm,
	&msm_multi_ch_pcm,
	&msm_lowlatency_pcm,
	&msm_pcm_routing,
	&msm_cpudai0,
	&msm_cpudai1,
	&msm8960_cpudai_slimbus_2_rx,
	&msm8960_cpudai_slimbus_2_tx,
	&msm_cpudai_hdmi_rx,
	&msm_cpudai_bt_rx,
	&msm_cpudai_bt_tx,
	&msm_cpudai_fm_rx,
	&msm_cpudai_fm_tx,
	&msm_cpudai_auxpcm_rx,
	&msm_cpudai_auxpcm_tx,
	&msm_cpu_fe,
	&msm_stub_codec,
	&msm_kgsl_3d0,
#ifdef CONFIG_MSM_KGSL_2D 			/* OpenVG support */
	&msm_kgsl_2d0,
	&msm_kgsl_2d1,
#endif
#ifdef CONFIG_MSM_GEMINI  			/* Inline JPEG engine */
	&msm8960_gemini_device,
#endif
	&msm_voice,
	&msm_voip,
	&msm_lpa_pcm,
	&msm_cpudai_afe_01_rx,
	&msm_cpudai_afe_01_tx,
	&msm_cpudai_afe_02_rx,
	&msm_cpudai_afe_02_tx,
	&msm_pcm_afe,
	&msm_compr_dsp,
	&msm_cpudai_incall_music_rx,
	&msm_cpudai_incall_record_rx,
	&msm_cpudai_incall_record_tx,
	&msm_pcm_hostless,
	&msm_bus_apps_fabric,
	&msm_bus_sys_fabric,
	&msm_bus_mm_fabric,
	&msm_bus_sys_fpb,
	&msm_bus_cpss_fpb,
	&msm8960_device_uart_gsbi,
	&pm8xxx_rgb_leds_device,
};

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL	/* HDMI support */
static struct platform_device *mmi_hdmi_devices[] __initdata = {
	&hdmi_msm_device,
};
#endif

static struct msm_pm_boot_platform_data msm_pm_boot_pdata __initdata = {
	.mode = MSM_PM_BOOT_CONFIG_TZ,
};

static __init int dt_read_msm_gpio_config(struct device_node *parent,
			const char *property, int property_len,
			struct gpiomux_setting *msm_gpio)
{
	int ret = -1, len = 0;
	const void *config_prop;

	config_prop = of_get_property(parent, property, &len);
	if (config_prop && (len == property_len)) {
		u8 *config = (u8 *)config_prop;

		msm_gpio->func = config[0];
		msm_gpio->drv  = config[1];
		msm_gpio->pull = config[2];
		msm_gpio->dir  = config[3];
		ret = 0;
	}
	return ret;
}

static __init int dt_read_pmic_gpio_config(struct device_node *parent,
			const char *property, int property_len,
			struct pm_gpio *pm_gpio)
{
	int ret = -1, len = 0;
	const void *config_prop;

	config_prop = of_get_property(parent, property, &len);
	if (config_prop && (len == property_len)) {
		u8 *config = (u8 *)config_prop;

		pm_gpio->direction	= config[0];
		pm_gpio->output_buffer	= config[1];
		pm_gpio->output_value	= config[2];
		pm_gpio->pull		= config[3];
		pm_gpio->vin_sel	= config[4];
		pm_gpio->out_strength	= config[5];
		pm_gpio->function	= config[6];
		pm_gpio->inv_int_pol	= config[7];
		pm_gpio->disable_pin	= config[8];
		ret = 0;
	}
	return ret;
}

static __init int dt_read_mpp_config(struct device_node *parent,
			const char *property, int property_len,
			struct pm8xxx_mpp_config_data *pmic_mpp)
{
	int ret = -1, len = 0;
	const void *config_prop;

	config_prop = of_get_property(parent, property, &len);
	if (config_prop && (len == property_len)) {
		u8 *config = (u8 *)config_prop;

		pmic_mpp->type    = config[0];
		pmic_mpp->level	  = config[1];
		pmic_mpp->control = config[2];
		ret = 0;
	}
	return ret;
}

static __init void config_EMU_detection_from_dt(void)
{
	struct device_node *parent, *child;
	char *name;
	int count = 0, len = 0;
	const void *prop;
	struct resource *res;

	parent = of_find_node_by_path("/System@0/EMUDetection@0");
	if (!parent)
		goto out;

	prop = of_get_property(parent, "ic-type", &len);
	if (prop && (len == sizeof(u8)))
		emu_det_dt_data.ic_type = *(u8 *)prop | 0xA0;

	prop = of_get_property(parent, "uart-gsbi", &len);
	if (prop && (len == sizeof(u8)))
		emu_det_dt_data.uart_gsbi = *(u8 *)prop;

	prop = of_get_property(parent, "accy-support", &len);
	if (prop && (len == sizeof(u8)))
		emu_det_dt_data.accy_support = *(u8 *)prop;

	prop = of_get_property(parent, "vdd-vreg-mv", &len);
	if (prop && (len == sizeof(u32)))
		emu_det_dt_data.vdd_voltage = *(u32 *)prop;

	prop = of_get_property(parent, "vdd-vreg", &len);
	if (prop && len) {
		name = kzalloc(len, GFP_KERNEL);
		if (name) {
			strlcpy(name, (const char *)prop, len);
			emu_det_dt_data.vdd_vreg = name;
		}
	}

	prop = of_get_property(parent, "usb-otg-vreg", &len);
	if (prop && len) {
		name = kzalloc(len, GFP_KERNEL);
		if (name) {
			strlcpy(name, (const char *)prop, len);
			emu_det_dt_data.usb_otg_vreg = name;
		}
	}

	/* count the child GPIO nodes */
	for_each_child_of_node(parent, child) {
		prop = of_get_property(child, "type", &len);
		if (prop && (len == sizeof(u32)) &&
			(*(u32 *)prop == 0x001E0010))
				count++;
	}

	if (count >= EMU_DET_RESOURCE_MAX) {
		pr_err("EMU resource: invalid number of resorces\n");
		goto out;
	}

	res = mmi_emu_det_resources;

	for_each_child_of_node(parent, child) {
		int gpio_idx;
		size_t ds;
		const void *info_prop;
		struct mmi_emu_det_gpio_data *emud;
		struct gpiomux_setting msm_gpio[2];
		struct pm_gpio pmic_gpio[2];
		struct pm8xxx_mpp_config_data pmic_mpp[2];

		info_prop = of_get_property(child, "gpio-info", &len);
		if (info_prop && (len == sizeof(u16))) {
			u8 *info = (u8 *)info_prop;

			if (info[0] == GPIO_IS_MSM) {
				res->start = res->end = info[1];
			} else if (info[0] == GPIO_IS_PMIC) {
				res->start = res->end =
					PM8921_GPIO_PM_TO_SYS(info[1]);
			} else if (info[0] == GPIO_IS_PMIC_MPP) {
				res->start = res->end =
					PM8921_MPP_PM_TO_SYS(info[1]);
			} else {
				pr_err("EMU unknown gpio %d owner %d; " \
					"skipping...\n", info[1], info[0]);
				continue;
			}

			prop = of_get_property(child, "name", &len);
			if (prop && len) {
				gpio_idx = -1;
				name = kzalloc(len, GFP_KERNEL);
				if (name) {
					strlcpy(name, (const char *)prop, len);
					res->name = (const char *)name;
				} else {
					pr_err("EMU unable to allocate memory\n");
					goto out;
				}
			} else {
				pr_err("EMU resource: unable to read name\n");
				goto out;
			}

			gpio_idx = mmi_emu_det_index_by_name(res->name);
			if (gpio_idx != -1)
				emud = &mmi_emu_det_gpio[gpio_idx];
			else {
				pr_err("EMU resource: unknown name: %s\n",
						res->name);
				goto out;
			}
			emud->gpio = res->start;
			emud->owner = info[0];

			switch (info[0]) {
			case GPIO_IS_MSM:
				ds = sizeof(struct gpiomux_setting);
				memset(msm_gpio, 0, sizeof(msm_gpio));
				if (!dt_read_msm_gpio_config(child,
					"msm-gpio-cfg-alt",
					sizeof(u8)*4, &msm_gpio[1]))
					emud->num_cfg++;

				if (!dt_read_msm_gpio_config(child,
					"msm-gpio-cfg",
					sizeof(u8)*4, &msm_gpio[0])) {
					emud->num_cfg++;
					emud->gpio_configs = kmemdup(msm_gpio,
						ds*emud->num_cfg, GFP_KERNEL);
				} else
					goto out;

					break;
			case GPIO_IS_PMIC:
				ds = sizeof(struct pm_gpio);
				memset(pmic_gpio, 0, sizeof(pmic_gpio));
				if (!dt_read_pmic_gpio_config(child,
					"pmic-gpio-cfg-alt",
					sizeof(u8)*9, &pmic_gpio[1]))
					emud->num_cfg++;

				if (!dt_read_pmic_gpio_config(child,
					"pmic-gpio-cfg",
					sizeof(u8)*9, &pmic_gpio[0])) {
					emud->num_cfg++;
					emud->gpio_configs = kmemdup(pmic_gpio,
						ds*emud->num_cfg, GFP_KERNEL);
				} else
					goto out;

					break;
			case GPIO_IS_PMIC_MPP:
				ds = sizeof(struct pm8xxx_mpp_config_data);
				memset(pmic_mpp, 0, sizeof(pmic_mpp));
				if (!dt_read_mpp_config(child,
					"pmic-mpp-cfg-alt",
					sizeof(u8)*3, &pmic_mpp[1]))
					emud->num_cfg++;

				if (!dt_read_mpp_config(child,
					"pmic-mpp-cfg",
					sizeof(u8)*3, &pmic_mpp[0])) {
					emud->num_cfg++;
					emud->gpio_configs = kmemdup(pmic_mpp,
						ds*emud->num_cfg, GFP_KERNEL);
				} else
					goto out;

					break;
			}
			res->flags = IORESOURCE_IO;
			res++;
		}
	}

	pr_info("EMU detection IC: %X, UART@GSBI%d, resources %d, " \
			"accy.support: %s\n",
			emu_det_dt_data.ic_type,
			emu_det_dt_data.uart_gsbi, count,
			emu_det_dt_data.accy_support ? "BASIC" : "FULL");

	mmi_emu_det_data.accy_support = emu_det_dt_data.accy_support;
	uart_over_gsbi12 = emu_det_dt_data.uart_gsbi == UART_GSBI12;

	mot_setup_whisper_clk();

	of_node_put(parent);
	return;
out:
	/* TODO: release allocated memory */
	/* disable EMU detection in case of error in device tree */
	otg_control_data = NULL;
	pr_err("EMU error reading devtree; EMU disabled\n");
	return;
}

static int mmi_dt_get_hdmi_feature(int *value)
{
	struct device_node *panel_node;
	const void *panel_prop;

	panel_node = of_find_node_by_path("/System@0/Feature@0");
	if (panel_node == NULL)
		return -ENODEV;

	panel_prop = of_get_property(panel_node, "feature_hdmi", NULL);
	if (panel_prop != NULL)
		 *value = *(u8 *)panel_prop;

	return 0;
}

int msm8960_headset_hw_has_gpio(int *hs_bias)
{
	struct device_node *node;
	const void *prop;
	int hs_hw_has_gpio = 1 , len = 0;

	node = of_find_node_by_path("/Chosen@0");
	if (node == NULL)
		return hs_hw_has_gpio;

	prop = of_get_property(node, "disable_headset_gpio", &len);
	if (prop && (len == sizeof(u8)) && (*(u8 *)prop))
		 hs_hw_has_gpio = 0;

	prop = of_get_property(node, "headset_bias", &len);
	if (prop && (len == sizeof(u32)))
		if( hs_bias ) *hs_bias = *(u32 *)prop;

	of_node_put(node);
	return hs_hw_has_gpio;
}

/*
 * HACK: Ideally all pinmuxes would be configured directly from the device
 * tree. Don't use this as a template for future device tree changes.
 */
static __init void config_mdp_vsync_from_dt(void)
{
	struct device_node *chosen;
	int len = 0;
	const void *prop;

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto out;

	prop = of_get_property(chosen, "use_mdp_vsync", &len);
	if (prop && (len == sizeof(u8)))
		use_mdp_vsync = *(u8*)prop ? MDP_VSYNC_ENABLED
			: MDP_VSYNC_DISABLED;

	of_node_put(chosen);

out:
	return;
}

/*
 * HACK: Ideally the complete keypad description could be pulled out of the
 * device tree. The LED configuration here should probably be moved elsewhere.
 * Don't use this as a template for future device tree changes.
 */
static __init void config_keyboard_from_dt(void)
{
	struct device_node *chosen;
	int len = 0;
	const void *prop;

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto out;

	prop = of_get_property(chosen, "qwerty_keyboard", &len);
	if (prop && (len == sizeof(u8)) && *(u8 *)prop) {
		keypad_data = &mmi_qwerty_keypad_data;
		keypad_mode = MMI_KEYPAD_RESET|MMI_KEYPAD_SLIDER;

		/* Enable keyboard backlight */
		strncpy((char *)&mp_lm3532_pdata.ctrl_b_name,
				"keyboard-backlight",
				sizeof(mp_lm3532_pdata.ctrl_b_name)-1);
		mp_lm3532_pdata.led2_controller = LM3532_CNTRL_B;
		mp_lm3532_pdata.ctrl_b_usage = LM3532_LED_DEVICE_FDBCK;
	}

	of_node_put(chosen);

out:
	return;
}

static __init u32 dt_get_u32_or_die(struct device_node *node, const char *name)
{
	int len = 0;
	const void *prop;

	prop = of_get_property(node, name, &len);
	BUG_ON(!prop || (len != sizeof(u32)));

	return *(u32 *)prop;
}

static __init u16 dt_get_u16_or_die(struct device_node *node, const char *name)
{
	int len = 0;
	const void *prop;

	prop = of_get_property(node, name, &len);
	BUG_ON(!prop || (len != sizeof(u16)));

	return *(u16 *)prop;
}

static __init u8 dt_get_u8_or_die(struct device_node *node, const char *name)
{
	int len = 0;
	const void *prop;

	prop = of_get_property(node, name, &len);
	BUG_ON(!prop || (len != sizeof(u8)));

	return *(u8 *)prop;
}

/* Temporary Workaround to get charging back working on all devices */
static __init void pm8921_mpps_w1_adjust_from_dt(void)
{
	struct device_node *chosen;
	int len = 0;
	const void *prop;
	int i;

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto out;

	prop = of_get_property(chosen, "old_w1_layout", &len);
	if (prop && (len == sizeof(u8)) && *(u8 *)prop) {
		for (i = 0; i < ARRAY_SIZE(pm8921_mpps); i++) {
			if (pm8921_mpps[i].mpp == PM8921_MPP_PM_TO_SYS(12)) {
				pm8921_mpps[3].config.control =
					PM8XXX_MPP_BI_PULLUP_1KOHM;
				pr_err("MPP 12 Turn on 1K Pullup\n");
			}
		}
	}


	of_node_put(chosen);

out:
	return;
}

static __init void load_pm8921_gpios_from_dt(void)
{
	struct device_node *parent, *child;
	int count = 0, index = 0;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent)
		goto out;

	/* count the child GPIO nodes */
	for_each_child_of_node(parent, child) {
		int len = 0;
		const void *prop;

		prop = of_get_property(child, "type", &len);
		if (prop && (len == sizeof(u32))) {
			/* must match type identifiers defined in DT schema */
			switch (*(u32 *)prop) {
			case 0x001E0006: /* Disable */
			case 0x001E0007: /* Output */
			case 0x001E0008: /* Input */
			case 0x001E0009: /* Output, Func */
			case 0x001E000A: /* Output, Vin */
			case 0x001E000B: /* Paired Input, Vin */
			case 0x001E000C: /* Paired Output, Vin */
				count++;
				break;
			}
		}
	}

	/* if no GPIO entries were found, just use the defaults */
	if (!count)
		goto out;

	/* make space for additional WCD9xxx reset GPIO added below */
	count++;
	/* allocate the space */
	pm8921_gpios = kmalloc(sizeof(struct pm8xxx_gpio_init) * count,
			GFP_KERNEL);
	pm8921_gpios_size = count;

	/* fill out the array */
	for_each_child_of_node(parent, child) {
		int len = 0;
		const void *type_prop;

		type_prop = of_get_property(child, "type", &len);
		if (type_prop && (len == sizeof(u32))) {
			const void *gpio_prop;
			u16 gpio;

			gpio_prop = of_get_property(child, "gpio", &len);
			if (!gpio_prop || (len != sizeof(u16)))
				continue;

			gpio = *(u16 *)gpio_prop;

			/* must match type identifiers defined in DT schema */
			switch (*(u32 *)type_prop) {
			case 0x001E0006: /* Disable */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_DISABLE(gpio);
				break;

			case 0x001E0007: /* Output */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_OUTPUT(gpio,
						dt_get_u16_or_die(child,
							"value"));
				break;

			case 0x001E0008: /* Input */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_INPUT(gpio,
						dt_get_u8_or_die(child,
							"pull"));
				break;

			case 0x001E0009: /* Output, Func */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_OUTPUT_FUNC(
						gpio, dt_get_u16_or_die(child,
							"value"),
						dt_get_u8_or_die(child,
							"func"));
				break;

			case 0x001E000A: /* Output, Vin */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_OUTPUT_VIN(
						gpio, dt_get_u16_or_die(child,
							"value"),
						dt_get_u8_or_die(child,
							"vin"));
				break;

			case 0x001E000B: /* Paired Input, Vin */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_PAIRED_IN_VIN(
						gpio, dt_get_u8_or_die(child,
							"vin"));
				break;

			case 0x001E000C: /* Paired Output, Vin */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_PAIRED_OUT_VIN(
						gpio, dt_get_u8_or_die(child,
							"vin"));
				break;
			}
		}
	}

	/* TABLA CODEC RESET - missing in DT */
	pm8921_gpios[index++] = (struct pm8xxx_gpio_init)
		PM8XXX_GPIO_OUTPUT_STRENGTH(34, 0, PM_GPIO_STRENGTH_MED);

	BUG_ON(index != count);

	of_node_put(parent);

out:
	return;
}

static __init void load_pm8921_rgb_leds_from_dt(void)
{
	int max_brightness = 0;
	struct device_node *parent;

	parent = of_find_node_by_path("/System@0/PowerIC@0/RGBLED@0");
	if (parent) {
		max_brightness = dt_get_u32_or_die(parent, "max_brightness");
		if (max_brightness)
			pm8xxx_rgb_leds_pdata.max_brightness = max_brightness;
	}
	of_node_put(parent);

	return;
}

static __init void load_pm8921_leds_from_dt(void)
{
	struct device_node *parent, *child;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent)
		goto out;

	for_each_child_of_node(parent, child) {
		int len = 0;
		const void *prop;

		prop = of_get_property(child, "type", &len);
		if (prop && (len == sizeof(u32))) {
			/* Qualcomm_PM8921_LED as defined in DT schema */
			if (0x001E000E == *(u32 *)prop) {
				unsigned index;
				struct led_info *led_info;

				index = dt_get_u32_or_die(child, "index");

				led_info = kzalloc(sizeof(struct led_info),
						GFP_KERNEL);
				BUG_ON(!led_info);

				prop = of_get_property(child, "name", &len);
				BUG_ON(!prop);

				led_info->name = kstrndup((const char *)prop,
						len, GFP_KERNEL);
				BUG_ON(!led_info->name);

				prop = of_get_property(child,
						"default_trigger", &len);
				BUG_ON(!prop);

				led_info->default_trigger = kstrndup(
						(const char *)prop,
						len, GFP_KERNEL);
				BUG_ON(!led_info->default_trigger);

				pm8xxx_set_led_info(index, led_info);
			}
		}
	}

	of_node_put(parent);

out:
	return;
}

static __init void register_i2c_devices_from_dt(int bus)
{
	char path[18];
	struct device_node *parent, *child;

	snprintf(path, sizeof(path), "/System@0/I2C@%d", bus);

	parent = of_find_node_by_path(path);
	if (!parent)
		goto out;

	for_each_child_of_node(parent, child) {
		struct i2c_board_info info;
		int len = 0;
		const void *prop;
		int err = 0;

		memset(&info, 0, sizeof(struct i2c_board_info));

		prop = of_get_property(child, "i2c,type", &len);
		if (prop)
			strncpy(info.type, (const char *)prop,
					len > I2C_NAME_SIZE ? I2C_NAME_SIZE :
					len);

		prop = of_get_property(child, "i2c,address", &len);
		if (prop && (len == sizeof(u32)))
			info.addr = *(u32 *)prop;

		prop = of_get_property(child, "irq,gpio", &len);
		if (prop && (len == sizeof(u8)))
			info.irq = MSM_GPIO_TO_INT(*(u8 *)prop);

		/*
		 * HACK: Teufel has different i2c devices depending on the
		 * display.
		 *
		 * teufel-hack possible values as defined in device tree
		 * schema:
		 *	0 - don't care
		 *	1 - AUO display
		 *	2 - SMD display
		 */
		prop = of_get_property(child, "teufel-hack", &len);
		if (prop && (len == sizeof(u8)) && *(u8 *)prop) {
			if (is_smd_hd_465() && (*(u8 *)prop != 2))
				continue;
			if (!is_smd_hd_465() && (*(u8 *)prop != 1))
				continue;
		}

		prop = of_get_property(child, "platform_data", &len);
		if (prop && len) {
			info.platform_data = kmemdup(prop, len, GFP_KERNEL);
			BUG_ON(!info.platform_data);
		}

		prop = of_get_property(child, "type", &len);
		if (prop && (len == sizeof(u32))) {
			/* must match type identifiers defined in DT schema */
			switch (*(u32 *)prop) {
			case 0x00040002: /* Cypress_CYTTSP3 */
				/* Check if this is bare board and don't set
				 * up driver if it is */
				if (bare_board == 1) {
					err = -1;
					pr_err(
						"%s: NO TOUCH(bare board)!\n",
						__func__);
				} else {
					err = mot_setup_touch_cyttsp3(&info,
							child);
				}
				break;

			case 0x000B0003: /* National_LM3559 */
				prop = of_get_property(child, "enable_gpio",
						&len);
				if (prop && (len == sizeof(u32)))
					camera_flash_3559.hw_enable =
						*(u32 *)prop;
				info.platform_data = &camera_flash_3559;
				break;

			case 0x000B0004: /* National_LM3532 */
				prop = of_get_property(child, "led2_controller",
						&len);
				if (prop && (len == sizeof(u8)))
					mp_lm3532_pdata.led2_controller =
						*(u8 *)prop;
				prop = of_get_property(child, "ctrl_a_fsc",
						&len);
				if (prop && (len == sizeof(u8)))
					mp_lm3532_pdata.ctrl_a_fsc =
						*(u8 *)prop;
				prop = of_get_property(child, "ctrl_b_fsc",
						&len);
				if (prop && (len == sizeof(u8)))
					mp_lm3532_pdata.ctrl_b_fsc =
						*(u8 *)prop;
				prop = of_get_property(child, "ctrl_c_fsc",
						&len);
				if (prop && (len == sizeof(u8)))
					mp_lm3532_pdata.ctrl_c_fsc =
						*(u8 *)prop;
				prop = of_get_property(child, "pwm_disable_threshold",
						&len);
				if (prop && (len == sizeof(u8)))
					mp_lm3532_pdata.pwm_disable_threshold =
						*(u8 *)prop;
				info.platform_data = &mp_lm3532_pdata;
				break;

			case 0x000B0006: /* National_LM3556 */
				prop = of_get_property(child, "enable_gpio",
						&len);
				if (prop && (len == sizeof(u32)))
					cam_flash_3556.hw_enable =
						*(u32 *)prop;

				prop = of_get_property(child,
						"current_cntrl_reg_val",
						&len);
				if (prop && (len == sizeof(u8)))
					cam_flash_3556.current_cntrl_reg_def =
						*(u8 *)prop;
				info.platform_data = &cam_flash_3556;
				break;

			case 0x00250001: /* TAOS_CT406 */
				ct406_init(&info, child);
				break;

			case 0x00260001: /* Atmel_MXT */
				/* Check if this is bare board and don't set
				 * up driver if it is */
				if (bare_board == 1) {
					err = -1;
					pr_err(
						"%s: NO TOUCH(bare board)!\n",
						__func__);
				} else {
					err = mot_setup_touch_atmxt(&info,
							child);
				}
				break;

			case 0x00270000: /* Melfas_MMS100 */
				/* Check if this is bare board and don't set
				 * up driver if it is */
				if (bare_board == 1) {
					err = -1;
					pr_err(
						"%s: NO TOUCH(bare board)!\n",
						__func__);
				} else {
					info.platform_data = &touch_pdata;
					melfas_ts_platform_init();
				}
				break;

			case 0x00280000: /* Aptina_MT9M114 */
				info.platform_data =
					&msm_camera_sensor_mt9m114_data;

				prop = of_get_property(child, "mclk_freq",
						&len);
				if (prop && (len == sizeof(u32)))
					msm_camera_sensor_mt9m114_data.
						sensor_platform_info->
						mclk_freq = *(int *)prop;
				else
					msm_camera_sensor_mt9m114_data.
						sensor_platform_info->
						mclk_freq = 24000000;
				break;

			case 0x00290000: /* Omnivision_OV8820 */
				prop = of_get_property(child, "1p8_via_gpio",
						&len);
				if (prop && (len == sizeof(u8)) && *(u8 *)prop)
					msm_camera_sensor_ov8820_data.
						sensor_platform_info->
						reg_1p8 = NULL;
				else
					msm_camera_sensor_ov8820_data.
						sensor_platform_info->
						digital_en = 0;
				prop = of_get_property(child, "drv_strength",
						&len);
				if (prop && (len == sizeof(u8)))
					update_camera_gpio_cfg(
						msm_camera_sensor_ov8820_data,
						*(uint8_t *)prop);
				info.platform_data =
					&msm_camera_sensor_ov8820_data;
				break;
			case 0x00290001: /* Omnivision_OV7736 */
				prop = of_get_property(child, "drv_strength",
						&len);
				if (prop && (len == sizeof(u8)))
					update_camera_gpio_cfg(
						msm_camera_sensor_ov7736_data,
						*(uint8_t *)prop);
				info.platform_data =
					&msm_camera_sensor_ov7736_data;
				break;
			case 0x00090007: /* Samsung_S5K5B3G */
				info.platform_data =
					&msm_camera_sensor_s5k5b3g_data;
				break;
			case 0x00030014: /* TexasInstruments, TMP105 */
				msm8960_tmp105_init(&info, child);
				break;
			}
		}

		if (err >= 0)
			i2c_register_board_info(bus, &info, 1);
	}

	of_node_put(parent);

out:
	return;
}

static void __init register_i2c_devices(void)
{
	register_i2c_devices_from_dt(MSM_8960_GSBI3_QUP_I2C_BUS_ID);
	register_i2c_devices_from_dt(MSM_8960_GSBI4_QUP_I2C_BUS_ID);
	register_i2c_devices_from_dt(MSM_8960_GSBI10_QUP_I2C_BUS_ID);
}

static unsigned sdc_detect_gpio = 20;

static __init void config_sdc_from_dt(void)
{
	struct device_node *node;
	int len = 0;
	const void *prop;

	node = of_find_node_by_path("/System@0/SDHC@0/SDHCSLOT@3");
	if (!node)
		goto out;

	prop = of_get_property(node, "pm8921,gpio", &len);
	if (prop && (len == sizeof(u32)))
		sdc_detect_gpio = *(u32 *)prop;

	of_node_put(node);

out:
	return;
}

static struct gpiomux_setting mdp_disp_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdp_disp_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm8960_mdp_5v_en_configs[] __initdata = {
	{
		.gpio = 43,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mdp_disp_active_cfg,
			[GPIOMUX_SUSPENDED] = &mdp_disp_suspend_cfg,
		},
	}
};

#ifdef CONFIG_INPUT_GPIO
static struct gpiomux_setting slide_det_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config mot_slide_detect_configs[] __initdata = {
	{
		.gpio = 11,
		.settings = {
			[GPIOMUX_ACTIVE]    = &slide_det_cfg,
			[GPIOMUX_SUSPENDED] = &slide_det_cfg,
		},
	},
};
#endif

#ifdef CONFIG_VIB_TIMED
/* vibrator GPIO configuration */
static struct gpiomux_setting vib_setting_suspended = {
		.func = GPIOMUX_FUNC_GPIO, /*suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting vib_setting_active = {
		.func = GPIOMUX_FUNC_GPIO, /*active*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm8960_vib_configs[] = {
	{
		.gpio = 79,
		.settings = {
			[GPIOMUX_ACTIVE]    = &vib_setting_active,
			[GPIOMUX_SUSPENDED] = &vib_setting_suspended,
		},
	},
	{
		.gpio = 47,
		.settings = {
			[GPIOMUX_ACTIVE]    = &vib_setting_active,
			[GPIOMUX_SUSPENDED] = &vib_setting_suspended,
		},
	},
};
#endif

static struct gpiomux_setting batt_eprom_setting_suspended = {
		.func = GPIOMUX_FUNC_GPIO, /*suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting batt_eprom_setting_active = {
		.func = GPIOMUX_FUNC_GPIO, /*active*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm8960_batt_eprom_configs[] = {
	{
		.gpio = BATT_EPROM_GPIO,
		.settings = {
			[GPIOMUX_ACTIVE]    = &batt_eprom_setting_active,
			[GPIOMUX_SUSPENDED] = &batt_eprom_setting_suspended,
		},
	},
};

struct gpiomux_settings {
	u16 gpio;
	u8 setting;
	u8 func;
	u8 pull;
	u8 drv;
	u8 dir;
} __attribute__ ((__packed__));

#define DT_PATH_MUX		"/System@0/IOMUX@0"
#define DT_PROP_MUX_SETTINGS	"settings"

static void __init mot_gpiomux_init(unsigned kp_mode)
{
	struct device_node *node;
	const struct gpiomux_settings *prop;
	int i, size = 0;
	struct gpiomux_setting setting, old_setting;

	msm_gpiomux_install(msm8960_mdp_5v_en_configs,
			ARRAY_SIZE(msm8960_mdp_5v_en_configs));
#ifdef CONFIG_INPUT_GPIO
	if (kp_mode & MMI_KEYPAD_SLIDER)
		msm_gpiomux_install(mot_slide_detect_configs,
				ARRAY_SIZE(mot_slide_detect_configs));
#endif
#ifdef CONFIG_VIB_TIMED
	msm_gpiomux_install(msm8960_vib_configs,
			ARRAY_SIZE(msm8960_vib_configs));
#endif
	msm_gpiomux_install(msm8960_batt_eprom_configs,
			ARRAY_SIZE(msm8960_batt_eprom_configs));

	/* Override the default setting by devtree */
	node = of_find_node_by_path(DT_PATH_MUX);
	if (!node) {
		pr_info("%s: no node found: %s\n", __func__, DT_PATH_MUX);
		return;
	}
	prop = (const void *)of_get_property(node, DT_PROP_MUX_SETTINGS, &size);
	if (prop && ((size % sizeof(struct gpiomux_settings)) == 0)) {
		for (i = 0; i < size / sizeof(struct gpiomux_settings); i++) {
			setting.func = prop->func;
			setting.drv = prop->drv;
			setting.pull = prop->pull;
			setting.dir = prop->dir;
			if (msm_gpiomux_write(prop->gpio, prop->setting,
						&setting, &old_setting))
				pr_err("%s: gpio%d mux setting %d failed\n",
					__func__, prop->gpio, prop->setting);

			prop++;
		}
	}

	of_node_put(node);
}

static void __init mot_init_factory_kill(void)
{
	struct device_node *chosen;
	int len = 0, enable = 1, rc;
	const void *prop;

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto out;

	prop = of_get_property(chosen, "factory_kill_disable", &len);
	if (prop && (len == sizeof(u8)) && *(u8 *)prop)
		enable = 0;

	rc = gpio_request(75, "Factory Kill Disable");
	if (rc) {
		pr_err("%s: GPIO request failure\n", __func__);
		goto putnode;
	}

	rc = gpio_direction_output(75, enable);
	if (rc) {
		pr_err("%s: GPIO configuration failure\n", __func__);
		goto gpiofree;
	}

	rc = gpio_export(75, 0);

	if (rc) {
		pr_err("%s: GPIO export failure\n", __func__);
		goto gpiofree;
	}

	pr_info("%s: Factory Kill Circuit: %s\n", __func__,
		(enable ? "enabled" : "disabled"));

	return;

gpiofree:
	gpio_free(75);
putnode:
	of_node_put(chosen);
out:
	return;
}

static void __init msm8960_init_watchdog(void)
{
	struct msm_watchdog_pdata *mmi_watchdog_pdata = msm8960_device_watchdog.dev.platform_data;

	/* Re-init pet time to half of bark time. */
	mmi_watchdog_pdata->bark_time = 41000;
	mmi_watchdog_pdata->pet_time = 21000;
}

static int mot_tcmd_export_gpio(void)
{
	int rc;

	rc = gpio_request(1, "USB_HOST_EN");
	if (!rc) {
		gpio_direction_output(1, 0);
		rc = gpio_export(1, 0);
		if (rc) {
			pr_err("%s: GPIO USB_HOST_EN export failure\n", __func__);
			gpio_free(1);
		}
	}

	rc = gpio_request(PM8921_GPIO_PM_TO_SYS(36), "SIM_DET");
	if (!rc) {
		gpio_direction_input(PM8921_GPIO_PM_TO_SYS(36));
		rc = gpio_export(PM8921_GPIO_PM_TO_SYS(36), 0);
		if (rc) {
			pr_err("%s: GPIO SIM_DET export failure\n", __func__);
			gpio_free(PM8921_GPIO_PM_TO_SYS(36));
		}
	}

	/* RF connection detect GPIOs */
	rc = gpio_request(24, "RF_CONN_DET_2G3G");
	if (!rc) {
		gpio_direction_input(24);
		rc = gpio_export(24, 0);
		if (rc) {
			pr_err("%s: GPIO 24 export failure\n", __func__);
			gpio_free(24);
		}
	}

	rc = gpio_request(25, "RF_CONN_DET_LTE_1");
	if (!rc) {
		gpio_direction_input(25);
		rc = gpio_export(25, 0);
		if (rc) {
			pr_err("%s: GPIO 25 export failure\n", __func__);
			gpio_free(25);
		}
	}

	rc = gpio_request(81, "RF_CONN_DET_LTE_2");
	if (!rc) {
		gpio_direction_input(81);
		rc = gpio_export(81, 0);
		if (rc) {
			pr_err("%s: GPIO 81 export failure\n", __func__);
			gpio_free(81);
		}
	}
	return 0;
}

#ifdef CONFIG_PM8921_FACTORY_SHUTDOWN
static void mot_factory_reboot_callback(void)
{
	pr_err("%s: configuring GPIO for factory shutdown\n", __func__);
	emu_mux_ctrl_config_pin(EMU_MUX_CTRL0_GPIO, 0);
	emu_mux_ctrl_config_pin(EMU_MUX_CTRL1_GPIO, 0);
}
static void (*reboot_ptr)(void) = &mot_factory_reboot_callback;
#else
static void (*reboot_ptr)(void);
#endif

static struct msm_spi_platform_data msm8960_qup_spi_gsbi1_pdata = {
	.max_clock_speed = 15060000,
};

#define EXPECTED_MBM_PROTOCOL_VERSION 1
static uint32_t mbm_protocol_version;

/* This sysfs allows sensor TCMD to switch the control of I2C-12
 *  from DSPS to Krait at runtime by issuing the following command:
 *	echo 1 > /sys/kernel/factory_gsbi12_mode/install
 * Upon phone reboot, everything will be back to normal.
 */
static ssize_t factory_gsbi12_mode_install_set(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int Error;

	Error = platform_device_register(&msm8960_device_qup_i2c_gsbi12);

	if (Error)
	  printk(KERN_ERR "factory_gsbi12_mode_install_set: failed to register gsbi12\n");

	/* We must return # of bytes used from buffer
	(do not return 0,it will throw an error) */
	return count;
}

static struct kobj_attribute factory_gsbi12_mode_install_attribute =
	__ATTR(install, S_IRUGO|S_IWUSR, NULL, factory_gsbi12_mode_install_set);
static struct kobject *factory_gsbi12_mode_kobj;

static int sysfs_factory_gsbi12_mode_init(void)
{
	int retval;

	/* creates a new folder(node) factory_gsbi12_mode under /sys/kernel */
	factory_gsbi12_mode_kobj = kobject_create_and_add("factory_gsbi12_mode", kernel_kobj);
	if (!factory_gsbi12_mode_kobj)
		return -ENOMEM;

	/* creates a file named install under /sys/kernel/factory_gsbi12_mode */
	retval = sysfs_create_file(factory_gsbi12_mode_kobj,
				&factory_gsbi12_mode_install_attribute.attr);
	if (retval)
		kobject_put(factory_gsbi12_mode_kobj);

	return retval;
}

static char acpu_type[20];

static int panic_acpu_handler(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	printk(KERN_EMERG"CPU type is %s", acpu_type);

	return NOTIFY_DONE;
}

static struct notifier_block panic_acpu = {
	.notifier_call	= panic_acpu_handler,
};


static int __init msm8960_get_acputype(void)
{
/* PTE EFUSE register. */
#define QFPROM_PTE_EFUSE_ADDR	(MSM_QFPROM_BASE + 0x00C0)

	uint32_t pte_efuse, pvs;

	pte_efuse = readl_relaxed(QFPROM_PTE_EFUSE_ADDR);

	pvs = (pte_efuse >> 10) & 0x7;
	if (pvs == 0x7)
		pvs = (pte_efuse >> 13) & 0x7;

	switch (pvs) {
	case 0x0:
	case 0x7:
		snprintf(acpu_type, 15, "ACPU PVS: Slow\n");
		break;
	case 0x1:
		snprintf(acpu_type, 18, "ACPU PVS: Nominal\n");
		break;
	case 0x3:
		snprintf(acpu_type, 15, "ACPU PVS: Fast\n");
		break;
	default:
		snprintf(acpu_type, 18, "ACPU PVS: Unknown\n");
		break;
	}

	atomic_notifier_chain_register(&panic_notifier_list,
			       &panic_acpu);

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	ram_console_ext_oldbuf_print("CPU type is %s", acpu_type);
#endif

	return 0;
}

core_initcall(msm8960_get_acputype);



static void __init msm8960_mmi_init(void)
{
	if (mbm_protocol_version == 0)
		pr_err("ERROR: ATAG MBM_PROTOCOL_VERSION is not present."
			" Bootloader update is required\n");
	else if (EXPECTED_MBM_PROTOCOL_VERSION != mbm_protocol_version)
		arch_reset(0, "mbm_proto_ver_mismatch");

	if (meminfo_init(SYS_MEMORY, SZ_256M) < 0)
		pr_err("meminfo_init() failed!\n");

	msm8960_init_tsens();
	msm8960_init_rpm();
	msm8960_init_sleep_status();

	msm_init_apanic();

	config_keyboard_from_dt();

	config_EMU_detection_from_dt();

	/* load panel_name from device tree, if present */
	load_panel_name_from_dt();

	/* configure pm8921 leds */
	load_pm8921_leds_from_dt();

	/* configure pm8921 RGB LED */
	load_pm8921_rgb_leds_from_dt();

	/* needs to happen before msm_clock_init */
	config_camera_single_mclk_from_dt();

	/* check for device specific ulpi phy init sequence */
	config_ulpi_from_dt();

	pmic_reset_irq = PM8921_IRQ_BASE + PM8921_RESOUT_IRQ;
	regulator_suppress_info_printing();
	if (msm_xo_init())
		pr_err("Failed to initialize XO votes\n");
	msm8960_init_regulators();
	msm_clock_init(&msm8960_clock_init_data);

	config_mdp_vsync_from_dt();
	gpiomux_init(use_mdp_vsync);
	mot_gpiomux_init(keypad_mode);
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	msm8960_init_hdmi(&hdmi_msm_device, &hdmi_msm_data);
#endif

	pm8921_init(keypad_data, boot_mode_is_factory(), 0, 45,
		    reboot_ptr, battery_data_is_meter_locked(),
		    get_hot_temp_dt(),  get_hot_offset_dt(),
		    get_hot_temp_pcb_dt(), get_hot_pcb_offset_dt());

	/* Init the bus, but no devices at this time */
	msm8960_spi_init(&msm8960_qup_spi_gsbi1_pdata, NULL, 0);

	msm8960_init_watchdog();
	msm8960_i2c_init(400000, uart_over_gsbi12);
	msm8960_gfx_init();
	msm8960_spm_init();
	msm8960_init_buses();

#ifdef CONFIG_VIB_TIMED
	mmi_vibrator_init();
#endif
	mmi_keypad_init(keypad_mode);

	platform_add_devices(msm_footswitch_devices,
		msm_num_footswitch_devices);
	msm8960_add_common_devices(msm_fb_detect_panel);

	load_pm8921_gpios_from_dt();
	pm8921_mpps_w1_adjust_from_dt();
	pm8921_gpio_mpp_init(pm8921_gpios, pm8921_gpios_size,
							pm8921_mpps, ARRAY_SIZE(pm8921_mpps));
	mot_init_factory_kill();
	msm8960_init_usb();

	mot_init_emu_detection(otg_control_data);

	platform_add_devices(mmi_devices, ARRAY_SIZE(mmi_devices));

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	mmi_dt_get_hdmi_feature(&mmi_feature_hdmi);
	if (mmi_feature_hdmi)
		platform_add_devices(mmi_hdmi_devices, ARRAY_SIZE(mmi_hdmi_devices));
#endif

#ifdef CONFIG_MSM_CAMERA
	msm8960_init_cam();
#endif
	config_sdc_from_dt();
	msm8960_init_mmc(sdc_detect_gpio);
	acpuclk_init(&acpuclk_8960_soc_data);
	register_i2c_devices();
	msm_fb_add_devices();
	msm8960_init_slim();
	msm8960_init_dsps();
	if (!uart_over_gsbi12)
		msm8960_init_gsbi4();
	sysfs_factory_gsbi12_mode_init();

	msm8960_pm_init(RPM_APCC_CPU0_WAKE_UP_IRQ);
	mot_tcmd_export_gpio();

	change_memory_power = &msm8960_change_memory_power;
	BUG_ON(msm_pm_boot_init(&msm_pm_boot_pdata));

	init_mmi_unit_info();
	init_mmi_ram_info();
	init_sysfs_extended_baseband();
	emmc_version_init();
	hw_rev_txt_init();

	enable_ext_5v_reg();
}

static int __init mot_parse_atag_display(const struct tag *tag)
{
	const struct tag_display *display_tag = &tag->u.display;
	strncpy(panel_name, display_tag->display, PANEL_NAME_MAX_LEN);
	panel_name[PANEL_NAME_MAX_LEN] = '\0';
	pr_info("%s: %s\n", __func__, panel_name);
	return 0;
}
__tagtable(ATAG_DISPLAY, mot_parse_atag_display);

static int __init mot_parse_atag_mbm_protocol_version(const struct tag *tag)
{
	const struct tag_mbm_protocol_version *mbm_protocol_version_tag =
		&tag->u.mbm_protocol_version;
	pr_info("%s: 0x%x\n", __func__, mbm_protocol_version_tag->value);
	mbm_protocol_version = mbm_protocol_version_tag->value;
	return 0;
}
__tagtable(ATAG_MBM_PROTOCOL_VERSION, mot_parse_atag_mbm_protocol_version);

static int __init mot_parse_atag_baseband(const struct tag *tag)
{
	const struct tag_baseband *baseband_tag = &tag->u.baseband;
	strncpy(extended_baseband, baseband_tag->baseband, BASEBAND_MAX_LEN);
	extended_baseband[BASEBAND_MAX_LEN] = '\0';
	pr_info("%s: %s\n", __func__, extended_baseband);
	return 0;
}
__tagtable(ATAG_BASEBAND, mot_parse_atag_baseband);

/* process flat device tree for hardware configuration */
static int __init parse_tag_flat_dev_tree_address(const struct tag *tag)
{
	struct tag_flat_dev_tree_address *fdt_addr =
		(struct tag_flat_dev_tree_address *)&tag->u.fdt_addr;

	if (fdt_addr->size) {
		fdt_start_address = (u32)phys_to_virt(fdt_addr->address);
		fdt_size = fdt_addr->size;
	}

	pr_info("flat_dev_tree_address=0x%08x, flat_dev_tree_size == 0x%08X\n",
			fdt_addr->address, fdt_addr->size);

	return 0;
}
__tagtable(ATAG_FLAT_DEV_TREE_ADDRESS, parse_tag_flat_dev_tree_address);

static void __init mmi_init_early(void)
{
	msm8960_allocate_memory_regions();

	if (fdt_start_address) {
		void *mem;
		mem = __alloc_bootmem(fdt_size, __alignof__(int), 0);
		BUG_ON(!mem);
		memcpy(mem, (const void *)fdt_start_address, fdt_size);
		initial_boot_params = (struct boot_param_header *)mem;
		pr_info("Unflattening device tree: 0x%08x\n", (u32)mem);
		unflatten_device_tree();
	}
}

static void __init msm8960_fixup(struct machine_desc *desc,
		struct tag *tags, char **cmdline, struct meminfo *mi)
{
	struct	tag	*t;

	bare_board = 0;
	/* Since command line is not available at this time, we have to parse
	 * the atags, specifially atag_cmdline.
	 */
	for (t = tags; t->hdr.size; t = tag_next(t)) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			if (strstr(t->u.cmdline.cmdline, "bare_board=1")) {
				bare_board = 1;
			}
			break;
		}
	}
}

static void (*msm8960_common_cal_rsv_sizes)(void) __initdata;

static void __init msm8960_mmi_cal_rsv_sizes(void)
{
	if (msm8960_common_cal_rsv_sizes)
		msm8960_common_cal_rsv_sizes();
	reserve_memory_for_watchdog();
}

static void __init msm8960_mmi_reserve(void)
{
	msm8960_common_cal_rsv_sizes = reserve_info->calculate_reserve_sizes;
	reserve_info->calculate_reserve_sizes = msm8960_mmi_cal_rsv_sizes;
	msm8960_reserve();
}

MACHINE_START(TEUFEL, "Teufel")
	.map_io = msm8960_map_io,
	.reserve = msm8960_mmi_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_mmi_init,
	.init_early = mmi_init_early,
	.init_very_early = msm8960_early_memory,
	.fixup = msm8960_fixup,
MACHINE_END

MACHINE_START(QINARA, "Qinara")
	.map_io = msm8960_map_io,
	.reserve = msm8960_mmi_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_mmi_init,
	.init_early = mmi_init_early,
	.init_very_early = msm8960_early_memory,
	.fixup = msm8960_fixup,
MACHINE_END

MACHINE_START(VANQUISH, "Vanquish")
	.map_io = msm8960_map_io,
	.reserve = msm8960_mmi_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_mmi_init,
	.init_early = mmi_init_early,
	.init_very_early = msm8960_early_memory,
	.fixup = msm8960_fixup,
MACHINE_END

MACHINE_START(BECKER, "Becker")
	.map_io = msm8960_map_io,
	.reserve = msm8960_mmi_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_mmi_init,
	.init_early = mmi_init_early,
	.init_very_early = msm8960_early_memory,
	.fixup = msm8960_fixup,
MACHINE_END

MACHINE_START(ASANTI, "Asanti")
	.map_io = msm8960_map_io,
	.reserve = msm8960_mmi_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_mmi_init,
	.init_early = mmi_init_early,
	.init_very_early = msm8960_early_memory,
	.fixup = msm8960_fixup,
MACHINE_END

MACHINE_START(ORDOG, "Ordog")
	.map_io = msm8960_map_io,
	.reserve = msm8960_mmi_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_mmi_init,
	.init_early = mmi_init_early,
	.init_very_early = msm8960_early_memory,
MACHINE_END

/* for use by products that are completely configured through device tree */
MACHINE_START(MSM8960DT, "msm8960dt")
	.map_io = msm8960_map_io,
	.reserve = msm8960_mmi_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_mmi_init,
	.init_early = mmi_init_early,
	.init_very_early = msm8960_early_memory,
	.fixup = msm8960_fixup,
MACHINE_END
