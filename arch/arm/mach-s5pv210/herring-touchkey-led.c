/*
 * Copyright (C) 2010 Samsung Electronics Co. Ltd. All Rights Reserved.
 * Author: Rom Lemarchand <rlemarchand@sta.samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/device.h>
#include <asm/mach-types.h>

#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH_USE_BLN
#include <linux/miscdevice.h>
#define BACKLIGHTNOTIFICATION_VERSION 8

static bool bln_enabled = false; // indicates if BLN function is enabled/allowed (default: false, app enables it on boot)
static bool BacklightNotification_ongoing = false; // indicates ongoing LED Notification
static bool bln_blink_enabled = false;	// indicates blink is set
static bool herring_touchkey_suspended = false;
#endif

static int led_gpios[] = { 2, 3, 6, 7 };

static void herring_touchkey_led_onoff(int onoff)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(led_gpios); i++)
		gpio_direction_output(S5PV210_GPJ3(led_gpios[i]), !!onoff);
}

static void herring_touchkey_led_early_suspend(struct early_suspend *h)
{
	herring_touchkey_led_onoff(0);
	herring_touchkey_suspended = true;
}

static void herring_touchkey_led_late_resume(struct early_suspend *h)
{
	herring_touchkey_led_onoff(1);
	herring_touchkey_suspended = false;
}

static struct early_suspend early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend = herring_touchkey_led_early_suspend,
	.resume = herring_touchkey_led_late_resume,
};

#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH_USE_BLN
/* bln start */

static void enable_touchkey_backlights(void)
{
	herring_touchkey_led_onoff(1);
}

static void disable_touchkey_backlights(void)
{
	herring_touchkey_led_onoff(0);
}

static void enable_led_notification(void)
{
	if (bln_enabled) {
		enable_touchkey_backlights();
		pr_info("%s: notification led enabled\n", __FUNCTION__);
	}
}

static void disable_led_notification(void)
{
	pr_info("%s: notification led disabled\n", __FUNCTION__);

	bln_blink_enabled = false;

	if (herring_touchkey_suspended)
		disable_touchkey_backlights();

	BacklightNotification_ongoing = false;
}

static ssize_t backlightnotification_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (bln_enabled ? 1 : 0));
}

static ssize_t backlightnotification_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;
	if(sscanf(buf, "%u\n", &data) == 1) {
		pr_devel("%s: %u \n", __FUNCTION__, data);
		if(data == 0 || data == 1) {
			if(data == 1) {
				pr_info("%s: backlightnotification function enabled\n", __FUNCTION__);
				bln_enabled = true;
			}

			if(data == 0) {
				pr_info("%s: backlightnotification function disabled\n", __FUNCTION__);
				bln_enabled = false;
				if (BacklightNotification_ongoing)
					disable_led_notification();
			}
		} else {
			pr_info("%s: invalid input range %u\n", __FUNCTION__, data);
		}
	} else {
		pr_info("%s: invalid input\n", __FUNCTION__);
	}

	return size;
}

static ssize_t notification_led_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", (BacklightNotification_ongoing ? 1 : 0));
}

static ssize_t notification_led_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1) {
		if (data == 0 || data == 1) {
			pr_devel("%s: %u \n", __FUNCTION__, data);
			if (data == 1)
				enable_led_notification();

			if (data == 0)
				disable_led_notification();
		} else {
			pr_info("%s: wrong input %u\n", __FUNCTION__, data);
		}
	} else {
		pr_info("%s: input error\n", __FUNCTION__);
	}

	return size;
}

static ssize_t blink_control_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (bln_blink_enabled ? 1 : 0));
}

static ssize_t blink_control_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1) {
		if (data == 0 || data == 1) {
			if (BacklightNotification_ongoing) {
				pr_devel("%s: %u \n", __FUNCTION__, data);
				if (data == 1) {
					bln_blink_enabled = true;
					disable_touchkey_backlights();
				}

				if(data == 0) {
					bln_blink_enabled = false;
					enable_touchkey_backlights();
				}
			}
		} else {
			pr_info("%s: wrong input %u\n", __FUNCTION__, data);
		}
	} else {
		pr_info("%s: input error\n", __FUNCTION__);
	}

	return size;
}

static ssize_t backlightnotification_version(struct device *dev, struct device_attribute *attr, char *buf) {
	return sprintf(buf, "%u\n", BACKLIGHTNOTIFICATION_VERSION);
}

static DEVICE_ATTR(blink_control, S_IRUGO | S_IWUGO , blink_control_read, blink_control_write);
static DEVICE_ATTR(enabled, S_IRUGO | S_IWUGO , backlightnotification_status_read, backlightnotification_status_write);
static DEVICE_ATTR(notification_led, S_IRUGO | S_IWUGO , notification_led_status_read, notification_led_status_write);
static DEVICE_ATTR(version, S_IRUGO , backlightnotification_version, NULL);

static struct attribute *bln_notification_attributes[] = {
		&dev_attr_blink_control.attr,
		&dev_attr_enabled.attr,
		&dev_attr_notification_led.attr,
		&dev_attr_version.attr,
		NULL
};

static struct attribute_group bln_notification_group = {
		.attrs  = bln_notification_attributes,
};

static struct miscdevice backlightnotification_device = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "backlightnotification",
};
/* bln end */
#endif


static int __init herring_init_touchkey_led(void)
{
	int i;
	int ret = 0;
	u32 gpio;

	if (!machine_is_herring() || system_rev < 0x10)
		return 0;

	for (i = 0; i < ARRAY_SIZE(led_gpios); i++) {
		gpio = S5PV210_GPJ3(led_gpios[i]);
		ret = gpio_request(gpio, "touchkey led");
		if (ret) {
			pr_err("Failed to request touchkey led gpio %d\n", i);
			goto err_req;
		}
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(gpio, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(gpio, S3C_GPIO_PULL_NONE);
	}

	herring_touchkey_led_onoff(1);

	register_early_suspend(&early_suspend);

#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH_USE_BLN
	pr_info("%s misc_register(%s)\n", __FUNCTION__, backlightnotification_device.name);
	ret = misc_register(&backlightnotification_device);
	if (ret) {
		pr_err("%s misc_register(%s) fail\n", __FUNCTION__,
			backlightnotification_device.name);
	} else {
		/* add the backlightnotification attributes */
		if (sysfs_create_group(&backlightnotification_device.this_device->kobj,
				&bln_notification_group) < 0) {
			pr_err("%s sysfs_create_group fail\n", __FUNCTION__);
			pr_err("Failed to create sysfs group for device (%s)!\n",
				backlightnotification_device.name);
		}
	}
#endif

	return 0;

err_req:
	while (--i >= 0)
		gpio_free(S5PV210_GPJ3(led_gpios[i]));
	return ret;
}

device_initcall(herring_init_touchkey_led);
