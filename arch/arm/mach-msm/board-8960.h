/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_ARM_MACH_MSM_BOARD_MSM8960_H
#define __ARCH_ARM_MACH_MSM_BOARD_MSM8960_H

#include <linux/regulator/gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/i2c.h>
#include <linux/i2c/sx150x.h>
#include <mach/irqs.h>
#include <mach/msm_spi.h>
#include <mach/rpm-regulator.h>
#include <linux/spi/spi.h>
#include <mach/board.h>
#include <linux/leds.h>
#include <mach/mdm2.h>
#include <mach/msm_memtypes.h>

/* Macros assume PMIC GPIOs and MPPs start at 1 */
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
#define PM8921_MPP_BASE			(PM8921_GPIO_BASE + PM8921_NR_GPIOS)
#define PM8921_MPP_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_MPP_BASE)
#define PM8921_IRQ_BASE			(NR_MSM_IRQS + NR_GPIO_IRQS)

extern struct pm8xxx_regulator_platform_data
	msm_pm8921_regulator_pdata[] __devinitdata;

extern int msm_pm8921_regulator_pdata_len __devinitdata;

#define MDM2AP_ERRFATAL			70
#define AP2MDM_ERRFATAL			95
#define MDM2AP_STATUS			69
#define AP2MDM_STATUS			94
#define AP2MDM_PMIC_RESET_N		80
#define AP2MDM_KPDPWR_N			81

#define GPIO_VREG_ID_EXT_5V		0
#define GPIO_VREG_ID_EXT_L2		1
#define GPIO_VREG_ID_EXT_3P3V		2
#define GPIO_VREG_ID_EXT_OTG_SW		3

#ifdef CONFIG_FB_MSM_OVERLAY0_WRITEBACK
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((1920 * 1200 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY0_WRITEBACK */

#ifdef CONFIG_FB_MSM_OVERLAY1_WRITEBACK
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE roundup((1920 * 1088 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY1_WRITEBACK */

#define MDP_VSYNC_GPIO 0

#define HAP_SHIFT_LVL_OE_GPIO	47

#define MDP_VSYNC_ENABLED	true
#define MDP_VSYNC_DISABLED	false

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((1280 * 736 * 4), 4096) * 3) /* 4 bpp x 3 pages */
#else
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((1280 * 736 * 4), 4096) * 2) /* 4 bpp x 2 pages */
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
#define MSM_FB_EXT_BUF_SIZE \
		(roundup((1920 * 1088 * 2), 4096) * 1) /* 2 bpp x 1 page */
#elif defined(CONFIG_FB_MSM_TVOUT)
#define MSM_FB_EXT_BUF_SIZE \
		(roundup((720 * 576 * 2), 4096) * 2) /* 2 bpp x 2 pages */
#else
#define MSM_FB_EXT_BUF_SIZE	0
#endif

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
#define MSM_FB_WRITEBACK_BUF_SIZE \
		(roundup((1280 * 720 * 2), 4096)) /* 2 bpp x 1 page */
#else
#define MSM_FB_WRITEBACK_BUF_SIZE 0
#endif

/* Note: must be multiple of 4096 */
#define MSM_FB_SIZE roundup(MSM_FB_PRIM_BUF_SIZE + \
			MSM_FB_EXT_BUF_SIZE + MSM_FB_WRITEBACK_BUF_SIZE, 4096)

#ifdef CONFIG_I2C

#define MSM_8960_GSBI4_QUP_I2C_BUS_ID 4
#define MSM_8960_GSBI3_QUP_I2C_BUS_ID 3
#define MSM_8960_GSBI10_QUP_I2C_BUS_ID 10

#endif

#define MSM_PMEM_ADSP_SIZE         0x6E00000 /* Need to be multiple of 64K */
#define MSM_PMEM_AUDIO_SIZE        0x4CF000
#define MSM_PMEM_SIZE 0x4600000 /* 70 Mbytes */
#define MSM_LIQUID_PMEM_SIZE 0x4000000 /* 64 Mbytes */
#define MSM_HDMI_PRIM_PMEM_SIZE 0x4000000 /* 64 Mbytes */
#define MSM_RAM_CONSOLE_SIZE       128 * SZ_1K

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
#define MSM_PMEM_KERNEL_EBI1_SIZE  0x280000
#ifdef CONFIG_MSM_IOMMU
#define MSM_ION_MM_SIZE		0x3800000
#define MSM_ION_SF_SIZE		0x0
#define MSM_ION_QSECOM_SIZE	0x380000 /* (3.5MB) - can't fallback to SF */
#define MMI_MSM_ION_HEAP_NUM	7
#else
#define MSM_ION_MM_SIZE		MSM_PMEM_ADSP_SIZE
#define MSM_ION_SF_SIZE		MSM_PMEM_SIZE
#define MSM_ION_QSECOM_SIZE	0x200000 /* (2MB) */
#define MMI_MSM_ION_HEAP_NUM	8
#endif
#define MSM_ION_MM_FW_SIZE	0x200000 /* (2MB) */
#define MSM_ION_MFC_SIZE	SZ_8K
#define MSM_ION_AUDIO_SIZE	MSM_PMEM_AUDIO_SIZE

#ifdef CONFIG_KERNEL_PMEM_AUDIO_MMI
 #define MSM_ION_HEAP_NUM	(MMI_MSM_ION_HEAP_NUM-1)
#else
 #define MSM_ION_HEAP_NUM	(MMI_MSM_ION_HEAP_NUM)
#endif

#define MSM_LIQUID_ION_MM_SIZE (MSM_ION_MM_SIZE + 0x600000)
#define MSM_LIQUID_ION_SF_SIZE MSM_LIQUID_PMEM_SIZE
#define MSM_HDMI_PRIM_ION_SF_SIZE MSM_HDMI_PRIM_PMEM_SIZE

#define MSM8960_FIXED_AREA_START 0xb0000000
#define MAX_FIXED_AREA_SIZE	0x10000000
#define MSM_MM_FW_SIZE		0x200000
#define MSM8960_FW_START	(MSM8960_FIXED_AREA_START - MSM_MM_FW_SIZE)

#else
#define MSM_PMEM_KERNEL_EBI1_SIZE  0x110C000
#define MSM_ION_HEAP_NUM	1
#endif


struct pm8xxx_gpio_init {
	unsigned			gpio;
	struct pm_gpio			config;
};

struct pm8xxx_mpp_init {
	unsigned			mpp;
	struct pm8xxx_mpp_config_data	config;
};

#define PM8XXX_GPIO_INIT(_gpio, _dir, _buf, _val, _pull, _vin, _out_strength, \
			_func, _inv, _disable) \
{ \
	.gpio	= PM8921_GPIO_PM_TO_SYS(_gpio), \
	.config	= { \
		.direction	= _dir, \
		.output_buffer	= _buf, \
		.output_value	= _val, \
		.pull		= _pull, \
		.vin_sel	= _vin, \
		.out_strength	= _out_strength, \
		.function	= _func, \
		.inv_int_pol	= _inv, \
		.disable_pin	= _disable, \
	} \
}

