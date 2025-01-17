/*
 * drivers/thermal/sunxi_thermal/sunxi_ths_combine.c
 *
 * Copyright (C) 2013-2024 allwinner.
 *	JiaRui Xiao<xiaojiarui@allwinnertech.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define NEED_DEBUG (0)

#if NEED_DEBUG
#define DEBUG
#endif

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/input.h>
#include <linux/of_gpio.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/thermal.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#include "sunxi_ths.h"
#include "sunxi_ths_core.h"

#define thsprintk(fmt, arg...) \
	pr_debug("%s()%d - "fmt, __func__, __LINE__, ##arg)

static LIST_HEAD(controller_list);
static DEFINE_MUTEX(controller_list_lock);

static const char * const combine_types[] = {
	[COMBINE_MAX_TEMP]	= "max",
	[COMBINE_AVG_TMP]	= "avg",
	[COMBINE_MIN_TMP]	= "min",
};

static enum combine_ths_temp_type get_combine_type(const char *t)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(combine_types); i++)
		if (!strcasecmp(t, combine_types[i])) {
			return i;
		}
	return 0;
}

static int sunxi_combine_get_temp(void *data, int *temperature)
{
	struct sunxi_ths_sensor *sensor = data;
	struct sunxi_ths_controller *controller = sensor->combine->controller;
	int i, ret, is_suspend;
	u32 sensor_id;
	int temp = 0, taget = 0;

	is_suspend = atomic_read(&sensor->is_suspend);

	if (!is_suspend) {
		switch (sensor->combine->type) {
		case COMBINE_MAX_TEMP:
			for (i = 0, taget = -40; i < sensor->combine->combine_ths_count; i++) {
				sensor_id = sensor->combine->combine_ths_id[i];
				ret = controller->ops->get_temp(controller, sensor_id, &temp);
				if (ret)
					return ret;
				if (temp > taget)
					taget = temp;
			}
			break;
		case COMBINE_AVG_TMP:
			for (i = 0, taget = 0; i < sensor->combine->combine_ths_count; i++) {
				sensor_id = sensor->combine->combine_ths_id[i];
				ret = controller->ops->get_temp(controller, sensor_id, &temp);
				if (ret)
					return ret;
				taget += temp;
			}
			do_div(taget, sensor->combine->combine_ths_count);
			break;
		case COMBINE_MIN_TMP:
			for (i = 0, taget = 180; i < sensor->combine->combine_ths_count; i++) {
				sensor_id = sensor->combine->combine_ths_id[i];
				ret = controller->ops->get_temp(controller, sensor_id, &temp);
				if (ret)
					return ret;
				if (temp < taget)
					taget = temp;
			}
			break;
		default:
			break;
		}
		*temperature = taget;
		sensor->last_temp = taget;
	}else{
		*temperature = sensor->last_temp;
	}
	thsprintk("get temp %d\n", (*temperature));
	return 0;
}

static struct sunxi_ths_controller *
combine_find_controller(struct device_node *np)
{
	struct sunxi_ths_controller *controller;
	struct device_node *pnp = of_get_parent(np);
	if (IS_ERR_OR_NULL(pnp)) {
		pr_err("ths combine: get parent err\n");
		return NULL;
	}
	mutex_lock(&controller_list_lock);
	list_for_each_entry(controller, &controller_list, node){
		if(pnp == controller->dev->of_node){
			mutex_unlock(&controller_list_lock);
			return controller;
		}
	}
	mutex_unlock(&controller_list_lock);
	return NULL;
}

struct sunxi_ths_controller *
sunxi_ths_controller_register(struct device *dev ,struct sunxi_ths_controller_ops *ops, void *data)
{
	struct sunxi_ths_controller *controller = NULL;
	struct device_node *np = dev->of_node;
	struct device_node *combine_np = NULL;
	struct platform_device *combine_dev = NULL;
	int i = 0, combine_num = 0;
	char combine_name[50];

	controller = kzalloc(sizeof(*controller), GFP_KERNEL);
	if (IS_ERR_OR_NULL(controller)) {
		pr_err("ths controller: not enough memory for controller data\n");
		return NULL;
	}

	if (of_property_read_u32(np, "combine_num", &combine_num)) {
		pr_err("%s: get combine_num failed\n", __func__);
		return NULL;
	}

	controller->dev = dev;
	controller->ops = ops;
	controller->data = data;
	controller->combine_num = combine_num;
	atomic_set(&controller->is_suspend, 0);
	atomic_set(&controller->usage, 0);
	mutex_init(&controller->lock);
	INIT_LIST_HEAD(&controller->combine_list);
	mutex_lock(&controller_list_lock);
	list_add_tail(&controller->node, &controller_list);
	mutex_unlock(&controller_list_lock);
	for_each_child_of_node(np, combine_np) {
		combine_dev = kzalloc(sizeof(*combine_dev), GFP_KERNEL);

		if (IS_ERR_OR_NULL(combine_dev)) {
			pr_err("combine_dev: not enough memory for combine_dev data\n");
			return NULL;
		}

		combine_dev->dev.of_node = combine_np;
		sprintf(combine_name, "sunxi_ths_combine_%d", i);
		combine_dev->name = combine_name;
		combine_dev->id = PLATFORM_DEVID_NONE;
		platform_device_register(combine_dev);
		i++;
	}

	return controller;
}
EXPORT_SYMBOL(sunxi_ths_controller_register);

void sunxi_ths_controller_unregister(struct sunxi_ths_controller *controller)
{
	struct sunxi_ths_sensor *sensor;
	struct platform_device *pdev;
	list_for_each_entry(sensor, &controller->combine_list, node) {
		pdev = sensor->pdev;
		platform_device_unregister(pdev);
		kfree(pdev);
	}
	mutex_lock(&controller_list_lock);
	list_del(&controller->node);
	mutex_unlock(&controller_list_lock);
	kfree(controller);
}
EXPORT_SYMBOL(sunxi_ths_controller_unregister);

static int sunxi_combine_parse(struct sunxi_ths_sensor *sensor)
{
	struct device_node *np = NULL;
	struct sunxi_ths_combine_disc *combine;
	const char *type = NULL;
	int combine_sensor_num = 0;
	int i = 0;

	combine = kzalloc(sizeof(*combine), GFP_KERNEL);
	if (IS_ERR_OR_NULL(combine)) {
		pr_err("ths combine: not enough memory for combine data\n");
		return -ENOMEM;
	}
	np = sensor->pdev->dev.of_node;
	combine->controller = combine_find_controller(np);
	if (!combine->controller) {
		pr_err("sensor find no controller\n");
		goto parse_fail;
	}
	/* getting the combine sensor have sensor num*/
	if (of_property_read_u32(np, "combine_sensor_num", &combine_sensor_num)) {
		pr_err("%s: get sensor_num failed\n", __func__);
		return -EBUSY;
	} else {
		combine->combine_ths_count = combine_sensor_num;
	}

	/* getting the combine sensor how to calcular all the sensor temp */
	if (of_property_read_string(np, "combine_sensor_temp_type", &type)) {
		pr_err("%s: get combine type failed\n", __func__);
		return -EBUSY;
	} else {
		combine->type = get_combine_type(type);
	}

	/* getting the combine sensor where focus on */
	if (of_property_read_string(np, "combine_sensor_type", &combine->combine_ths_type)) {
		pr_err("%s: get sensor_num failed\n", __func__);
		return -EBUSY;
	}

	/* getting the combine sensor include all sensor id */
	for (i = 0; i < combine->combine_ths_count; i++) {
		if (of_property_read_u32_index(np, "combine_sensor_id",
					i, &(combine->combine_ths_id[i]))) {
			pr_err("node combine chn get failed!\n");
			goto parse_fail;
		}
	}

	sensor->combine = combine;
	list_add_tail(&sensor->node, &combine->controller->combine_list);
	return 0;
