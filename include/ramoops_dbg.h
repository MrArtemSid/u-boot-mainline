/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __RAMOOPS_DBG_H
#define __RAMOOPS_DBG_H

#if IS_ENABLED(CONFIG_QCOM_RAMOOPS_DBG)
void ramoops_dbg_putc(char c);
void ramoops_dbg_puts(const char *s);
void ramoops_dbg_fb_test(const void *fdt);
#else
static inline void ramoops_dbg_putc(char c) {}
static inline void ramoops_dbg_puts(const char *s) {}
static inline void ramoops_dbg_fb_test(const void *fdt) {}
#endif

#endif
