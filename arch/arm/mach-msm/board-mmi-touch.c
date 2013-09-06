/*
 * Copyright (c) 2012 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/input/touch_platform.h>

static const uint16_t *mapphone_touch_vkeys_map;
static int mapphone_touch_vkeys_map_size;
static struct attribute_group mapphone_attribute_group;
static struct attribute *mapphone_attributes[];
static struct kobj_attribute mapphone_touch_vkeys_attr;

static int __init mapphone_touch_driver_settings_init(
		struct touch_platform_data *tpdata,
		struct device_node *dtnode,
		struct i2c_board_info *i2c_info);
static int __init mapphone_touch_ic_settings_init(
		struct touch_platform_data *tpdata,
		struct device_node *dtnode,
		struct i2c_board_info *i2c_info);
static int __init mapphone_touch_firmware_init(
		struct touch_platform_data *tpdata,
		struct device_node *dtnode);
static int __init mapphone_touch_tdat_init(
		struct touch_platform_data *tpdata,
		struct device_node *dtnode);
static int __init mapphone_touch_framework_settings_init(
		struct touch_platform_data *tpdata,
		struct device_node *dtnode);
static void __init mapphone_touch_free(struct touch_platform_data *tpdata);
static int __init mapphone_touch_vkeys_init(
		struct device_node *dtnode,
		struct i2c_board_info *i2c_info);
static ssize_t mapphone_touch_vkeys_kobj_output(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

int __init mapphone_touch_panel_init(struct i2c_board_info *i2c_info,
		struct device_node *parent)
{
	int err = 0;
	struct device_node *child;
	struct touch_platform_data *tpdata =
		(struct touch_platform_data *)i2c_info->platform_data;

	for_each_child_of_node(parent, child) {
		int len = 0;
		const void *prop;

		prop = of_get_property(child, "type", &len);
		if (prop && (len == sizeof(u32))) {
			/* must match type identifiers defined in DT schema */
			switch (*(u32 *)prop) {
			case 0x00000019: /* Driver*/
				err = mapphone_touch_driver_settings_init(
						tpdata, child, i2c_info);
				break;

			case 0x0000001A: /* IC */
				err = mapphone_touch_ic_settings_init(tpdata,
						child, i2c_info);
				break;

			case 0x0000001B: /* Firmware */
				err = mapphone_touch_firmware_init(tpdata,
						child);
				break;

			case 0x0000001C: /* Framework */
				err = mapphone_touch_framework_settings_init(
						tpdata, child);
				if (err < 0)
					break;

				if (tpdata->frmwrk->enable_vkeys)
					err = mapphone_touch_vkeys_init(
							child, i2c_info);
				break;

			case 0x0000001E: /* Tdat */
				err = mapphone_touch_tdat_init(tpdata, child);
				break;

			}
		}

		if (err < 0)
			break;
	}

	if (err < 0)
		goto touch_init_fail;

	return 0;

touch_init_fail:
	mapphone_touch_free(tpdata);
	pr_err("%s: Touch init failed for %s driver with error code %d.\n",
			__func__, i2c_info->type, err);
	return -ENOENT;
}

static int __init mapphone_touch_driver_settings_init(
		struct touch_platform_data *tpdata,
		struct device_node *dtnode,
		struct i2c_board_info *i2c_info)
{
	int err = 0;
	const void *prop;
	int size = 0;

	prop = of_get_property(dtnode, "touch_driver_flags", &size);
	if (prop != NULL && size > 0)
		tpdata->flags = *((uint16_t *) prop);
	else
		tpdata->flags = 0;

	return err;
}

static int __init mapphone_touch_ic_settings_init(
		struct touch_platform_data *tpdata,
		struct device_node *dtnode,
		struct i2c_board_info *i2c_info)
{
	int err = 0;
	const void *prop;
	int size = 0;
	const void *list_prop;
	int list_size = 0;
	char *prop_name = NULL;
	char *str_num = NULL;
	const char str1[] = "tsett";
	const char str2[] = "_tag";
	uint8_t tsett_num;
	int i = 0;

