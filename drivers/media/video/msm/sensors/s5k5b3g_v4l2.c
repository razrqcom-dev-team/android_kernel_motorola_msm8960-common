/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.7006
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#include <linux/regulator/consumer.h>

#define SENSOR_NAME "s5k5b3g"
#define PLATFORM_DRIVER_NAME "msm_camera_s5k5b3g"
#define s5k5b3g_obj s5k5b3g_##obj

#define S5K5B3G_DEFAULT_MCLK_RATE 24000000


DEFINE_MUTEX(s5k5b3g_mut);
static struct msm_sensor_ctrl_t s5k5b3g_s_ctrl;

static struct regulator *reg_1p2, *reg_1p8, *reg_2p8;

static struct msm_camera_i2c_reg_conf s5k5b3g_start_settings[] = {
	{0x0100, 0x01, MSM_CAMERA_I2C_BYTE_DATA},  /* */
};

static struct msm_camera_i2c_reg_conf s5k5b3g_stop_settings[] = {
	{0x0100, 0x00, MSM_CAMERA_I2C_BYTE_DATA},  /* */
};

static struct msm_camera_i2c_reg_conf s5k5b3g_groupon_settings[] = {
	{0x0104, 0x0001},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_groupoff_settings[] = {
	{0x0104, 0x0000},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_prev_settings[] = {
	/* Placeholder */
	{0x7006, 0x0000},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_snap_settings[] = {
	/* Placeholder */
	{0x7006, 0x0001},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_reset_settings[] = {
	{0x0103, 0x0001},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_recommend_settings[] = {
	/* Settings from Samsung Dev Kit */
	{0x0B06, 0x0180,},/* */
	{0x0B08, 0x0180,},/* */
	{0x0B80, 0x00, MSM_CAMERA_I2C_BYTE_DATA},  /* */
	{0x0B00, 0x00, MSM_CAMERA_I2C_BYTE_DATA},  /* */

	/* PLL Parameters */
	{0x3000, 0x1800,},/* input_clock_mhz_int (24Mhz) */
	{0x0300, 0x0005,},/* vt_pix_clk_div (72Mhz) */
	{0x0302, 0x0002,},/* vt_sys_clk_div (360Mhz) */
	{0x0304, 0x0006,},/* pre_pll_clk_div (4Mhz) */
	{0x0306, 0x00B4,},/* pll_multiplier (720Mhz) */
	{0x0308, 0x000A,},/* op_pix_clk_div (72Mhz) */
	{0x030A, 0x0001,},/* op_sys_clk_div (720Mhz) */

	/* Frame Size */
	{0x0340, 0x0479,},/* */
	{0x0342, 0x0830,},/* */

	/* Input Size and Format */
	{0x0344, 0x0004,},/* */
	{0x0346, 0x0004,},/* */
	{0x0348, 0x0783,},/* (1923) */
	{0x034A, 0x043B,},/* (1083) */
	{0x0382, 0x0001,},/* */
	{0x0386, 0x0001,},/* */
	/*V 0 - Subsampling ; 1 - binning V*/
	{0x0900, 0x01, MSM_CAMERA_I2C_BYTE_DATA},
	/*V High nib vert;Low nib horiz; 1-Subsample; 2-Binning V*/
	{0x0901, 0x22, MSM_CAMERA_I2C_BYTE_DATA},
	{0x0101, 0x03, MSM_CAMERA_I2C_BYTE_DATA},  /* XY Mirror */

	/* Output Size and Format */
	{0x034C, 0x0780,},/* (1920) */
	{0x034E, 0x0438,},/* (1080) */
	{0x0112, 0x0A0A,},/* 10 bit out */
	{0x0111, 0x02, MSM_CAMERA_I2C_BYTE_DATA},  /* MIPI */
	/*V PVI mode, no CCP2 framing V*/
	{0x30EE, 0x00, MSM_CAMERA_I2C_BYTE_DATA},
	{0x30F4, 0x0008,},/* RAW10 mode */
	{0x300C, 0x00, MSM_CAMERA_I2C_BYTE_DATA},  /* No embedded lines */
	/*V Horiz size not limited to mult of 16 V*/
	{0x30ED, 0x00, MSM_CAMERA_I2C_BYTE_DATA},
	{0x7088, 0x0157,},/* Increase drive strength on PCLK */
	{0xB0E6, 0x0000,},/* */

	/* Integration Time */
	{0x0200, 0x0200,},/* */
	{0x0202, 0x0475,},/* (33ms) */

	/* Analog Gain */
	{0x0120, 0x0000,},/* Global analog gain (not per channel) */
	/* gain=128/(x+16) ; x = 128/gain - 16 */
	/* AGx1 0070 */
	/* AGx2 0030 */
	/* AGx3 001B */
	/* AGx4 0010 */
	/* AGx5 000A */
	/* AGx6 0005 */
	/* AGx8 0000 */
	{0x0204, 0x0070,},/* AGx1 */

	/* Digital Gain */
	{0x020E, 0x0100,},/* */
	{0x0210, 0x0100,},/* */
	{0x0212, 0x0100,},/* */
	{0x0214, 0x0100,},/* */

	/* #smiaRegs_vendor_emb_use_header //for Frame count */
	{0x300C, 0x01, MSM_CAMERA_I2C_BYTE_DATA},  /* */
};

static struct v4l2_subdev_info s5k5b3g_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array s5k5b3g_init_conf[] = {
	{&s5k5b3g_reset_settings[0],
	ARRAY_SIZE(s5k5b3g_reset_settings), 50, MSM_CAMERA_I2C_WORD_DATA},
	{&s5k5b3g_recommend_settings[0],
	ARRAY_SIZE(s5k5b3g_recommend_settings), 0, MSM_CAMERA_I2C_WORD_DATA}
};

static struct msm_camera_i2c_conf_array s5k5b3g_confs[] = {
	{&s5k5b3g_snap_settings[0],
	ARRAY_SIZE(s5k5b3g_snap_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&s5k5b3g_prev_settings[0],
	ARRAY_SIZE(s5k5b3g_prev_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_sensor_output_info_t s5k5b3g_dimensions[] = {
	{
		.x_output = 0x780,
		.y_output = 0x438,
		.line_length_pclk = 0x830,
		.frame_length_lines = 0x479,
		.vt_pixel_clk = 72000000,
		.op_pixel_clk = 72000000,
		.binning_factor = 1,
	},
};

static struct msm_camera_csid_vc_cfg s5k5b3g_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params s5k5b3g_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 1,
		.lut_params = {
			.num_cid = 2,
			.vc_cfg = s5k5b3g_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 1,
		.settle_cnt = 0x14,
	},

};

static struct msm_camera_csi2_params *s5k5b3g_csi_params_array[] = {
	&s5k5b3g_csi_params,
	&s5k5b3g_csi_params,
};

static struct msm_sensor_output_reg_addr_t s5k5b3g_reg_addr = {
	.x_output = 0x348,
	.y_output = 0x34A,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x340,
};

static struct msm_sensor_id_info_t s5k5b3g_id_info = {
	.sensor_id_reg_addr = 0x7006,
	.sensor_id = 0x05B3,
};

static struct msm_sensor_exp_gain_info_t s5k5b3g_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0204,
	.vert_offset = 6,
};

static int32_t s5k5b3g_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines, offset;
	uint8_t int_time[3];
	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	pr_info("%s: gain(0x%x) line(0x%x) fl_lines(0x%x)\n",
		__func__, gain, line, fl_lines);

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);

	int_time[0] = line >> 12;
	int_time[1] = line >> 4;
	int_time[2] = line << 4;

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
		line, MSM_CAMERA_I2C_WORD_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);

	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);

	return 0;
}

static int32_t s5k5b3g_regulator_on(struct regulator **reg,
		char *regname, int uV)
{
	int32_t rc = 0;
	pr_info("%s: %s %d\n", __func__, regname, uV);

	*reg = regulator_get(NULL, regname);
	if (IS_ERR(*reg)) {
		pr_err("%s: failed to get %s (%ld)\n",
				__func__, regname, PTR_ERR(*reg));
		goto reg_on_done;
	}
	rc = regulator_set_voltage(*reg, uV, uV);
	if (rc) {
		pr_err("%s: failed to set voltage for %s (%d)\n",
				__func__, regname, rc);
		goto reg_on_done;
	}
	rc = regulator_enable(*reg);
	if (rc) {
		pr_err("%s: failed to enable %s (%d)\n",
				__func__, regname, rc);
		goto reg_on_done;
	}
reg_on_done:
	return rc;
}

static int32_t s5k5b3g_regulator_off(struct regulator *reg, char *regname)
{
	int32_t rc = 0;

	if (reg) {
		pr_err("%s: %s\n", __func__, regname);

		rc = regulator_disable(reg);
		if (rc) {
			pr_err("%s: failed to disable %s (%d)\n",
					__func__, regname, rc);
			goto reg_off_done;
		}
		regulator_put(reg);
	}
reg_off_done:
	return rc;
}


static int32_t s5k5b3g_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_platform_info *pinfo =
		s_ctrl->sensordata->sensor_platform_info;

	pr_info("%s: R:%d D1.2:%s D1.8:%s IO:%d A2.8:%s A:%d\n",
			__func__,
			pinfo->sensor_reset,
			(pinfo->reg_1p2 ? pinfo->reg_1p2 : "-"),
			(pinfo->reg_1p8 ? pinfo->reg_1p8 : "-"),
			pinfo->digital_en,
			(pinfo->reg_2p8 ? pinfo->reg_2p8 : "-"),
			pinfo->analog_en);

	/* obtain gpios */
	if (pinfo->digital_en) {
		rc = gpio_request(pinfo->digital_en, "s5k5b3g");
	}
	if (rc) {
		pr_err("%s: gpio request digital_en failed (%d)\n",
				__func__, rc);
		goto power_on_done;
	}

	if (pinfo->analog_en) {
		rc = gpio_request(pinfo->analog_en, "s5k5b3g");
	}
	if (rc) {
		pr_err("%s: gpio request analog_en failed (%d)\n",
				__func__, rc);
		goto power_on_done;
	}

	if (pinfo->sensor_reset) {
		rc = gpio_request(pinfo->sensor_reset, "s5k5b3g");
	}
	if (rc) {
		pr_err("%s: gpio request sensor_reset failed (%d)\n",
				__func__, rc);
		goto power_on_done;
	}

	/* turn on mclk */
	msm_sensor_probe_on(&s_ctrl->sensor_i2c_client->client->dev);
	msm_camio_clk_rate_set(S5K5B3G_DEFAULT_MCLK_RATE);
	usleep_range(1000, 2000);

	/* turn on I/O digital supply */
	if (pinfo->digital_en) {
		gpio_direction_output(pinfo->digital_en, pinfo->dig_en_inv ^ 0);
	} else {
		if (pinfo->reg_1p8) {
			rc = s5k5b3g_regulator_on(&reg_1p8,
				pinfo->reg_1p8,
				1800000);
		}
	}
	if (rc < 0) {
		pr_err("%s: reg_1p8 enable failed (%d)\n", __func__, rc);
		goto power_on_done;
	}

	/* turn on core digital supply */
	if (pinfo->reg_1p2) {
		rc = s5k5b3g_regulator_on(&reg_1p2, pinfo->reg_1p2, 1200000);
	}
	if (rc < 0) {
		pr_err("%s: reg_1p2 enable failed (%d)\n", __func__, rc);
		goto power_on_done;
	}

	/* turn on pixel analog supply */
	if (pinfo->analog_en) {
		gpio_direction_output(pinfo->analog_en, pinfo->ana_en_inv ^ 1);
	} else {
		if (pinfo->reg_2p8) {
			rc = s5k5b3g_regulator_on(&reg_2p8,
				pinfo->reg_2p8,
				2800000);
		}
	}
	if (rc < 0) {
		pr_err("%s: reg_2p8 enable failed (%d)\n", __func__, rc);
		goto power_on_done;
	}

	/* toggle reset */
	if (pinfo->sensor_reset) {
		gpio_direction_output(pinfo->sensor_reset, 0);
		usleep_range(5000, 6000);
		gpio_set_value_cansleep(pinfo->sensor_reset, 1);
	}
	msleep(50);

power_on_done:
	return rc;
}

static int32_t s5k5b3g_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_platform_info *pinfo =
		s_ctrl->sensordata->sensor_platform_info;

	pr_info("%s\n", __func__);

	/* assert reset */
	if (pinfo->sensor_reset) {
		gpio_direction_output(pinfo->sensor_reset, 0);
		gpio_free(pinfo->sensor_reset);
	}
	msleep(50);

	/* turn off analog pixel supply */
	if (pinfo->analog_en) {
		gpio_direction_output(pinfo->analog_en, pinfo->ana_en_inv ^ 0);
		gpio_free(pinfo->analog_en);
	}
	if (pinfo->reg_2p8) {
		s5k5b3g_regulator_off(reg_2p8, "2.8");
	}

	/* turn off core digital supply */
	if (pinfo->reg_1p2) {
		s5k5b3g_regulator_off(reg_1p2, "1.2");
	}

	/* turn off I/O digital supply */
	if (pinfo->digital_en) {
		gpio_direction_output(pinfo->digital_en, pinfo->dig_en_inv ^ 1);
		gpio_free(pinfo->digital_en);
	}

	msm_sensor_probe_off(&s_ctrl->sensor_i2c_client->client->dev);

	return 0;
}


static const struct i2c_device_id s5k5b3g_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&s5k5b3g_s_ctrl},
	{ }
};

