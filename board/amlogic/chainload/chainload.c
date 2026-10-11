// SPDX-License-Identifier: GPL-2.0+
/*
 * S905X3 (SM1) boxes: mainline U-Boot chainloaded by the stock vendor U-Boot.
 */

#include <init.h>
#include <asm/arch/eth.h>

int misc_init_r(void)
{
	meson_generate_serial_ethaddr();

	return 0;
}