	prop = of_get_property(dtnode, "i2c_addrs", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: I2C address data is missing.\n", __func__);
		err = -ENOENT;
		goto touch_ic_settings_init_fail;
	} else if (size > ARRAY_SIZE(tpdata->addr)) {
		pr_err("%s: Too many I2C addresses provided.\n", __func__);
		err = -E2BIG;
		goto touch_ic_settings_init_fail;
	}

	for (i = 0; i < size; i++)
		tpdata->addr[i] = ((uint8_t *)prop)[i];

	for (i = size; i < ARRAY_SIZE(tpdata->addr); i++)
		tpdata->addr[i] = 0;

	list_prop = of_get_property(dtnode, "tsett_list", &list_size);
	if (list_prop == NULL || size <= 0) {
		pr_err("%s: No settings list provided.\n", __func__);
		err = -ENOENT;
		goto touch_ic_settings_init_fail;
	}

	prop_name = kzalloc(sizeof(str1) + (sizeof(char) * 4) +
					sizeof(str2), GFP_KERNEL);
	if (prop_name == NULL) {
		pr_err("%s: No memory for prop_name.\n", __func__);
		err = -ENOMEM;
		goto touch_ic_settings_init_fail;
	}

	str_num = kzalloc(sizeof(char) * 4, GFP_KERNEL);
	if (str_num == NULL) {
		pr_err("%s: No memory for str_num.\n", __func__);
		err = -ENOMEM;
		goto touch_ic_settings_init_fail;
	}

	for (i = 0; i < ARRAY_SIZE(tpdata->sett); i++)
		tpdata->sett[i] = NULL;

	for (i = 0; i < list_size; i++) {
		tsett_num = ((uint8_t *)list_prop)[i];
		err = snprintf(str_num, 3, "%hu", tsett_num);
		if (err < 0) {
			pr_err("%s: Error in snprintf converting %hu.\n",
					__func__, tsett_num);
			goto touch_ic_settings_init_fail;
		}
		prop_name[0] = '\0';
		strncat(prop_name, str1, sizeof(str1));
		strncat(prop_name, str_num, 3);

		prop = of_get_property(dtnode, prop_name, &size);
		if (prop == NULL || size <= 0) {
			pr_err("%s: Entry %s from tsett_list is missing.\n",
					__func__, prop_name);
			err = -ENOENT;
			goto touch_ic_settings_init_fail;
		}

		tpdata->sett[tsett_num] = kzalloc(sizeof(struct touch_settings),
				GFP_KERNEL);
		if (tpdata->sett[tsett_num] == NULL) {
			pr_err("%s: Unable to create tsett node %s.\n",
					__func__, str_num);
			err = -ENOMEM;
			goto touch_ic_settings_init_fail;
		}

		tpdata->sett[tsett_num]->size = size;
		tpdata->sett[tsett_num]->data = kmemdup(prop, size,
				GFP_KERNEL);

		if (!tpdata->sett[tsett_num]->data) {
			pr_err("%s: unable to allocate memory for sett data\n",
					__func__);
			err = -ENOMEM;
			goto touch_ic_settings_init_fail;
		}

		strncat(prop_name, str2, sizeof(str2));

		prop = of_get_property(dtnode, prop_name, &size);
		if (prop != NULL && size > 0)
			tpdata->sett[tsett_num]->tag = *((uint8_t *)prop);
		else
			tpdata->sett[tsett_num]->tag = 0;
	}

touch_ic_settings_init_fail:
	kfree(prop_name);
	kfree(str_num);

	return err;
}

static int __init mapphone_touch_firmware_init(
		struct touch_platform_data *tpdata,
		struct device_node *dtnode)
{
	int err = 0;
	const void *prop;
	int size = 0;

	tpdata->fw =
		kzalloc(sizeof(struct touch_firmware), GFP_KERNEL);
	if (tpdata->fw == NULL) {
		pr_err("%s: Unable to create fw node.\n", __func__);
		err = -ENOMEM;
		goto touch_firmware_init_fail;
	}

	prop = of_get_property(dtnode, "fw_image", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: Firmware image is missing.\n", __func__);
		err = -ENOENT;
		goto touch_firmware_init_fail;
	} else {
		tpdata->fw->img = kmemdup(prop, size, GFP_KERNEL);
		tpdata->fw->size = size;

		if (!tpdata->fw->img) {
			pr_err("%s: unable to allocate memory for FW image\n",
					__func__);
			err = -ENOMEM;
			goto touch_firmware_init_fail;
		}
	}