#define PM8XXX_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8921_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

#define PM8XXX_GPIO_DISABLE(_gpio) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, 0, 0, 0, PM_GPIO_VIN_S4, \
			 0, 0, 0, 1)

#define PM8XXX_GPIO_OUTPUT(_gpio, _val) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_INPUT(_gpio, _pull) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
			_pull, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_NO, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_OUTPUT_FUNC(_gpio, _val, _func) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_HIGH, \
			_func, 0, 0)

#define PM8XXX_GPIO_OUTPUT_VIN(_gpio, _val, _vin) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, _vin, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_PAIRED_IN_VIN(_gpio, _vin) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
			PM_GPIO_PULL_UP_1P5, _vin, \
			PM_GPIO_STRENGTH_NO, \
			PM_GPIO_FUNC_PAIRED, 0, 0)

#define PM8XXX_GPIO_PAIRED_OUT_VIN(_gpio, _vin) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 0, \
			PM_GPIO_PULL_UP_1P5, _vin, \
			PM_GPIO_STRENGTH_MED, \
			PM_GPIO_FUNC_PAIRED, 0, 0)

extern struct gpio_regulator_platform_data
	msm_gpio_regulator_pdata[] __devinitdata;

extern struct msm_bus_scale_pdata mdp_bus_scale_pdata;
extern struct msm_panel_common_pdata mdp_pdata;
extern struct msm_camera_device_platform_data msm_camera_csi_device_data[];

extern struct regulator_init_data msm_saw_regulator_pdata_s5;
extern struct regulator_init_data msm_saw_regulator_pdata_s6;

extern struct rpm_regulator_platform_data msm_rpm_regulator_pdata __devinitdata;
extern struct lcdc_platform_data dtv_pdata;
extern struct msm_camera_gpio_conf msm_camif_gpio_conf_mclk0;
extern struct msm_camera_gpio_conf msm_camif_gpio_conf_mclk1;
extern struct platform_device hdmi_msm_device;
extern struct platform_device android_usb_device;
extern struct platform_device msm_tsens_device;

extern struct msm_otg_platform_data msm_otg_pdata;

extern bool camera_single_mclk;
extern void update_camera_gpio_cfg(struct msm_camera_sensor_info sensor_info,
		uint8_t drv_strength);

