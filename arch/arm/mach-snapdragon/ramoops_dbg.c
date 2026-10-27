// SPDX-License-Identifier: GPL-2.0+
/*
 * Bring-up aid: mirror every console byte into the lk2nd ramoops window
 * (coreboot DBGC layout: u32 sig, u32 start, u32 size, data[]) so the log
 * survives a reset and can be read with `fastboot oem ramoops raw`.
 * Not intended for upstream.
 */

#include <cpu_func.h>
#include <linux/libfdt.h>
#include <stdio.h>
#include <string.h>
#include <linux/compiler.h>
#include <ramoops_dbg.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <video_font_8x16.h>
#include "early_text.h"

/* Must survive pre-relocation BSS and be copied with initialized data. */
static struct early_text text_fb __section(".data") = { .width = 720 };
static bool text_busy __section(".data");

#define DBGC_BASE	0xbffc0000UL
#define DBGC_SIG	0x43474244
#define DBGC_CAP	(0x40000 - 12 - 0x100)

struct dbgc {
	u32 sig;
	u32 start;
	u32 size;
	u8 data[];
};

static void dbgc_flush(void *p, size_t len)
{
	unsigned long s = (unsigned long)p & ~63UL;
	unsigned long e = ALIGN((unsigned long)p + len, 64);

	flush_dcache_range(s, e);
}

static void dbgc_raw(char c);

/* Prefix each line with CNTPCT in ms (19.2 MHz) since power-on */
void ramoops_dbg_putc(char c)
{
	static bool bol __section(".data") = true;

	if (bol) {
		unsigned long t;
		char buf[16];
		int i = 0;

		asm volatile("isb; mrs %0, cntpct_el0" : "=r"(t));
		t /= 19200;
		do {
			buf[i++] = '0' + t % 10;
			t /= 10;
		} while (t && i < 15);
		dbgc_raw('[');
		while (i)
			dbgc_raw(buf[--i]);
		dbgc_raw(']');
		dbgc_raw(' ');
	}
	bol = (c == '\n');
	dbgc_raw(c);
}

static void dbgc_raw(char c)
{
	struct dbgc *h = (struct dbgc *)DBGC_BASE;
	/* .data: BSS is not usable before relocation */
	static bool inited __section(".data");

	if (!inited) {
		h->sig = DBGC_SIG;
		h->start = 0;
		h->size = 0;
		inited = true;
	}
	if (h->start >= DBGC_CAP)
		h->start = 0;
	h->data[h->start] = c;
	dbgc_flush(&h->data[h->start], 1);
	h->start++;
	if (h->size < DBGC_CAP)
		h->size++;
	dbgc_flush(h, sizeof(*h));
	if (text_fb.pixels && !text_busy) {
		unsigned int oldrow = text_fb.row, oldcol = text_fb.col, y;

		text_busy = true;
		early_text_putc(&text_fb, (unsigned char)c, video_fontdata_8x16);
		if (dcache_status()) {
			for (y = oldrow * 32; y < oldrow * 32 + 32; y++)
				dbgc_flush(text_fb.pixels + y * text_fb.stride + oldcol * 16, 64);
			if (text_fb.row != oldrow)
				dbgc_flush(text_fb.pixels + text_fb.row * 32 * text_fb.stride,
					   32 * text_fb.stride * 4);
		}
		text_busy = false;
	}
}

void ramoops_dbg_puts(const char *s)
{
	while (*s)
		ramoops_dbg_putc(*s++);
}

/* UB-FB: inspect the actual LK handoff, not the source DT. */
void ramoops_dbg_fb_test(const void *fdt)
{
	int node = -1, len, parent, ac, sc;
	char msg[192];
	const char *fmt, *status;
	const fdt32_t *p;
	u32 w, h, stride;
	u64 base, size;
	unsigned int x, y;

	while ((node = fdt_node_offset_by_compatible(fdt, node,
						   "simple-framebuffer")) >= 0) {
		fmt = fdt_getprop(fdt, node, "format", NULL);
		status = fdt_getprop(fdt, node, "status", NULL);
		snprintf(msg, sizeof(msg), "[UB-FB] node=%s format=%s status=%s\n",
			 fdt_get_name(fdt, node, NULL), fmt ?: "missing", status ?: "okay");
		ramoops_dbg_puts(msg);
		if (!fmt || (status && strcmp(status, "okay") && strcmp(status, "ok")))
			continue;
		parent = fdt_parent_offset(fdt, node);
		ac = fdt_address_cells(fdt, parent);
		sc = fdt_size_cells(fdt, parent);
		p = fdt_getprop(fdt, node, "reg", &len);
		if (!p || ac != 2 || sc != 2 || len < 16)
			continue;
		base = ((u64)fdt32_to_cpu(p[0]) << 32) | fdt32_to_cpu(p[1]);
		size = ((u64)fdt32_to_cpu(p[2]) << 32) | fdt32_to_cpu(p[3]);
		p = fdt_getprop(fdt, node, "width", &len);
		if (!p || len != 4) continue;
		w = fdt32_to_cpu(*p);
		p = fdt_getprop(fdt, node, "height", &len);
		if (!p || len != 4) continue;
		h = fdt32_to_cpu(*p);
		p = fdt_getprop(fdt, node, "stride", &len);
		if (!p || len != 4) continue;
		stride = fdt32_to_cpu(*p);
		snprintf(msg, sizeof(msg), "[UB-FB] base=%llx size=%llx %ux%u stride=%u\n",
			 (unsigned long long)base, (unsigned long long)size, w, h, stride);
		ramoops_dbg_puts(msg);
		if (strcmp(fmt, "x8r8g8b8") || w != 720 || h != 1280 ||
		    stride != 2880 || base != 0x90001000 || size < (u64)stride * h)
			continue;
		for (y = 0; y < h; y++) {
			volatile u32 *row = (volatile u32 *)(unsigned long)(base + y * stride);
			for (x = 0; x < w; x++)
				row[x] = 0;
		}
		dbgc_flush((void *)(unsigned long)base, stride * h);
		text_fb.pixels = (unsigned int *)(unsigned long)base;
		text_fb.width = w;
		text_fb.height = h;
		text_fb.stride = stride / 4;
		text_fb.col = text_fb.row = text_fb.escape = 0;
		ramoops_dbg_puts("UB-TEXT: early XRGB8888 console 16x32\n");
	}
}