	prop = of_get_property(dtnode, "fw_version", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: Firmware version is missing.\n", __func__);
		err = -ENOENT;
		goto touch_firmware_init_fail;
	} else {
		tpdata->fw->ver = kmemdup(prop, size, GFP_KERNEL);
		tpdata->fw->vsize = size;

		if (!tpdata->fw->ver) {
			kfree(tpdata->fw->img);

			pr_err("%s: unable to allocate memory for FW version\n",
					__func__);
			err = -ENOMEM;
			goto touch_firmware_init_fail;
		}
	}

touch_firmware_init_fail:
	return err;
}

static int __init mapphone_touch_tdat_init(
		struct touch_platform_data *tpdata,
		struct device_node *dtnode)
{
	int err = 0;
	const void *prop;
	int size = 0;

	prop = of_get_property(dtnode, "tdat_filename", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: tdat file name is missing.\n", __func__);
		err = -ENOENT;
		goto touch_tdat_init_fail;
	} else {
		tpdata->filename = kstrndup((char *)prop, size, GFP_KERNEL);

		if (!tpdata->filename) {
			pr_err("%s: unable to allocate memory for "
					"tdat file name\n", __func__);
			err = -ENOMEM;
			goto touch_tdat_init_fail;
		}
	}

touch_tdat_init_fail:
	return err;
}

static int __init mapphone_touch_framework_settings_init(
		struct touch_platform_data *tpdata,
		struct device_node *dtnode)
{
	int err = 0;
	const void *prop;
	int size = 0;

	tpdata->frmwrk =
		kzalloc(sizeof(struct touch_framework), GFP_KERNEL);
	if (tpdata->frmwrk == NULL) {
		pr_err("%s: Unable to create frmwrk node.\n",
			__func__);
		err = -ENOMEM;
		goto touch_framework_settings_fail;
	}

	prop = of_get_property(dtnode, "abs_params", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: Abs parameters are missing.\n", __func__);
		err = -ENOENT;
		goto touch_framework_settings_fail;
	} else {
		tpdata->frmwrk->abs = kmemdup(prop, size, GFP_KERNEL);
		tpdata->frmwrk->size = size/sizeof(uint16_t);

		if (!tpdata->frmwrk->abs) {
			pr_err("%s: unable to allocate memory for abs_params\n",
					__func__);
			err = -ENOMEM;
			goto touch_framework_settings_fail;
		}
	}

	prop = of_get_property(dtnode, "enable_touch_vkeys", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: Vkeys flag is missing.\n", __func__);
		err = -ENOENT;
		goto touch_framework_settings_fail;
	} else {
		tpdata->frmwrk->enable_vkeys = *((uint8_t *)prop);

		if (!tpdata->frmwrk->enable_vkeys) {
			kfree(tpdata->frmwrk->abs);

			pr_err("%s: unable to allocate memory for abs_params\n",
					__func__);
			err = -ENOMEM;
			goto touch_framework_settings_fail;
		}
	}

touch_framework_settings_fail:
	return err;
}

static void __init mapphone_touch_free(struct touch_platform_data *tpdata)
{
	int i = 0;

	if (tpdata != NULL) {
		for (i = 0; i < ARRAY_SIZE(tpdata->sett); i++) {
			if (tpdata->sett[i] != NULL) {
				kfree(tpdata->sett[i]->data);
				kfree(tpdata->sett[i]);
				tpdata->sett[i] = NULL;
			}
		}

		if (tpdata->frmwrk != NULL) {
			kfree(tpdata->frmwrk->abs);
			kfree(tpdata->frmwrk);
			tpdata->frmwrk = NULL;
		}

		kfree(tpdata->fw->img);
		kfree(tpdata->fw->ver);
	}

	kfree(mapphone_touch_vkeys_init);

	return;
}

static int __init mapphone_touch_vkeys_init(
		struct device_node *dtnode,
		struct i2c_board_info *i2c_info)
{
	int err = 0;
	const void *prop;
	int size = 0;
	const char str1[] = "virtualkeys.";
	char *filename = NULL;
	struct kobject *prop_kobj = NULL;
	char dirname[64];
	static int counter;

