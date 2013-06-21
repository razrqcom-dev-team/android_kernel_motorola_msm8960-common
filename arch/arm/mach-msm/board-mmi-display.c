/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/msm_ion.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/system_info.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/ion.h>
#include <mach/socinfo.h>

#include "devices.h"
#include "board-8960.h"
#include "board-mmi.h"

#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
static unsigned char hdmi_is_primary = 1;
#else
static unsigned char hdmi_is_primary;
#endif

unsigned char msm8960_hdmi_as_primary_selected(void)
{
	return hdmi_is_primary;
}

static struct resource msm_fb_resources[] = {
	{
		.flags = IORESOURCE_DMA,
	}
};

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = NULL, /* msm_fb_detect_panel, */
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources     = ARRAY_SIZE(msm_fb_resources),
	.resource          = msm_fb_resources,
	.dev.platform_data = &msm_fb_pdata,
};

#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors mdp_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};


#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
static struct msm_bus_vectors hdmi_as_primary_vectors[] = {
	/* If HDMI is used as primary */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 2000000000UL,
		.ib = 2000000000UL,
	},
};
static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
};
#else
static struct msm_bus_vectors mdp_ui_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
		.ib = 270000000 * 2,
	},
};

static struct msm_bus_vectors mdp_vga_vectors[] = {
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
		.ib = 270000000 * 2,
	},
};

static struct msm_bus_vectors mdp_720p_vectors[] = {
	/* 720p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 230400000 * 2,
		.ib = 288000000 * 2,
	},
};

static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 334080000 * 2,
		.ib = 417600000 * 2,
	},
};

static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_vga_vectors),
		mdp_vga_vectors,
	},
	{
		ARRAY_SIZE(mdp_720p_vectors),
		mdp_720p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
};
#endif

static struct msm_bus_scale_pdata mdp_bus_scale_pdata = {
	mdp_bus_scale_usecases,
	ARRAY_SIZE(mdp_bus_scale_usecases),
	.name = "mdp",
};

#endif

#if 0
static int mdp_core_clk_rate_table[] = {
	85330000,
	85330000,
	160000000,
	200000000,
};
#endif

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
	.mdp_max_clk = 200000000,
	.mdp_max_bw = 2000000000,
	.mdp_bw_ab_factor = 115,
	.mdp_bw_ib_factor = 125,
//	.mdp_core_clk_rate = 85330000,
//	.mdp_core_clk_table = mdp_core_clk_rate_table,
//	.num_mdp_clk = ARRAY_SIZE(mdp_core_clk_rate_table),
#ifdef CONFIG_MSM_BUS_SCALING
	.mdp_bus_scale_table = &mdp_bus_scale_pdata,
#endif
	.mdp_rev = MDP_REV_42,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = BIT(ION_CP_MM_HEAP_ID),
#else
	.mem_hid = MEMTYPE_EBI1,
#endif
	.cont_splash_enabled = 0x01,
	.splash_screen_addr = 0x00,
	.splash_screen_size = 0x00,
	.mdp_iommu_split_domain = 0,
};


// PANEL SETUP

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

char panel_name[PANEL_NAME_MAX_LEN + 1] = DEFAULT_PANEL_NAME;

int is_smd_hd_465(void)
{
	return !strncmp(panel_name, "mipi_mot_video_smd_hd_465",
				PANEL_NAME_MAX_LEN);
}
int is_smd_qhd_429(void)
{
	return !strncmp(panel_name, "mipi_mot_cmd_smd_qhd_429",
				PANEL_NAME_MAX_LEN);
}
int is_auo_hd_450(void)
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
				rc = regulator_set_voltage(reg_vddio, 1800000, 1800000);
			else
				rc = regulator_set_voltage(reg_vddio, 2650000 /*get_l17_voltage()*/, 2850000);

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

void __init msm8960_mdp_writeback(struct memtype_reserve* reserve_table)
{
	mdp_pdata.ov0_wb_size = MSM_FB_OVERLAY0_WRITEBACK_SIZE;
	mdp_pdata.ov1_wb_size = MSM_FB_OVERLAY1_WRITEBACK_SIZE;
#if defined(CONFIG_ANDROID_PMEM) && !defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov0_wb_size;
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov1_wb_size;

	pr_info("mem_map: mdp reserved with size 0x%lx in pool\n",
		mdp_pdata.ov0_wb_size + mdp_pdata.ov1_wb_size);
#endif
}

#if 0
static char mipi_dsi_splash_is_enabled(void)
{
	return mdp_pdata.cont_splash_enabled;
}
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
static struct resource hdmi_msm_resources[] = {
	{
		.name  = "hdmi_msm_qfprom_addr",
		.start = 0x00700000,
		.end   = 0x007060FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_hdmi_addr",
		.start = 0x04A00000,
		.end   = 0x04A00FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_irq",
		.start = HDMI_IRQ,
		.end   = HDMI_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static int hdmi_enable_5v(int on);
static int hdmi_core_power(int on, int show);
static int hdmi_cec_power(int on);
static int hdmi_gpio_config(int on);
static int hdmi_panel_power(int on);

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
	.panel_power = hdmi_panel_power,
	.gpio_config = hdmi_gpio_config,
};

static struct platform_device hdmi_msm_device = {
	.name = "hdmi_msm",
	.id = 0,
	.num_resources = ARRAY_SIZE(hdmi_msm_resources),
	.resource = hdmi_msm_resources,
	.dev.platform_data = &hdmi_msm_data,
};
#else
static int hdmi_panel_power(int on) { return 0; }
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
static struct platform_device wfd_panel_device = {
	.name = "wfd_panel",
	.id = 0,
	.dev.platform_data = NULL,
};

static struct platform_device wfd_device = {
	.name          = "msm_wfd",
	.id            = -1,
};
#endif

#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors dtv_bus_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors dtv_bus_def_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 566092800 * 2,
		.ib = 707616000 * 2,
	},
};