parse_fail:
	kfree(combine);
	return -ENOMEM;
}

static int sunxi_combine_probe(struct platform_device *pdev)
{
	int err = 0, id = 0;
	struct sunxi_ths_sensor *sensor;
	struct sunxi_ths_data *ths_data;

	thsprintk("sunxi ths sensor probe start !\n");

	if (!pdev->dev.of_node) {
		pr_err("can 't get the combine ths node!\n");
		return -EBUSY;
	}
	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (IS_ERR_OR_NULL(sensor)) {
		pr_err("ths combine: not enough memory for sensor data\n");
		return -EBUSY;
	}
	sensor->pdev = pdev;

	err = sunxi_combine_parse(sensor);
	if (err)
		goto fail;

	sscanf(pdev->name, "sunxi_ths_combine_%d", &id);
	sensor->sensor_id = id;
	atomic_add(1, &sensor->combine->controller->usage);
	sensor->tz = thermal_zone_of_sensor_register(&pdev->dev,
				id, sensor, sunxi_combine_get_temp, NULL);
	if (IS_ERR(sensor->tz)) {
		pr_err("sunxi ths sensor register err!\n");
		goto fail1;
	}

	ths_data = (struct sunxi_ths_data *)sensor->combine->controller->data;
	ths_data->comb_sensor[id] = sensor;

	dev_set_drvdata(&pdev->dev, sensor);
	atomic_set(&sensor->is_suspend, 0);

	if (sensor->tz->ops->set_mode)
		sensor->tz->ops->set_mode(sensor->tz, THERMAL_DEVICE_ENABLED);
	else
		thermal_zone_device_update(sensor->tz);

	thsprintk("%s probe end!\n", pdev->name);
	return 0;
fail1:
	kfree(sensor->combine);
fail:
	kfree(sensor);
	return -EBUSY;
}

