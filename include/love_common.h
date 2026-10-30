#ifndef LOVE_COMMON_H
#define LOVE_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#ifdef __cplusplus
}
#endif

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 320
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 240
#endif

// Fast 4-instruction 2-pixel RGB565 average with rounding
static inline uint16_t avg2_rgb565(uint16_t p0, uint16_t p1) {
    return (uint16_t)((p0 | p1) - (((p0 ^ p1) & 0xF7DEU) >> 1));
}

// Fast SWAR 4-pixel RGB565 box filter average
static inline uint16_t avg4_rgb565(uint16_t p0, uint16_t p1, uint16_t p2, uint16_t p3) {
    uint32_t e0 = (p0 | ((uint32_t)p0 << 16)) & 0x07E0F81FU;
    uint32_t e1 = (p1 | ((uint32_t)p1 << 16)) & 0x07E0F81FU;
    uint32_t e2 = (p2 | ((uint32_t)p2 << 16)) & 0x07E0F81FU;
    uint32_t e3 = (p3 | ((uint32_t)p3 << 16)) & 0x07E0F81FU;
    uint32_t sum = ((e0 + e1 + e2 + e3 + 0x00401002U) >> 2) & 0x07E0F81FU;
    return (uint16_t)(sum | (sum >> 16));
}

// RGB565 pack / unpack
static inline uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static inline void rgb565_to_rgb(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
    *g = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
    *b = (uint8_t)((c & 0x1F) * 255 / 31);
}

// Fast 32-bit SWAR single-multiply RGB565 alpha blend
static inline uint16_t blend_rgb565(uint16_t dst, uint16_t src, uint8_t alpha) {
    uint32_t a = (alpha + 4) >> 3;
    if (a >= 32) return src;
    if (a == 0) return dst;
    uint32_t bg = (dst | ((uint32_t)dst << 16)) & 0x07E0F81FU;
    uint32_t fg = (src | ((uint32_t)src << 16)) & 0x07E0F81FU;
    uint32_t res = ((((fg - bg) * a) >> 5) + bg) & 0x07E0F81FU;
    return (uint16_t)(res | (res >> 16));
}

// RGBA8888 struct
typedef struct {
    uint8_t r, g, b, a;
} ColorRGBA;

static inline ColorRGBA make_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    ColorRGBA c = {r, g, b, a};
    return c;
}

#endif // LOVE_COMMON_H
