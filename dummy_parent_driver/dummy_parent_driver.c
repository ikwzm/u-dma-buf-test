#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

static int dummy_parent_probe(struct platform_device* pdev)
{
	dev_info(&pdev->dev, "driver probe start.\n");
	dev_info(&pdev->dev, "driver probe done.\n");
	return 0;
}


static int dummy_parent_remove(struct platform_device* pdev)
{
	dev_info(&pdev->dev, "driver remove start.\n");
	dev_info(&pdev->dev, "driver remove done.\n");
	return 0;
}

static const struct of_device_id dummy_parent_of_match_table[] = {
	{ .compatible = "ikwzm,dummy_parent" },
	{ },
};
MODULE_DEVICE_TABLE(of, dummy_parent_of_match_table);

static struct platform_driver dummy_parent_driver = {
	.driver = {
		.name = "dummy_parent",
		.of_match_table = dummy_parent_of_match_table,
	},
	.probe  = dummy_parent_probe,
	.remove = dummy_parent_remove,
};
module_platform_driver(dummy_parent_driver);

MODULE_DESCRIPTION("Dummy Parent Driver");
MODULE_AUTHOR("ikwzm");
MODULE_LICENSE("Dual BSD/GPL");