static struct msm_bus_paths dtv_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(dtv_bus_init_vectors),
		dtv_bus_init_vectors,
	},
	{
		ARRAY_SIZE(dtv_bus_def_vectors),
		dtv_bus_def_vectors,
	},
};
static struct msm_bus_scale_pdata dtv_bus_scale_pdata = {
	dtv_bus_scale_usecases,
	ARRAY_SIZE(dtv_bus_scale_usecases),
	.name = "dtv",
};

static struct lcdc_platform_data dtv_pdata = {
	.bus_scale_table = &dtv_bus_scale_pdata,
	.lcdc_power_save = hdmi_panel_power,
};
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
static int hdmi_enable_5v(int on)
{
	/* TBD: PM8921 regulator instead of 8901 */
	static struct regulator *reg_8921_hdmi_mvs;	/* HDMI_5V */
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_8921_hdmi_mvs) {
		reg_8921_hdmi_mvs = regulator_get(&hdmi_msm_device.dev,
					"hdmi_mvs");
		if (IS_ERR(reg_8921_hdmi_mvs)) {
			pr_err("'%s' regulator not found, rc=%ld\n",
				"hdmi_mvs", IS_ERR(reg_8921_hdmi_mvs));
			reg_8921_hdmi_mvs = NULL;
			return -ENODEV;
		}
	}

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
		pr_debug("%s(on): success\n", __func__);
	} else {
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
}

static int hdmi_gpio_config(int on)
{
	int rc = 0;
	static int prev_on;

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(100, "HDMI_DDC_CLK");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_CLK", 100, rc);
			return rc;
		}
		rc = gpio_request(101, "HDMI_DDC_DATA");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_DATA", 101, rc);
			goto error1;
		}
		rc = gpio_request(102, "HDMI_HPD");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_HPD", 102, rc);
			goto error2;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(100);
		gpio_free(101);
		gpio_free(102);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;
	return 0;

error2:
	gpio_free(101);
error1:
	gpio_free(100);
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

static int hdmi_panel_power(int on)
{
	int rc;

	pr_debug("%s: HDMI Core: %s\n", __func__, (on ? "ON" : "OFF"));
	rc = hdmi_core_power(on, 1);
	if (rc)
		rc = hdmi_cec_power(on);

	pr_debug("%s: HDMI Core: %s Success\n", __func__, (on ? "ON" : "OFF"));
	return rc;
}
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

void __init msm8960_init_fb(int (*detect_client)(const char *name), int mmi_feature_hdmi)
{
	uint32_t soc_platform_version = socinfo_get_version();


	if (SOCINFO_VERSION_MAJOR(soc_platform_version) >= 3)
		mdp_pdata.mdp_rev = MDP_REV_43;

	if (cpu_is_msm8960ab())
		mdp_pdata.mdp_rev = MDP_REV_44;

	msm_fb_pdata.detect_client = detect_client;
	platform_device_register(&msm_fb_device);

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
	platform_device_register(&wfd_panel_device);
	platform_device_register(&wfd_device);
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	if (mmi_feature_hdmi)
		platform_device_register(&hdmi_msm_device);
#endif

	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
#ifdef CONFIG_MSM_BUS_SCALING
	msm_fb_register_device("dtv", &dtv_pdata);
#endif
}

void __init msm8960_allocate_fb_region(void)
{
	void *addr;
	unsigned long size;

	size = MSM_FB_SIZE;
	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
			size, addr, __pa(addr));
}

/**
 * Set MDP clocks to high frequency to avoid DSI underflow
 * when using high resolution 1200x1920 WUXGA panels
 */
static void set_mdp_clocks_for_wuxga(void)
{
	mdp_ui_vectors[0].ab = 2000000000;
	mdp_ui_vectors[0].ib = 2000000000;
	mdp_vga_vectors[0].ab = 2000000000;
	mdp_vga_vectors[0].ib = 2000000000;
	mdp_720p_vectors[0].ab = 2000000000;
	mdp_720p_vectors[0].ib = 2000000000;
	mdp_1080p_vectors[0].ab = 2000000000;
	mdp_1080p_vectors[0].ib = 2000000000;

	if (hdmi_is_primary) {
		dtv_bus_def_vectors[0].ab = 2000000000;
		dtv_bus_def_vectors[0].ib = 2000000000;
	}
}

void __init msm8960_set_display_params(char *prim_panel, char *ext_panel)
{
	int disable_splash = 0;
	if (strnlen(prim_panel, PANEL_NAME_MAX_LEN)) {
		strlcpy(msm_fb_pdata.prim_panel_name, prim_panel,
			PANEL_NAME_MAX_LEN);
		pr_debug("msm_fb_pdata.prim_panel_name %s\n",
			msm_fb_pdata.prim_panel_name);

		disable_splash = 1;

		if (!strncmp((char *)msm_fb_pdata.prim_panel_name,
			HDMI_PANEL_NAME, strnlen(HDMI_PANEL_NAME,
				PANEL_NAME_MAX_LEN))) {
			pr_debug("HDMI is the primary display by"
				" boot parameter\n");
			hdmi_is_primary = 1;
			set_mdp_clocks_for_wuxga();
		}
	}
	if (strnlen(ext_panel, PANEL_NAME_MAX_LEN)) {
		strlcpy(msm_fb_pdata.ext_panel_name, ext_panel,
			PANEL_NAME_MAX_LEN);
		pr_debug("msm_fb_pdata.ext_panel_name %s\n",
			msm_fb_pdata.ext_panel_name);
	}

	if (disable_splash)
		mdp_pdata.cont_splash_enabled = 0;
}