extern void msm8960_init_hdmi(struct platform_device *hdmi_dev,
						struct msm_hdmi_platform_data *hdmi_data);

extern void __init msm8960_init_usb(void);
extern void __init msm8960_init_dsps(void);
extern void __init msm8960_init_gsbi4(void);

extern void __init msm8960_init_hsic(void);

extern void __init msm8960_init_buses(void);
extern int  __init gpiomux_init(bool use_mdp_vsync);

extern void __init msm8960_init_tsens(void);
extern void __init msm8960_init_rpm(void);
extern void __init msm8960_init_sleep_status(void);
extern void __init msm8960_init_regulators(void);

extern void __init msm8960_i2c_init(unsigned speed, u8 gsbi_shared_mode);
extern void __init msm8960_spi_init(struct msm_spi_platform_data *pdata, 
					struct spi_board_info *binfo, unsigned size);
extern void __init msm8960_gfx_init(void);
extern void __init msm8960_spm_init(void);
extern void __init msm8960_add_common_devices(int (*detect_client)(const char *name));
extern void __init pm8921_gpio_mpp_init(struct pm8xxx_gpio_init *pm8921_gpios,
									unsigned gpio_size,
									struct pm8xxx_mpp_init *pm8921_mpps,
									unsigned mpp_size);
extern void __init msm8960_init_slim(void);
extern void __init msm8960_pm_init(unsigned wakeup_irq);
extern void __init pm8921_init(struct pm8xxx_keypad_platform_data *keypad,
			       int mode, int cool_temp, int warm_temp, void *cb,
			       int lock, int hot_temp, int hot_temp_offset,
			       int hot_temp_pcb,
			       signed char hot_temp_pcb_offset);

extern int  msm8960_change_memory_power(u64 start, u64 size, int change_type);
extern void __init msm8960_map_io(void);
extern void __init msm8960_reserve(void);
extern void __init msm8960_allocate_memory_regions(void);
extern void __init msm8960_early_memory(void);

#ifdef CONFIG_ARCH_MSM8930
extern void msm8930_map_io(void);
#endif

extern void msm8960_init_irq(void);

extern int pm8xxx_set_led_info(unsigned index, struct led_info *linfo);

#define PLATFORM_IS_CHARM25() \
	(machine_is_msm8960_cdp() && \
		(socinfo_get_platform_subtype() == 1) \
	)

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
enum {
	GPIO_EXPANDER_IRQ_BASE = (PM8921_IRQ_BASE + PM8921_NR_IRQS),
	GPIO_EXPANDER_GPIO_BASE = (PM8921_MPP_BASE + PM8921_NR_MPPS),
	/* CAM Expander */
	GPIO_CAM_EXPANDER_BASE = GPIO_EXPANDER_GPIO_BASE,
	GPIO_CAM_GP_STROBE_READY = GPIO_CAM_EXPANDER_BASE,
	GPIO_CAM_GP_AFBUSY,
	GPIO_CAM_GP_STROBE_CE,
	GPIO_CAM_GP_CAM1MP_XCLR,
	GPIO_CAM_GP_CAMIF_RESET_N,
	GPIO_CAM_GP_XMT_FLASH_INT,
	GPIO_CAM_GP_LED_EN1,
	GPIO_CAM_GP_LED_EN2,
	GPIO_LIQUID_EXPANDER_BASE = GPIO_CAM_EXPANDER_BASE + 8,
};
#endif

enum {
	SX150X_CAM,
	SX150X_LIQUID,
};

#endif

extern struct sx150x_platform_data msm8960_sx150x_data[];
extern struct msm_camera_board_info msm8960_camera_board_info;

void msm8960_init_cam(void);
void msm8960_init_fb(void);
void msm8960_init_pmic(void);
void msm8960_init_mmc(unsigned sd_detect);
int msm8960_init_gpiomux(void);
unsigned char msm8960_hdmi_as_primary_selected(void);
void msm8960_allocate_fb_region(void);
void msm8960_set_display_params(char *prim_panel, char *ext_panel);
void msm8960_pm8921_gpio_mpp_init(void);
void msm8960_mdp_writeback(struct memtype_reserve *reserve_table);
uint32_t msm_rpm_get_swfi_latency(void);
#define PLATFORM_IS_CHARM25() \
	(machine_is_msm8960_cdp() && \
		(socinfo_get_platform_subtype() == 1) \
	)

#define MSM_8960_GSBI4_QUP_I2C_BUS_ID 4
#define MSM_8960_GSBI3_QUP_I2C_BUS_ID 3
#define MSM_8960_GSBI10_QUP_I2C_BUS_ID 10

#define HDMI_PANEL_NAME	"hdmi_msm"

extern int msm8960_headset_hw_has_gpio(int *hs_bias);
