// SPDX-License-Identifier: GPL-2.0+
/*
 * Qualcomm MSM8917/MSM8937/MSM8940 TLMM pinctrl
 *
 * Based on pinctrl-apq8016.c. Pin layout from Linux pinctrl-msm8917.c:
 * 134 GPIOs with 0x1000 stride, then the SDC/QDSD pad groups.
 */

#include <dm.h>

#include "pinctrl-qcom.h"

#define MAX_PIN_NAME_LEN 32
#define MSM8917_NUM_GPIOS 134

static char pin_name[MAX_PIN_NAME_LEN] __section(".data");

static const char * const msm8917_special_pins[] = {
	"sdc1_clk", "sdc1_cmd", "sdc1_data", "sdc1_rclk",
	"sdc2_clk", "sdc2_cmd", "sdc2_data",
	"qdsd_clk", "qdsd_cmd", "qdsd_data0", "qdsd_data1", "qdsd_data2",
	"qdsd_data3",
};

#define SDC_PIN(pg_name, ctl, pull, drv) \
	{ .name = pg_name, .ctl_reg = ctl, .pull_bit = pull, .drv_bit = drv }

static const struct msm_special_pin_data msm8917_special_pins_data[] = {
	SDC_PIN("sdc1_clk",   0x10a000, 13, 6),
	SDC_PIN("sdc1_cmd",   0x10a000, 11, 3),
	SDC_PIN("sdc1_data",  0x10a000, 9, 0),
	SDC_PIN("sdc1_rclk",  0x10a000, 15, 0),
	SDC_PIN("sdc2_clk",   0x109000, 14, 6),
	SDC_PIN("sdc2_cmd",   0x109000, 11, 3),
	SDC_PIN("sdc2_data",  0x109000, 9, 0),
	SDC_PIN("qdsd_clk",   0x19c000, 3, 0),
	SDC_PIN("qdsd_cmd",   0x19c000, 8, 5),
	SDC_PIN("qdsd_data0", 0x19c000, 13, 10),
	SDC_PIN("qdsd_data1", 0x19c000, 18, 15),
	SDC_PIN("qdsd_data2", 0x19c000, 23, 20),
	SDC_PIN("qdsd_data3", 0x19c000, 28, 25),
};

static const struct pinctrl_function msm_pinctrl_functions[] = {
	{ "gpio", 0 },
	{ "blsp_uart2", 2 },
	{ "sdc2_cd", 0 },
};

static const char *msm8917_get_function_name(struct udevice *dev,
					     unsigned int selector)
{
	return msm_pinctrl_functions[selector].name;
}

static const char *msm8917_get_pin_name(struct udevice *dev,
					unsigned int selector)
{
	if (selector < MSM8917_NUM_GPIOS) {
		snprintf(pin_name, MAX_PIN_NAME_LEN, "gpio%u", selector);
		return pin_name;
	}

	return msm8917_special_pins[selector - MSM8917_NUM_GPIOS];
}

static int msm8917_get_function_mux(__maybe_unused unsigned int pin,
				    unsigned int selector)
{
	return msm_pinctrl_functions[selector].val;
}

static const struct msm_pinctrl_data msm8917_data = {
	.pin_data = {
		.pin_count = MSM8917_NUM_GPIOS + ARRAY_SIZE(msm8917_special_pins),
		.special_pins_start = MSM8917_NUM_GPIOS,
		.special_pins_data = msm8917_special_pins_data,
	},
	.functions_count = ARRAY_SIZE(msm_pinctrl_functions),
	.get_function_name = msm8917_get_function_name,
	.get_function_mux = msm8917_get_function_mux,
	.get_pin_name = msm8917_get_pin_name,
};

static const struct udevice_id msm_pinctrl_ids[] = {
	{ .compatible = "qcom,msm8917-pinctrl", .data = (ulong)&msm8917_data },
	{ /* Sentinel */ }
};

U_BOOT_DRIVER(pinctrl_msm8917) = {
	.name		= "pinctrl_msm8917",
	.id		= UCLASS_NOP,
	.of_match	= msm_pinctrl_ids,
	.ops		= &msm_pinctrl_ops,
	.bind		= msm_pinctrl_bind,
	.flags		= DM_FLAG_PRE_RELOC,
};