static struct i2c_driver s5k5b3g_i2c_driver = {
	.id_table = s5k5b3g_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k5b3g_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int32_t s5k5b3g_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: read id failed\n", __func__);
		return rc;
	}

	pr_info("%s: chipid: %04x\n", __func__, chipid);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("%s: chip id does not match\n", __func__);
		return -ENODEV;
	}
	pr_info("%s: match_id success\n", __func__);
	return 0;
}

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&s5k5b3g_i2c_driver);
}

static struct v4l2_subdev_core_ops s5k5b3g_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops s5k5b3g_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops s5k5b3g_subdev_ops = {
	.core = &s5k5b3g_subdev_core_ops,
	.video  = &s5k5b3g_subdev_video_ops,
};

static struct msm_sensor_fn_t s5k5b3g_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = s5k5b3g_write_exp_gain,
	.sensor_write_snapshot_exp_gain = s5k5b3g_write_exp_gain,
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = s5k5b3g_sensor_power_up,
	.sensor_power_down = s5k5b3g_sensor_power_down,
	.sensor_match_id = s5k5b3g_match_id,
};

static struct msm_sensor_reg_t s5k5b3g_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = s5k5b3g_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(s5k5b3g_start_settings),
	.stop_stream_conf = s5k5b3g_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(s5k5b3g_stop_settings),
	.group_hold_on_conf = s5k5b3g_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(s5k5b3g_groupon_settings),
	.group_hold_off_conf = s5k5b3g_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(s5k5b3g_groupoff_settings),
	.init_settings = &s5k5b3g_init_conf[0],
	.init_size = ARRAY_SIZE(s5k5b3g_init_conf),
	.mode_settings = &s5k5b3g_confs[0],
	.output_settings = &s5k5b3g_dimensions[0],
	.num_conf = ARRAY_SIZE(s5k5b3g_confs),
};

static struct msm_sensor_ctrl_t s5k5b3g_s_ctrl = {
	.msm_sensor_reg = &s5k5b3g_regs,
	.sensor_i2c_client = &s5k5b3g_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &s5k5b3g_reg_addr,
	.sensor_id_info = &s5k5b3g_id_info,
	.sensor_exp_gain_info = &s5k5b3g_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &s5k5b3g_csi_params_array[0],
	.msm_sensor_mutex = &s5k5b3g_mut,
	.sensor_i2c_driver = &s5k5b3g_i2c_driver,
	.sensor_v4l2_subdev_info = s5k5b3g_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k5b3g_subdev_info),
	.sensor_v4l2_subdev_ops = &s5k5b3g_subdev_ops,
	.func_tbl = &s5k5b3g_func_tbl,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Samsung S5K5B3G Bayer sensor driver");
MODULE_LICENSE("GPL v2");


