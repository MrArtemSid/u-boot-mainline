// SPDX-License-Identifier: GPL-2.0+
/* Normalize the duplicate RGB888 reservation left by lk2nd's XRGB handoff. */
#include <linux/libfdt.h>
#include <string.h>

static uint64_t splash_reg_cell(const fdt32_t *p)
{
	return ((uint64_t)fdt32_to_cpu(p[0]) << 32) | fdt32_to_cpu(p[1]);
}

static uint32_t splash_u32(const void *fdt, int node, const char *name)
{
	const fdt32_t *p;
	int len;

	p = fdt_getprop(fdt, node, name, &len);
	return p && len == sizeof(*p) ? fdt32_to_cpu(*p) : 0;
}

int qcom_lk2nd_fixup(void *fdt)
{
	const fdt32_t *reg;
	u64 base, size, rgb_size;
	u32 width, height, stride, phandle;
	int fb = -1, parent, active, old, next, len, ret;
	const char *name;

	if (fdt_node_check_compatible(fdt, 0, "xiaomi,santoni"))
		return 0;
	parent = fdt_path_offset(fdt, "/reserved-memory");
	if (parent < 0 || fdt_address_cells(fdt, parent) != 2 ||
	    fdt_size_cells(fdt, parent) != 2)
		return 0;

	while ((fb = fdt_node_offset_by_compatible(fdt, fb,
						   "simple-framebuffer")) >= 0) {
		if (fdt_stringlist_search(fdt, fb, "status", "okay") != 0 &&
		    fdt_stringlist_search(fdt, fb, "status", "ok") != 0)
			continue;
		if (fdt_stringlist_search(fdt, fb, "format", "x8r8g8b8") != 0)
			continue;
		width = splash_u32(fdt, fb, "width");
		height = splash_u32(fdt, fb, "height");
		stride = splash_u32(fdt, fb, "stride");
		if (!width || !height || (uint64_t)width * 4 != stride)
			continue;
		phandle = splash_u32(fdt, fb, "memory-region");
		if (!phandle)
			continue;
		active = fdt_node_offset_by_phandle(fdt, phandle);
		if (active < 0 || fdt_parent_offset(fdt, active) != parent)
			continue;
		name = fdt_get_name(fdt, active, NULL);
		if (!name || strncmp(name, "cont-splash@", 12))
			continue;
		reg = fdt_getprop(fdt, active, "reg", &len);
		if (!reg || len != 4 * sizeof(*reg))
			continue;
		base = splash_reg_cell(reg);
		size = splash_reg_cell(reg + 2);
		if (size != (uint64_t)stride * height)
			continue;
		rgb_size = (uint64_t)width * height * 3;

		for (old = fdt_first_subnode(fdt, parent); old >= 0; old = next) {
			next = fdt_next_subnode(fdt, old);
			if (old == active || fdt_get_phandle(fdt, old) ||
			    fdt_getprop(fdt, old, "compatible", NULL) ||
			    !fdt_getprop(fdt, old, "no-map", NULL))
				continue;
			name = fdt_get_name(fdt, old, NULL);
			if (!name || strncmp(name, "memory@", 7))
				continue;
			reg = fdt_getprop(fdt, old, "reg", &len);
			if (!reg || len != 4 * sizeof(*reg) ||
			    splash_reg_cell(reg) != base ||
			    splash_reg_cell(reg + 2) != rgb_size)
				continue;
			/* Keep the live XRGB reservation. NOP preserves FDT offsets. */
			ret = fdt_nop_node(fdt, old);
			if (ret)
				return ret;
		}
	}
	return 0;
}
