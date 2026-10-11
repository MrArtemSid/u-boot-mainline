// SPDX-License-Identifier: GPL-2.0+
/*
 * S905X3 (SM1) boxes: mainline U-Boot chainloaded by the stock vendor U-Boot.
 */

#include <fdt_support.h>
#include <init.h>
#include <linux/kernel.h>
#include <linux/libfdt.h>
#include <linux/sizes.h>
#include <asm/io.h>
#include <asm/arch/eth.h>
#include <asm/arch/g12a.h>

int misc_init_r(void)
{
	meson_generate_serial_ethaddr();

	return 0;
}

/* DDR size as BL2 reported it, without the U-Boot 0xf5000000 cap */
static u64 chainload_ddr_size(void)
{
	return (u64)((readl(G12A_AO_SEC_GP_CFG0) & G12A_AO_MEM_SIZE_MASK)
		     >> G12A_AO_MEM_SIZE_SHIFT) * SZ_1M;
}

/*
 * Vendor kernels take linux,usable-memory over the reg that bootm sets from
 * the DDR size, and their DTBs are per size (sm1_ac213_2g: 0x80000000,
 * sm1_ac213_4g: 0xf5700000). Clamp it to the DDR so a 4 GiB DTB boots on a
 * 2 GiB box; it never grows, so the 4 GiB value stays as it is.
 */
int ft_system_setup(void *blob, struct bd_info *bd)
{
	u64 ddr = chainload_ddr_size();
	int ac = fdt_address_cells(blob, 0);
	int sc = fdt_size_cells(blob, 0);
	int node, len, i, n;
	fdt32_t *p;

	if (!ddr || ac < 1 || ac > 2 || sc < 1 || sc > 2)
		return 0;

	for (node = fdt_node_offset_by_prop_value(blob, -1, "device_type",
						   "memory", sizeof("memory"));
	     node >= 0;
	     node = fdt_node_offset_by_prop_value(blob, node, "device_type",
						   "memory", sizeof("memory"))) {
		p = fdt_getprop_w(blob, node, "linux,usable-memory", &len);
		if (!p)
			continue;
		n = len / (int)sizeof(*p) / (ac + sc);
		for (i = 0; i < n; i++, p += ac + sc) {
			u64 start = fdt_read_number(p, ac);
			u64 size = fdt_read_number(p + ac, sc);
			u64 max = start < ddr ? ddr - start : 0;

			if (size <= max)
				continue;
			printf("chainload: linux,usable-memory 0x%llx@0x%llx -> 0x%llx (DDR %llu MiB)\n",
			       size, start, max, ddr / SZ_1M);
			if (sc == 2) {
				p[ac] = cpu_to_fdt32(upper_32_bits(max));
				p[ac + 1] = cpu_to_fdt32(lower_32_bits(max));
			} else {
				p[ac] = cpu_to_fdt32(max);
			}
		}
	}

	return 0;
}
