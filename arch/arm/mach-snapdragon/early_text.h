/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef QCOM_EARLY_TEXT_H
#define QCOM_EARLY_TEXT_H
struct early_text {
	unsigned int *pixels;
	unsigned int width, height, stride, col, row, escape;
};

/* Circular screen rows avoid a full uncached framebuffer memmove. */
static inline void early_text_newline(struct early_text *fb)
{
	unsigned int x, y;
	fb->col = 0;
	fb->row = (fb->row + 1) % (fb->height / 32);
	for (y = fb->row * 32; y < (fb->row + 1) * 32; y++)
		for (x = 0; x < fb->width; x++)
			fb->pixels[y * fb->stride + x] = 0;
}

static inline void early_text_putc(struct early_text *fb, unsigned char c,
				 const unsigned char *font)
{
	unsigned int x, y;
	if (!fb->pixels || fb->width < 16 || fb->height < 32 || fb->stride < fb->width)
		return;
	if (fb->escape) {
		if (fb->escape == 1)
			fb->escape = c == '[' ? 2 : 0;
		else if (c >= 0x40 && c <= 0x7e)
			fb->escape = 0;
		return;
	}
	if (c == 27) { fb->escape = 1; return; }
	if (c == '\r') { fb->col = 0; return; }
	if (c == '\n') { early_text_newline(fb); return; }
	if (c < 32 || c > 126)
		return;
	for (y = 0; y < 32; y++) {
		unsigned int bits = font[c * 16 + y / 2];
		for (x = 0; x < 16; x++)
			fb->pixels[(fb->row * 32 + y) * fb->stride + fb->col * 16 + x] =
				(bits & (0x80 >> (x / 2))) ? 0xffffff : 0;
	}
	if (++fb->col >= fb->width / 16)
		early_text_newline(fb);
}
#endif
