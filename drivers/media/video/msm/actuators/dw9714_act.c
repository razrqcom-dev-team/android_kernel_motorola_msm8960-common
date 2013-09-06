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

#include "msm_actuator.h"
#include <linux/debugfs.h>
#include "media/msm_camera_query.h"

#define DW9714_TOTAL_STEPS_NEAR_TO_FAR_MAX 45

DEFINE_MUTEX(dw9714_act_mutex);
static int dw9714_actuator_debug_init(void);
static struct msm_actuator_ctrl_t dw9714_act_t;

static int32_t dw9714_wrapper_i2c_write(struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, void *params)
{
	uint16_t msb = 0, lsb = 0;
	msb = (next_lens_position >> 4) & 0x3F;
	lsb = (next_lens_position << 4) & 0xF0;
	lsb |= (*(uint8_t *)params);
	CDBG("%s: Actuator MSB:0x%x, LSB:0x%x\n", __func__, msb, lsb);
	msm_camera_i2c_write(&a_ctrl->i2c_client,
		msb, lsb, MSM_CAMERA_I2C_BYTE_DATA);
	return next_lens_position;
}

static uint8_t dw9714_hw_params[] = {
	0x0,/*0*/
	0x7,/*1*/
	0x6,/*2*/
	0x5,/*3*/
	0xA,/*4*/
	0xE,/*5*/
};

static uint16_t dw9714_macro_scenario[] = {
	/* MOVE_NEAR dir*/
	1,
	3,
	4,
	8,
	14,
	DW9714_TOTAL_STEPS_NEAR_TO_FAR_MAX,
};

static uint16_t dw9714_inf_scenario[] = {
	/* MOVE_NEAR dir*/
	1,
	6,
	24,
	DW9714_TOTAL_STEPS_NEAR_TO_FAR_MAX,
};

static struct region_params_t dw9714_regions[] = {
	/* step_bound[0] - macro side boundary
	 * step_bound[1] - infinity side boundary
	 */
	/* Region 1 */
	{
		.step_bound = {2, 0},
		.code_per_step = 90,
	},
	/* Region 2 */
	{
		.step_bound = {DW9714_TOTAL_STEPS_NEAR_TO_FAR_MAX, 2},
		.code_per_step = 9,
	}
};

static struct damping_params_t dw9714_macro_reg1_damping[] = {
	/* MOVE_NEAR Dir */
	/* Scene 1 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 1500,
		.hw_params = &dw9714_hw_params[0],
	},
	/* Scene 2 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 1500,
		.hw_params = &dw9714_hw_params[0],
	},
	/* Scene 3 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 1500,
		.hw_params = &dw9714_hw_params[0],
	},
	/* Scene 4 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 1500,
		.hw_params = &dw9714_hw_params[0],
	},
	/* Scene 5 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 1500,
		.hw_params = &dw9714_hw_params[0],
	},
	/* Scene 6 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 1500,
		.hw_params = &dw9714_hw_params[0],
	},
};

static struct damping_params_t dw9714_macro_reg2_damping[] = {
	/* MOVE_NEAR Dir */
	/* Scene 1 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 4500,
		.hw_params = &dw9714_hw_params[1],
	},
	/* Scene 2 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 4500,
		.hw_params = &dw9714_hw_params[2],
	},
	/* Scene 3 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 4500,
		.hw_params = &dw9714_hw_params[3],
	},
	/* Scene 4 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 4500,
		.hw_params = &dw9714_hw_params[2],
	},
	/* Scene 5 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 4500,
		.hw_params = &dw9714_hw_params[4],
	},
	/* Scene 6 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 4500,
		.hw_params = &dw9714_hw_params[5],
	},
};

static struct damping_params_t dw9714_inf_reg1_damping[] = {
	/* MOVE_FAR Dir */
	/* Scene 1 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 450,
		.hw_params = &dw9714_hw_params[4],
	},
	/* Scene 2 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 450,
		.hw_params = &dw9714_hw_params[4],
	},
	/* Scene 3 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 450,
		.hw_params = &dw9714_hw_params[4],
	},
	/* Scene 4 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 450,
		.hw_params = &dw9714_hw_params[4],
	},
};

static struct damping_params_t dw9714_inf_reg2_damping[] = {
	/* MOVE_FAR Dir */
	/* Scene 1 => Damping params */
	{
		.damping_step = 0x1FF,
		.damping_delay = 1000,
		.hw_params = &dw9714_hw_params[1],
	},
	/* Scene 2 => Damping params */
	{
		.damping_step = 0x1FF,
		.damping_delay = 24000,
		.hw_params = &dw9714_hw_params[2],
	},
	/* Scene 3 => Damping params */
	{
		.damping_step = 0x1FF,
		.damping_delay = 28000,
		.hw_params = &dw9714_hw_params[4],
	},
	/* Scene 4 => Damping params */
	{
		.damping_step = 135,
		.damping_delay = 20000,
		.hw_params = &dw9714_hw_params[4],
	},
};

