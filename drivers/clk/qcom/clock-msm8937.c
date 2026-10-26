// SPDX-License-Identifier: BSD-3-Clause
/*
 * Clock driver for Qualcomm MSM8917/MSM8937/MSM8940 GCC
 *
 * Based on clock-apq8016.c. Register offsets of the SDCC, USB HS and
 * BLSP1 blocks are identical to MSM8916; clock IDs come from the
 * upstream qcom,gcc-msm8917.h binding shared by the whole family.
 */

#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <dt-bindings/clock/qcom,gcc-msm8917.h>

#include "clock-qcom.h"

#define GPLL0_MODE			0x21000
#define APCS_GPLL_ENA_VOTE		0x45000
#define APCS_CLOCK_BRANCH_ENA_VOTE	0x45004

#define SDCC1_APPS_CMD_RCGR		0x42004
#define SDCC2_APPS_CMD_RCGR		0x43004
#define USB_HS_SYSTEM_CMD_RCGR		0x41010

/*
 * Unlike MSM8916 the GPLL0 lock indication on MSM8937 is the
 * ACTIVE bit of GPLL0_MODE, GPLL0_STATUS reads back 0
 * (see msm8937_clock_override() in LK).
 */
static const struct pll_vote_clk gpll0_vote_clk = {
	.status = GPLL0_MODE,
	.status_bit = BIT(30),
	.ena_vote = APCS_GPLL_ENA_VOTE,
	.vote_bit = BIT(0),
};

static const struct gate_clk msm8937_clks[] = {
	GATE_CLK_POLLED(GCC_BLSP1_AHB_CLK,	0x45004, BIT(10), 0x01008),
	GATE_CLK_POLLED(GCC_PRNG_AHB_CLK,	0x45004, BIT(8),  0x13004),
	GATE_CLK_POLLED(GCC_SDCC1_AHB_CLK,	0x4201c, BIT(0),  0x4201c),
	GATE_CLK_POLLED(GCC_SDCC1_APPS_CLK,	0x42018, BIT(0),  0x42018),
	GATE_CLK_POLLED(GCC_SDCC1_ICE_CORE_CLK,	0x5d014, BIT(0),  0x5d014),
	GATE_CLK_POLLED(GCC_SDCC2_AHB_CLK,	0x4301c, BIT(0),  0x4301c),
	GATE_CLK_POLLED(GCC_SDCC2_APPS_CLK,	0x43018, BIT(0),  0x43018),
	GATE_CLK_POLLED(GCC_USB2A_PHY_SLEEP_CLK,	0x4102c, BIT(0), 0x4102c),
	GATE_CLK_POLLED(GCC_USB_HS_AHB_CLK,	0x41008, BIT(0),  0x41008),
	GATE_CLK_POLLED(GCC_USB_HS_PHY_CFG_AHB_CLK, 0x41030, BIT(0), 0x41030),
	GATE_CLK_POLLED(GCC_USB_HS_SYSTEM_CLK,	0x41004, BIT(0),  0x41004),
};

static const struct qcom_reset_map msm8937_gcc_resets[] = {
	[GCC_QUSB2_PHY_BCR]		= { 0x4103c },
	[GCC_USB_HS_BCR]		= { 0x41000 },
	[GCC_USB2_HS_PHY_ONLY_BCR]	= { 0x41034 },
	[GCC_MDSS_BCR]			= { 0x4d074 },
};

static const struct qcom_power_map msm8937_gdscs[] = {
	[MDSS_GDSC]	= { .reg = 0x4d078, .poll_gdscr = true },
};

/* XO 19.2 MHz and GPLL0 800 MHz rates from Linux gcc-msm8917.c */
static const struct freq_tbl ftbl_sdcc_apps_clk_src[] = {
	F(144000,    CFG_CLK_SRC_CXO,   16,  3, 25),
	F(400000,    CFG_CLK_SRC_CXO,   12,  1, 4),
	F(20000000,  CFG_CLK_SRC_GPLL0, 10,  1, 4),
	F(25000000,  CFG_CLK_SRC_GPLL0, 16,  1, 2),
	F(50000000,  CFG_CLK_SRC_GPLL0, 16,  0, 0),
	F(100000000, CFG_CLK_SRC_GPLL0, 8,   0, 0),
	F(177770000, CFG_CLK_SRC_GPLL0, 4.5, 0, 0),
	F(200000000, CFG_CLK_SRC_GPLL0, 4,   0, 0),
	{ }
};

static ulong msm8937_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	const struct freq_tbl *freq;
	u32 rcgr;

	switch (clk->id) {
	case GCC_SDCC1_APPS_CLK:
		rcgr = SDCC1_APPS_CMD_RCGR;
		break;
	case GCC_SDCC2_APPS_CLK:
		rcgr = SDCC2_APPS_CMD_RCGR;
		break;
	case GCC_USB_HS_SYSTEM_CLK:
		/* 800 MHz / 8 = 100 MHz, valid on 8917 and 8937 tables */
		clk_enable_gpll0(priv->base, &gpll0_vote_clk);
		clk_rcg_set_rate(priv->base, USB_HS_SYSTEM_CMD_RCGR, 8,
				 CFG_CLK_SRC_GPLL0);
		return 100000000;
	default:
		return 0;
	}

	freq = qcom_find_freq(ftbl_sdcc_apps_clk_src, rate);
	if (freq->src == CFG_CLK_SRC_GPLL0)
		clk_enable_gpll0(priv->base, &gpll0_vote_clk);
	clk_rcg_set_rate_mnd(priv->base, rcgr, freq->pre_div, freq->m,
			     freq->n, freq->src, 8);

	return freq->freq;
}

static int msm8937_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (clk->id >= priv->data->num_clks || !msm8937_clks[clk->id].reg) {
		log_debug("%s: unknown clk id %lu, ignoring\n", __func__,
			  clk->id);
		return 0;
	}

	return qcom_gate_clk_en(priv, clk->id);
}

static struct msm_clk_data msm8937_clk_data = {
	.set_rate = msm8937_set_rate,
	.enable = msm8937_enable,
	.clks = msm8937_clks,
	.num_clks = ARRAY_SIZE(msm8937_clks),
	.resets = msm8937_gcc_resets,
	.num_resets = ARRAY_SIZE(msm8937_gcc_resets),
	.power_domains = msm8937_gdscs,
	.num_power_domains = ARRAY_SIZE(msm8937_gdscs),
};

static const struct udevice_id gcc_msm8937_of_match[] = {
	{ .compatible = "qcom,gcc-msm8917", .data = (ulong)&msm8937_clk_data },
	{ .compatible = "qcom,gcc-msm8937", .data = (ulong)&msm8937_clk_data },
	{ .compatible = "qcom,gcc-msm8940", .data = (ulong)&msm8937_clk_data },
	{ }
};

U_BOOT_DRIVER(gcc_msm8937) = {
	.name		= "gcc_msm8937",
	.id		= UCLASS_NOP,
	.of_match	= gcc_msm8937_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC,
};
