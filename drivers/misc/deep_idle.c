/* drivers/misc/deep_idle.c
 *
 * Copyright 2011  Ezekeel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/deep_idle.h>

#define DEEPIDLE_VERSION 1

static bool deepidle_enabled = false;

static ssize_t deepidle_status_read(struct device * dev, struct device_attribute * attr, char * buf)
{
    return sprintf(buf, "%u\n", (deepidle_enabled ? 1 : 0));
}

static ssize_t deepidle_status_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    unsigned int data;

    if(sscanf(buf, "%u\n", &data) == 1) 
	{
	    if (data == 1)
		{
		    pr_info("%s: DEEPIDLE enabled\n", __FUNCTION__);

		    deepidle_enabled = true;
		} 
	    else if (data == 0)
		{
		    pr_info("%s: DEEPIDLE disabled\n", __FUNCTION__);

		    deepidle_enabled = false;
		}
	    else 
		{
		    pr_info("%s: invalid input range %u\n", __FUNCTION__, data);
		}
	} 
    else 
	{
	    pr_info("%s: invalid input\n", __FUNCTION__);
	}

    return size;
}

static ssize_t deepidle_version(struct device * dev, struct device_attribute * attr, char * buf)
{
    return sprintf(buf, "%u\n", DEEPIDLE_VERSION);
}

static DEVICE_ATTR(enabled, S_IRUGO | S_IWUGO, deepidle_status_read, deepidle_status_write);
static DEVICE_ATTR(version, S_IRUGO , deepidle_version, NULL);

static struct attribute *deepidle_attributes[] = 
    {
	&dev_attr_enabled.attr,
	&dev_attr_version.attr,
	NULL
    };

static struct attribute_group deepidle_group = 
    {
	.attrs  = deepidle_attributes,
    };

static struct miscdevice deepidle_device = 
    {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "deepidle",
    };

bool deepidle_is_enabled(void)
{
    return deepidle_enabled;
}
EXPORT_SYMBOL(deepidle_is_enabled);

static int __init deepidle_init(void)
{
    int ret;

    pr_info("%s misc_register(%s)\n", __FUNCTION__, deepidle_device.name);

    ret = misc_register(&deepidle_device);

    if (ret) 
	{
	    pr_err("%s misc_register(%s) fail\n", __FUNCTION__, deepidle_device.name);

	    return 1;
	}

    if (sysfs_create_group(&deepidle_device.this_device->kobj, &deepidle_group) < 0) 
	{
	    pr_err("%s sysfs_create_group fail\n", __FUNCTION__);
	    pr_err("Failed to create sysfs group for device (%s)!\n", deepidle_device.name);
	}

    return 0;
}

device_initcall(deepidle_init);