	prop = of_get_property(dtnode, "vkey_map", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: Virtual keymap is missing.\n", __func__);
		err = -ENOENT;
		goto touch_vkeys_init_fail;
	} else {
		mapphone_touch_vkeys_map = kmemdup(prop, size, GFP_KERNEL);
		mapphone_touch_vkeys_map_size = size/sizeof(uint16_t);

		if (!mapphone_touch_vkeys_map) {
			pr_err("%s: unable to allocate space for vkeys map\n",
					__func__);
			err = -ENOMEM;
			goto touch_vkeys_init_fail;
		}
	}

	filename = kzalloc(sizeof(str1) + (sizeof(char) * (I2C_NAME_SIZE + 1)),
				GFP_KERNEL);
	if (filename == NULL) {
		pr_err("%s: No memory for filename.\n", __func__);
		err = -ENOMEM;
		goto touch_vkeys_init_fail;
	}
	strncat(filename, str1, sizeof(str1));
	strncat(filename, i2c_info->type, I2C_NAME_SIZE);
	mapphone_touch_vkeys_attr.attr.name = filename;

	if (counter) {
		pr_err("%s: There are multiple virtual key maps\n", __func__);
		snprintf(dirname, sizeof(dirname), "board_properties_%d",
				counter);
	} else
		strncpy(dirname, "board_properties", sizeof(dirname));

	prop_kobj = kobject_create_and_add(dirname, NULL);
	if (prop_kobj == NULL) {
		pr_err("%s: Unable to create and add %s.\n", __func__, dirname);
		err = -ENOMEM;
		goto touch_vkeys_init_fail;
	}

	err = sysfs_create_group(prop_kobj, &mapphone_attribute_group);
	if (err < 0) {
		pr_err("%s: Failed to create sysfs group.\n", __func__);
		kobject_put(prop_kobj);
		goto touch_vkeys_init_fail;
	}

	goto touch_vkeys_init_pass;

touch_vkeys_init_fail:
	kfree(filename);

touch_vkeys_init_pass:
	counter = counter + 1;
	return err;
}

static const uint16_t *mapphone_touch_vkeys_map;
static int mapphone_touch_vkeys_map_size;

static struct attribute_group mapphone_attribute_group = {
	.attrs = mapphone_attributes,
};

static struct attribute *mapphone_attributes[] = {
	&mapphone_touch_vkeys_attr.attr,
	NULL,
};

static struct kobj_attribute mapphone_touch_vkeys_attr = {
	.attr = {
		.name = NULL,
		.mode = S_IRUGO,
	},
	.show = &mapphone_touch_vkeys_kobj_output,
};

static ssize_t mapphone_touch_vkeys_kobj_output(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int err = 0;
	const uint16_t *map;
	int size = 0;
	int i = 0;

	map = mapphone_touch_vkeys_map;
	size = mapphone_touch_vkeys_map_size;

	if (map == NULL) {
		pr_err("%s Virtual keymap is missing!\n", __func__);
		err = -ENODATA;
		goto touch_vkeys_kobj_output_fail;
	}

	if ((size <= 0) || ((size % 5) != 0)) {
		pr_err("%s: Virtual keymap is invalid.\n", __func__);
		err = -EINVAL;
		goto touch_vkeys_kobj_output_fail;
	}

	for (i = 0; i < size; i++) {
		if (i % 5 == 0) {
			err = sprintf(buf, "%s" __stringify(EV_KEY) ":", buf);
			if (err < 0) {
				pr_err("%s: Error in ev_key sprintf %d.\n",
					__func__, i);
				goto touch_vkeys_kobj_output_fail;
			}
		}

		err = sprintf(buf, "%s%hu:", buf, map[i]);
		if (err < 0) {
			pr_err("%s: Error in sprintf %d.\n", __func__, i);
			goto touch_vkeys_kobj_output_fail;
		}
	}

	buf[err-1] = '\n';
	buf[err] = '\0';

touch_vkeys_kobj_output_fail:
	return (ssize_t) err;
}
