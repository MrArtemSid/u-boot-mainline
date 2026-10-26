// SPDX-License-Identifier: GPL-2.0+
/*
 * Qualcomm PSHOLD reset driver
 *
 * Copyright (c) 2024 Sartura Ltd.
 *
 * Author: Robert Marko <robert.marko@sartura.hr>
 * Based on the Linux msm-poweroff driver.
 *
 */

#include <dm.h>
#include <sysreset.h>
#include <asm/io.h>
#include <dm/ofnode.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <power/pmic.h>

#define PON_REV2			0x01
#define PON_PS_HOLD_RST_CTL		0x5a
#define PON_PS_HOLD_RST_CTL2		0x5b
#define  PON_PS_HOLD_ENABLE		BIT(7)
#define  PON_PS_HOLD_TYPE_MASK		0x0f
#define  PON_PS_HOLD_TYPE_WARM_RESET	1
#define  PON_PS_HOLD_TYPE_SHUTDOWN	4
#define  PON_PS_HOLD_TYPE_HARD_RESET	7

struct qcom_pshold_priv {
	phys_addr_t base;
};

static const char * const qcom_pon_compats[] = {
	"qcom,pm8916-pon",
	"qcom,pm8941-pon",
	"qcom,pms405-pon",
};

/*
 * The PMIC decides what happens when PS_HOLD is dropped. Program it the
 * same way Linux does (pm8941-pwrkey) instead of relying on whatever the
 * previous bootloader left there, otherwise a reset may turn into a power
 * off or the other way round.
 */
static void qcom_pshold_set_type(enum sysreset_t type)
{
	struct udevice *pmic;
	ofnode pon = ofnode_null();
	unsigned int enable_reg;
	u32 base;
	int i, rev, reset_type;

	for (i = 0; i < ARRAY_SIZE(qcom_pon_compats); i++) {
		pon = ofnode_by_compatible(ofnode_null(), qcom_pon_compats[i]);
		if (ofnode_valid(pon))
			break;
	}
	if (!ofnode_valid(pon) || ofnode_read_u32(pon, "reg", &base))
		return;
	if (uclass_get_device_by_ofnode(UCLASS_PMIC, ofnode_get_parent(pon),
					&pmic))
		return;

	switch (type) {
	case SYSRESET_POWER_OFF:
		reset_type = PON_PS_HOLD_TYPE_SHUTDOWN;
		break;
	case SYSRESET_WARM:
		reset_type = PON_PS_HOLD_TYPE_WARM_RESET;
		break;
	default:
		reset_type = PON_PS_HOLD_TYPE_HARD_RESET;
		break;
	}

	/* PMICs with revision 0 have the enable bit in the same register */
	rev = pmic_reg_read(pmic, base + PON_REV2);
	enable_reg = rev > 0 ? PON_PS_HOLD_RST_CTL2 : PON_PS_HOLD_RST_CTL;

	pmic_clrsetbits(pmic, base + enable_reg, PON_PS_HOLD_ENABLE, 0);
	/* Updates of PON_PS_HOLD_ENABLE require 3 sleep clock cycles */
	udelay(500);
	pmic_clrsetbits(pmic, base + PON_PS_HOLD_RST_CTL,
			PON_PS_HOLD_TYPE_MASK, reset_type);
	pmic_clrsetbits(pmic, base + enable_reg, 0, PON_PS_HOLD_ENABLE);
}

static int qcom_pshold_request(struct udevice *dev, enum sysreset_t type)
{
	struct qcom_pshold_priv *priv = dev_get_priv(dev);

	if (CONFIG_IS_ENABLED(DM_PMIC))
		qcom_pshold_set_type(type);

	writel(0, priv->base);
	mdelay(10000);

	return 0;
}

static struct sysreset_ops qcom_pshold_ops = {
	.request = qcom_pshold_request,
};

static int qcom_pshold_probe(struct udevice *dev)
{
	struct qcom_pshold_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr(dev);
	return priv->base == FDT_ADDR_T_NONE ? -EINVAL : 0;
}

static const struct udevice_id qcom_pshold_ids[] = {
	{ .compatible = "qcom,pshold", },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(qcom_pshold) = {
	.name 		= "qcom_pshold",
	.id 		= UCLASS_SYSRESET,
	.of_match 	= qcom_pshold_ids,
	.probe 		= qcom_pshold_probe,
	.priv_auto	= sizeof(struct qcom_pshold_priv),
	.ops 		= &qcom_pshold_ops,
};