static struct damping_t dw9714_macro_regions[] = {
	/* MOVE_NEAR dir */
	/* Region 1 */
	{
		.ringing_params = dw9714_macro_reg1_damping,
	},
	/* Region 2 */
	{
		.ringing_params = dw9714_macro_reg2_damping,
	},
};

static struct damping_t dw9714_inf_regions[] = {
	/* MOVE_FAR dir */
	/* Region 1 */
	{
		.ringing_params = dw9714_inf_reg1_damping,
	},
	/* Region 2 */
	{
		.ringing_params = dw9714_inf_reg2_damping,
	},
};


static int32_t dw9714_set_params(struct msm_actuator_ctrl_t *a_ctrl)
{
	msm_camera_i2c_write(&a_ctrl->i2c_client,
		0xEC, 0xA3, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(&a_ctrl->i2c_client,
		0xA1, 0x04, MSM_CAMERA_I2C_BYTE_DATA);

	if ((af_info.af_man_type1 == 0x39343031) &&
			(af_info.af_man_type2 == 0x34303134) &&
			(af_info.af_act_type == 0x4D49)) {
		msm_camera_i2c_write(&a_ctrl->i2c_client,
				0xF2, 0xD8, MSM_CAMERA_I2C_BYTE_DATA);
	} else {
		msm_camera_i2c_write(&a_ctrl->i2c_client,
				0xF2, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	}

	msm_camera_i2c_write(&a_ctrl->i2c_client,
		0xDC, 0x51, MSM_CAMERA_I2C_BYTE_DATA);

	return 0;
}

static const struct i2c_device_id dw9714_act_i2c_id[] = {
	{"dw9714_act", (kernel_ulong_t)&dw9714_act_t},
	{ }
};

static int dw9714_act_config(
	void __user *argp)
{
	CDBG("%s called\n", __func__);
	return (int) msm_actuator_config(&dw9714_act_t, argp);
}

static int dw9714_i2c_add_driver_table(
	void)
{
	CDBG("%s called\n", __func__);
	return (int) msm_actuator_init_table(&dw9714_act_t);
}

static struct i2c_driver dw9714_act_i2c_driver = {
	.id_table = dw9714_act_i2c_id,
	.probe  = msm_actuator_i2c_probe,
	.remove = __exit_p(dw9714_act_i2c_remove),
	.driver = {
		.name = "dw9714_act",
	},
};

static int __init dw9714_i2c_add_driver(
	void)
{
	CDBG("%s called\n", __func__);
	return i2c_add_driver(dw9714_act_t.i2c_driver);
}

static struct v4l2_subdev_core_ops dw9714_act_subdev_core_ops;

static struct v4l2_subdev_ops dw9714_act_subdev_ops = {
	.core = &dw9714_act_subdev_core_ops,
};

static int32_t dw9714_act_probe(
	void *board_info,
	void *sdev)
{
	dw9714_actuator_debug_init();

	return (int) msm_actuator_create_subdevice(&dw9714_act_t,
		(struct i2c_board_info const *)board_info,
		(struct v4l2_subdev *)sdev);
}

static int dw9714_act_power_down(void *act_info)
{
	return (int) msm_actuator_af_power_down(&dw9714_act_t);
}

static struct msm_actuator_ctrl_t dw9714_act_t = {
	.i2c_driver = &dw9714_act_i2c_driver,
	.i2c_addr = 0x18,
	.act_v4l2_subdev_ops = &dw9714_act_subdev_ops,
	.actuator_ext_ctrl = {
		.a_init_table = dw9714_i2c_add_driver_table,
		.a_create_subdevice = dw9714_act_probe,
		.a_config = dw9714_act_config,
		.a_power_down = dw9714_act_power_down,
	},

	.i2c_client = {
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	},

	.set_info = {
		.total_steps = DW9714_TOTAL_STEPS_NEAR_TO_FAR_MAX,
	},

	.curr_step_pos = 0,
	.curr_region_index = 0,
	.initial_code = 0,
	.actuator_mutex = &dw9714_act_mutex,

	/* Initialize scenario */
	.ringing_scenario[MOVE_NEAR] = dw9714_macro_scenario,
	.scenario_size[MOVE_NEAR] = ARRAY_SIZE(dw9714_macro_scenario),
	.ringing_scenario[MOVE_FAR] = dw9714_inf_scenario,
	.scenario_size[MOVE_FAR] = ARRAY_SIZE(dw9714_inf_scenario),

	/* Initialize region params */
	.region_params = dw9714_regions,
	.region_size = ARRAY_SIZE(dw9714_regions),

	/* Initialize damping params */
	.damping[MOVE_NEAR] = dw9714_macro_regions,
	.damping[MOVE_FAR] = dw9714_inf_regions,

	.func_tbl = {
		.actuator_set_params = dw9714_set_params,
		.actuator_init_focus = NULL,
		.actuator_init_table = msm_actuator_init_table,
		.actuator_move_focus = msm_actuator_move_focus,
		.actuator_write_focus = msm_actuator_write_focus,
		.actuator_i2c_write = dw9714_wrapper_i2c_write,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_set_lens_mode =  msm_actuator_set_lens_mode,
	},

	.get_info = {
		.focal_length_num = 483,
		.focal_length_den = 100,
		.f_number_num = 24,
		.f_number_den = 10,
		.f_pix_num = 14,
		.f_pix_den = 10,
		.total_f_dist_num = 347,
		.total_f_dist_den = 1,
	},
};

static int dw9714_actuator_set_delay(void *data, u64 val)
{
	dw9714_inf_reg2_damping[1].damping_delay = val;
	return 0;
}

static int dw9714_actuator_get_delay(void *data, u64 *val)
{
	*val = dw9714_inf_reg2_damping[1].damping_delay;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dw9714_delay,
	dw9714_actuator_get_delay,
	dw9714_actuator_set_delay,
	"%llu\n");

static int dw9714_actuator_set_jumpparam(void *data, u64 val)
{
	dw9714_inf_reg2_damping[1].damping_step = val & 0xFFF;
	return 0;
}

static int dw9714_actuator_get_jumpparam(void *data, u64 *val)
{
	*val = dw9714_inf_reg2_damping[1].damping_step;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dw9714_jumpparam,
	dw9714_actuator_get_jumpparam,
	dw9714_actuator_set_jumpparam,
	"%llu\n");

static int dw9714_actuator_set_hwparam(void *data, u64 val)
{
	dw9714_hw_params[2] = val & 0xFF;
	return 0;
}

static int dw9714_actuator_get_hwparam(void *data, u64 *val)
{
	*val = dw9714_hw_params[2];
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dw9714_hwparam,
	dw9714_actuator_get_hwparam,
	dw9714_actuator_set_hwparam,
	"%llu\n");

static int dw9714_actuator_debug_init(void)
{
	struct dentry *debugfs_base = debugfs_create_dir("dw9714_actuator", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	if (!debugfs_create_file("dw9714_delay",
		S_IRUGO | S_IWUSR, debugfs_base, NULL, &dw9714_delay))
		return -ENOMEM;

	if (!debugfs_create_file("dw9714_jumpparam",
		S_IRUGO | S_IWUSR, debugfs_base, NULL, &dw9714_jumpparam))
		return -ENOMEM;

	if (!debugfs_create_file("dw9714_hwparam",
		S_IRUGO | S_IWUSR, debugfs_base, NULL, &dw9714_hwparam))
		return -ENOMEM;

	return 0;
}
subsys_initcall(dw9714_i2c_add_driver);
MODULE_DESCRIPTION("DW9714 actuator");
MODULE_LICENSE("GPL v2");