static int sunxi_combine_remove(struct platform_device *pdev)
{
	struct sunxi_ths_sensor *sensor;

	sensor = dev_get_drvdata(&pdev->dev);
	thermal_zone_of_sensor_unregister(&pdev->dev, sensor->tz);
	kfree(sensor->combine);
	kfree(sensor);
	return 0;
}

#ifdef CONFIG_PM
static int sunxi_combine_suspend(struct device *dev)
{
	struct sunxi_ths_sensor *sensor = dev_get_drvdata(dev);
	struct sunxi_ths_controller *controller = sensor->combine->controller;

	thsprintk("enter: sunxi_ths_suspend.\n");
	atomic_set(&sensor->is_suspend, 1);
	if (atomic_sub_return(1, &controller->usage) == 0)
		if (controller->ops->suspend)
			return controller->ops->suspend(controller);

	return 0;
}

static int sunxi_combine_resume(struct device *dev)
{
	struct sunxi_ths_sensor *sensor = dev_get_drvdata(dev);
	struct sunxi_ths_controller *controller = sensor->combine->controller;
	int max_combine;

	thsprintk("enter: sunxi_ths_resume.\n");
	max_combine = sensor->combine->controller->combine_num;
	if (atomic_add_return(1, &controller->usage) == max_combine)
		if (controller->ops->resume)
			controller->ops->resume(controller);

	atomic_set(&sensor->is_suspend, 0);

	return 0;
}

static const struct dev_pm_ops sunxi_combine_pm_ops = {
	.suspend        = sunxi_combine_suspend,
	.resume         = sunxi_combine_resume,
};
#endif

static const struct of_device_id of_sunxi_combine_ths_match[] = {
	{ .compatible = "allwinner,ths_combine0" },
	{ .compatible = "allwinner,ths_combine1" },
	{ .compatible = "allwinner,ths_combine2" },
	{ /* end */ }
};

static struct platform_driver sunxi_combine_driver = {
	.probe  = sunxi_combine_probe,
	.remove = sunxi_combine_remove,
	.driver = {
		.name   = SUNXI_THS_COMBINE_NAME,
		.owner  = THIS_MODULE,
	.of_match_table = of_sunxi_combine_ths_match,
#ifdef CONFIG_PM
		.pm	= &sunxi_combine_pm_ops,
#endif
	},
};

static int __init sunxi_ths_combine_init(void)
{
	return platform_driver_register(&sunxi_combine_driver);
}

static void __exit sunxi_ths_combine_exit(void)
{
	platform_driver_unregister(&sunxi_combine_driver);
	mutex_destroy(&controller_list_lock);
}

subsys_initcall_sync(sunxi_ths_combine_init);
module_exit(sunxi_ths_combine_exit);
MODULE_DESCRIPTION("SUNXI combine thermal sensor driver");
MODULE_AUTHOR("QIn");
MODULE_LICENSE("GPL v2");
