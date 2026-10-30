#include "love_graphics.h"
#include "love_filesystem.h"
#include <stdio.h>

#ifdef LOVE_PROFILE
uint32_t g_gfx_draw_calls = 0;
uint32_t g_gfx_touched_px = 0;
uint32_t g_gfx_fill_px = 0;
uint32_t g_gfx_glyph_px = 0;
uint32_t g_gfx_img_conv_px = 0;
uint32_t g_gfx_canvas_px = 0;
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

uint16_t *LoveGraphics::s_screenBuffer = NULL;
int LoveGraphics::s_screenWidth = SCREEN_WIDTH;
int LoveGraphics::s_screenHeight = SCREEN_HEIGHT;

uint16_t *LoveGraphics::s_targetBuffer = NULL;
int LoveGraphics::s_targetWidth = SCREEN_WIDTH;
int LoveGraphics::s_targetHeight = SCREEN_HEIGHT;

LoveCanvas *LoveGraphics::s_currentCanvas = NULL;
LoveFont *LoveGraphics::s_currentFont = NULL;
LoveFont *LoveGraphics::s_defaultFont = NULL;

ColorRGBA LoveGraphics::s_currentColor = {255, 255, 255, 255};
ColorRGBA LoveGraphics::s_backgroundColor = {0, 0, 0, 255};
ColorRGBA LoveGraphics::s_colorStack[MAX_TRANSFORM_STACK];
float LoveGraphics::s_lineWidth = 1.0f;

Matrix2D LoveGraphics::s_matrixStack[MAX_TRANSFORM_STACK];
int LoveGraphics::s_matrixTop = 0;

int LoveGraphics::s_scissorX = 0;
int LoveGraphics::s_scissorY = 0;
int LoveGraphics::s_scissorW = SCREEN_WIDTH;
int LoveGraphics::s_scissorH = SCREEN_HEIGHT;
bool LoveGraphics::s_scissorActive = false;

static inline void matrix_identity(Matrix2D *m) {
    m->a = 1.0f; m->c = 0.0f; m->tx = 0.0f;
    m->b = 0.0f; m->d = 1.0f; m->ty = 0.0f;
}

static inline void matrix_multiply(Matrix2D *out, const Matrix2D *m1, const Matrix2D *m2) {
    Matrix2D r;
    r.a  = m1->a * m2->a + m1->c * m2->b;
    r.c  = m1->a * m2->c + m1->c * m2->d;
    r.tx = m1->a * m2->tx + m1->c * m2->ty + m1->tx;

    r.b  = m1->b * m2->a + m1->d * m2->b;
    r.d  = m1->b * m2->c + m1->d * m2->d;
    r.ty = m1->b * m2->tx + m1->d * m2->ty + m1->ty;
    *out = r;
}

void LoveGraphics::init() {
    s_matrixTop = 0;
    matrix_identity(&s_matrixStack[0]);
    s_currentCanvas = NULL;
    s_targetBuffer = s_screenBuffer;
    s_targetWidth = s_screenWidth;
    s_targetHeight = s_screenHeight;
    s_currentColor = make_rgba(255, 255, 255, 255);
    s_scissorActive = false;
    s_lineWidth = 1.0f;
}

void LoveGraphics::deinit() {
}

void LoveGraphics::setScreenBuffer(uint16_t *buffer, int width, int height) {
    s_screenBuffer = buffer;
    s_screenWidth = width;
    s_screenHeight = height;
    if (!s_currentCanvas) {
        s_targetBuffer = s_screenBuffer;
        s_targetWidth = s_screenWidth;
        s_targetHeight = s_screenHeight;
    }
}

void LoveGraphics::clear(float r, float g, float b, float a) {
    if (!s_targetBuffer) return;
    if (r > 1.0f || g > 1.0f || b > 1.0f || a > 1.0f) {
        r /= 255.0f; g /= 255.0f; b /= 255.0f; a /= 255.0f;
    }
    uint8_t ur = (uint8_t)(fminf(fmaxf(r, 0.0f), 1.0f) * 255.0f);
    uint8_t ug = (uint8_t)(fminf(fmaxf(g, 0.0f), 1.0f) * 255.0f);
    uint8_t ub = (uint8_t)(fminf(fmaxf(b, 0.0f), 1.0f) * 255.0f);
    uint16_t c565 = rgb_to_rgb565(ur, ug, ub);

    uint32_t c32 = ((uint32_t)c565 << 16) | c565;
    uint32_t *dst32 = (uint32_t*)s_targetBuffer;
    int count32 = (s_targetWidth * s_targetHeight) / 2;
    for (int i = 0; i < count32; i++) {
        dst32[i] = c32;
    }
}

void LoveGraphics::present() {
    // If double-buffered, flip or sync here
}

void LoveGraphics::setColor(float r, float g, float b, float a) {
    if (r > 1.0f || g > 1.0f || b > 1.0f || a > 1.0f) {
        r /= 255.0f; g /= 255.0f; b /= 255.0f; a /= 255.0f;
    }
    s_currentColor.r = (uint8_t)(fminf(fmaxf(r, 0.0f), 1.0f) * 255.0f);
    s_currentColor.g = (uint8_t)(fminf(fmaxf(g, 0.0f), 1.0f) * 255.0f);
    s_currentColor.b = (uint8_t)(fminf(fmaxf(b, 0.0f), 1.0f) * 255.0f);
    s_currentColor.a = (uint8_t)(fminf(fmaxf(a, 0.0f), 1.0f) * 255.0f);
}

void LoveGraphics::getColor(float *r, float *g, float *b, float *a) {
    if (r) *r = s_currentColor.r / 255.0f;
    if (g) *g = s_currentColor.g / 255.0f;
    if (b) *b = s_currentColor.b / 255.0f;
    if (a) *a = s_currentColor.a / 255.0f;
}

void LoveGraphics::setBackgroundColor(float r, float g, float b, float a) {
    if (r > 1.0f || g > 1.0f || b > 1.0f || a > 1.0f) {
        r /= 255.0f; g /= 255.0f; b /= 255.0f; a /= 255.0f;
    }
    s_backgroundColor.r = (uint8_t)(fminf(fmaxf(r, 0.0f), 1.0f) * 255.0f);
    s_backgroundColor.g = (uint8_t)(fminf(fmaxf(g, 0.0f), 1.0f) * 255.0f);
    s_backgroundColor.b = (uint8_t)(fminf(fmaxf(b, 0.0f), 1.0f) * 255.0f);
    s_backgroundColor.a = (uint8_t)(fminf(fmaxf(a, 0.0f), 1.0f) * 255.0f);
}

void LoveGraphics::getBackgroundColor(float *r, float *g, float *b, float *a) {
    if (r) *r = s_backgroundColor.r / 255.0f;
    if (g) *g = s_backgroundColor.g / 255.0f;
    if (b) *b = s_backgroundColor.b / 255.0f;
    if (a) *a = s_backgroundColor.a / 255.0f;
}

void LoveGraphics::clearBackground() {
    if (!s_screenBuffer) return;
    uint16_t c = rgb_to_rgb565(s_backgroundColor.r, s_backgroundColor.g, s_backgroundColor.b);
    int count = s_screenWidth * s_screenHeight;
    for (int i = 0; i < count; i++) s_screenBuffer[i] = c;
}

void LoveGraphics::push() {
    if (s_matrixTop < MAX_TRANSFORM_STACK - 1) {
        s_matrixStack[s_matrixTop + 1] = s_matrixStack[s_matrixTop];
        s_colorStack[s_matrixTop] = s_currentColor;
        s_matrixTop++;
    }
}

void LoveGraphics::pop() {
    if (s_matrixTop > 0) {
        s_matrixTop--;
        s_currentColor = s_colorStack[s_matrixTop];
    }
}

void LoveGraphics::origin() {
    matrix_identity(&s_matrixStack[s_matrixTop]);
}

void LoveGraphics::translate(float dx, float dy) {
    Matrix2D m;
    matrix_identity(&m);
    m.tx = dx;
    m.ty = dy;
    matrix_multiply(&s_matrixStack[s_matrixTop], &s_matrixStack[s_matrixTop], &m);
}

void LoveGraphics::scale(float sx, float sy) {
    Matrix2D m;
    matrix_identity(&m);
    m.a = sx;
    m.d = sy;
    matrix_multiply(&s_matrixStack[s_matrixTop], &s_matrixStack[s_matrixTop], &m);
}

void LoveGraphics::rotate(float rad) {
    Matrix2D m;
    float c = cosf(rad);
    float s = sinf(rad);
    m.a = c;  m.c = -s; m.tx = 0;
    m.b = s;  m.d = c;  m.ty = 0;
    matrix_multiply(&s_matrixStack[s_matrixTop], &s_matrixStack[s_matrixTop], &m);
}

void LoveGraphics::setScissor(int x, int y, int w, int h) {
    s_scissorX = x;
    s_scissorY = y;
    s_scissorW = w;
    s_scissorH = h;
    s_scissorActive = true;
}

void LoveGraphics::clearScissor() {
    s_scissorActive = false;
}

void LoveGraphics::setCanvas(LoveCanvas *canvas) {
    s_currentCanvas = canvas;
    if (canvas) {
        s_targetBuffer = canvas->pixels;
        s_targetWidth = canvas->width;
        s_targetHeight = canvas->height;
    } else {
        s_targetBuffer = s_screenBuffer;
        s_targetWidth = s_screenWidth;
        s_targetHeight = s_screenHeight;
    }
}

void LoveGraphics::setFont(LoveFont *font) {
    s_currentFont = font;
}

void LoveGraphics::transformPoint(float x, float y, float *out_x, float *out_y) {
    const Matrix2D *m = &s_matrixStack[s_matrixTop];
    *out_x = m->a * x + m->c * y + m->tx;
    *out_y = m->b * x + m->d * y + m->ty;
}

inline void LoveGraphics::putPixelBlend(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || x >= s_targetWidth || y < 0 || y >= s_targetHeight) return;
    if (s_scissorActive) {
        if (x < s_scissorX || x >= s_scissorX + s_scissorW ||
            y < s_scissorY || y >= s_scissorY + s_scissorH) return;
    }

    uint16_t *dst = &s_targetBuffer[y * s_targetWidth + x];
    uint16_t src565 = rgb_to_rgb565(r, g, b);
    *dst = blend_rgb565(*dst, src565, a);
}

void LoveGraphics::fillRectClipped(int x0, int y0, int x1, int y1, uint16_t c565, uint8_t a) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }

    int min_x = 0, max_x = s_targetWidth - 1;
    int min_y = 0, max_y = s_targetHeight - 1;
    if (s_scissorActive) {
        if (s_scissorX > min_x) min_x = s_scissorX;
        if (s_scissorX + s_scissorW - 1 < max_x) max_x = s_scissorX + s_scissorW - 1;
        if (s_scissorY > min_y) min_y = s_scissorY;
        if (s_scissorY + s_scissorH - 1 < max_y) max_y = s_scissorY + s_scissorH - 1;
    }

    if (x0 < min_x) x0 = min_x;
    if (x1 > max_x) x1 = max_x;
    if (y0 < min_y) y0 = min_y;
    if (y1 > max_y) y1 = max_y;

#ifdef LOVE_PROFILE
    if (x1 >= x0 && y1 >= y0) g_gfx_fill_px += (uint32_t)(x1 - x0 + 1) * (uint32_t)(y1 - y0 + 1);
#endif

    if (x0 > x1 || y0 > y1) return;

    if (a >= 255) {
        uint32_t c32 = ((uint32_t)c565 << 16) | c565;
        int w = x1 - x0 + 1;
        for (int y = y0; y <= y1; y++) {
            uint16_t *row = &s_targetBuffer[y * s_targetWidth + x0];
            int x = 0;
            if (((uintptr_t)row & 2) && w > 0) {
                row[0] = c565;
                x = 1;
            }
            int w32 = (w - x) >> 1;
            uint32_t *row32 = (uint32_t*)&row[x];
            for (int i = 0; i < w32; i++) {
                row32[i] = c32;
            }
            if ((w - x) & 1) {
                row[w - 1] = c565;
            }
        }
    } else if (a > 0) {
        for (int y = y0; y <= y1; y++) {
            uint16_t *row = &s_targetBuffer[y * s_targetWidth + x0];
            int w = x1 - x0 + 1;
            for (int x = 0; x < w; x++) {
                row[x] = blend_rgb565(row[x], c565, a);
            }
        }
    }
}

void LoveGraphics::rectangle(const char *mode, float x, float y, float w, float h, float rx, float ry) {
    if (!s_targetBuffer) return;
    float p0x, p0y, p1x, p1y;
    transformPoint(x, y, &p0x, &p0y);
    transformPoint(x + w, y + h, &p1x, &p1y);

    uint16_t c565 = rgb_to_rgb565(s_currentColor.r, s_currentColor.g, s_currentColor.b);

    if (strcmp(mode, "fill") == 0) {
        fillRectClipped((int)floorf(p0x), (int)floorf(p0y), (int)floorf(p1x), (int)floorf(p1y), c565, s_currentColor.a);
    } else {
        // Outline mode
        int ix0 = (int)floorf(p0x), iy0 = (int)floorf(p0y);
        int ix1 = (int)floorf(p1x), iy1 = (int)floorf(p1y);
        fillRectClipped(ix0, iy0, ix1, iy0, c565, s_currentColor.a); // Top
        fillRectClipped(ix0, iy1, ix1, iy1, c565, s_currentColor.a); // Bottom
        fillRectClipped(ix0, iy0, ix0, iy1, c565, s_currentColor.a); // Left
        fillRectClipped(ix1, iy0, ix1, iy1, c565, s_currentColor.a); // Right
    }
}

void LoveGraphics::polygon(const char *mode, const float *points, int count) {
    if (!points || count < 6) return;
    int n = count / 2;
    if (n > 64) n = 64;

    float tx[64], ty[64];
    for (int i = 0; i < n; i++) {
        transformPoint(points[i * 2], points[i * 2 + 1], &tx[i], &ty[i]);
    }

    if (strcmp(mode, "fill") == 0) {
        float min_yf = ty[0], max_yf = ty[0];
        for (int i = 1; i < n; i++) {
            if (ty[i] < min_yf) min_yf = ty[i];
            if (ty[i] > max_yf) max_yf = ty[i];
        }

        int y_start = (int)ceilf(min_yf);
        int y_end   = (int)floorf(max_yf);

        int clip_y0 = 0, clip_y1 = s_targetHeight - 1;
        int clip_x0 = 0, clip_x1 = s_targetWidth - 1;
        if (s_scissorActive) {
            if (s_scissorX > clip_x0) clip_x0 = s_scissorX;
            if (s_scissorX + s_scissorW - 1 < clip_x1) clip_x1 = s_scissorX + s_scissorW - 1;
            if (s_scissorY > clip_y0) clip_y0 = s_scissorY;
            if (s_scissorY + s_scissorH - 1 < clip_y1) clip_y1 = s_scissorY + s_scissorH - 1;
        }

        if (y_start < clip_y0) y_start = clip_y0;
        if (y_end > clip_y1)   y_end   = clip_y1;

        uint16_t c565 = rgb_to_rgb565(s_currentColor.r, s_currentColor.g, s_currentColor.b);
        uint8_t a = s_currentColor.a;
        if (a == 0) return;

        for (int y = y_start; y <= y_end; y++) {
            float y_sample = (float)y + 0.5f;
            float min_x = 999999.0f, max_x = -999999.0f;
            int hits = 0;

            for (int i = 0; i < n; i++) {
                int next = (i + 1) % n;
                float y0 = ty[i], y1 = ty[next];
                float x0 = tx[i], x1 = tx[next];

                if ((y0 <= y_sample && y1 > y_sample) || (y1 <= y_sample && y0 > y_sample)) {
                    float t = (y_sample - y0) / (y1 - y0);
                    float x_interp = x0 + t * (x1 - x0);
                    if (x_interp < min_x) min_x = x_interp;
                    if (x_interp > max_x) max_x = x_interp;
                    hits++;
                }
            }

            if (hits >= 2) {
                int x0 = (int)ceilf(min_x);
                int x1 = (int)floorf(max_x);
                if (x0 < clip_x0) x0 = clip_x0;
                if (x1 > clip_x1) x1 = clip_x1;

                if (x0 <= x1) {
                    uint16_t *row = &s_targetBuffer[y * s_targetWidth + x0];
                    int span_w = x1 - x0 + 1;
                    if (a >= 255) {
                        for (int sx = 0; sx < span_w; sx++) row[sx] = c565;
                    } else {
                        for (int sx = 0; sx < span_w; sx++) row[sx] = blend_rgb565(row[sx], c565, a);
                    }
                }
            }
        }
    } else {
        for (int i = 0; i < n; i++) {
            int next = (i + 1) % n;
            int x0 = (int)tx[i], y0 = (int)ty[i];
            int x1 = (int)tx[next], y1 = (int)ty[next];

            int dx = abs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
            int dy = -abs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
            int err = dx + dy, e2;
            int cx = x0, cy = y0;
            while (1) {
                putPixelBlend(cx, cy, s_currentColor.r, s_currentColor.g, s_currentColor.b, s_currentColor.a);
                if (cx == x1 && cy == y1) break;
                e2 = 2 * err;
                if (e2 >= dy) { err += dy; cx += sx; }
                if (e2 <= dx) { err += dx; cy += sy; }
            }
        }
    }
}

void LoveGraphics::draw(LoveImage *img, LoveQuad *quad, float x, float y, float r, float sx, float sy, float ox, float oy) {
    if (!img || !img->pixels565 || !img->alpha || !s_targetBuffer) return;

    float qx = 0, qy = 0, qw = img->width, qh = img->height;
    if (quad) {
        qx = quad->x;
        qy = quad->y;
        qw = quad->w;
        qh = quad->h;
    }

    // Build local transformation: translate(x, y) * rotate(r) * scale(sx, sy) * translate(-ox, -oy)
    push();
    translate(x, y);
    if (r != 0.0f) rotate(r);
    if (sx != 1.0f || sy != 1.0f) scale(sx, sy);
    if (ox != 0.0f || oy != 0.0f) translate(-ox, -oy);

    const Matrix2D *M = &s_matrixStack[s_matrixTop];

    // Four corners in local space: (0,0), (qw, 0), (qw, qh), (0, qh)
    float p0x = M->tx, p0y = M->ty;
    float p1x = M->a * qw + M->tx, p1y = M->b * qw + M->ty;
    float p2x = M->a * qw + M->c * qh + M->tx, p2y = M->b * qw + M->d * qh + M->ty;
    float p3x = M->c * qh + M->tx, p3y = M->d * qh + M->ty;

    pop();

    // Check axis-aligned fast path (no rotation or negligible rotation)
    if (fabsf(M->b) < 0.05f && fabsf(M->c) < 0.05f) {
        float dst_x0 = fminf(p0x, p1x);
        float dst_x1 = fmaxf(p0x, p1x);
        float dst_y0 = fminf(p0y, p3y);
        float dst_y1 = fmaxf(p0y, p3y);

        float span_x = dst_x1 - dst_x0;
        float span_y = dst_y1 - dst_y0;
        if (span_x <= 0.0001f || span_y <= 0.0001f) return;

        int min_ix = (int)floorf(dst_x0);
        int max_ix = (int)ceilf(dst_x1);
        int min_iy = (int)floorf(dst_y0);
        int max_iy = (int)ceilf(dst_y1);

        int clip_x0 = 0, clip_x1 = s_targetWidth - 1;
        int clip_y0 = 0, clip_y1 = s_targetHeight - 1;
        if (s_scissorActive) {
            if (s_scissorX > clip_x0) clip_x0 = s_scissorX;
            if (s_scissorX + s_scissorW - 1 < clip_x1) clip_x1 = s_scissorX + s_scissorW - 1;
            if (s_scissorY > clip_y0) clip_y0 = s_scissorY;
            if (s_scissorY + s_scissorH - 1 < clip_y1) clip_y1 = s_scissorY + s_scissorH - 1;
        }

        if (min_ix < clip_x0) min_ix = clip_x0;
        if (max_ix > clip_x1) max_ix = clip_x1;
        if (min_iy < clip_y0) min_iy = clip_y0;
        if (max_iy > clip_y1) max_iy = clip_y1;

        if (min_ix > max_ix || min_iy > max_iy) return;

        int u_step = (int)((qw / span_x) * 65536.0f);
        int v_step = (int)((qh / span_y) * 65536.0f);
        int cur_u_start = (int)(((min_ix - dst_x0) / span_x * qw) * 65536.0f);

        int iqx = (int)qx;
        int iqy = (int)qy;
        int qx_min = iqx;
        int qx_max = (int)(qx + qw) - 1;
        if (qx_min < 0) qx_min = 0;
        if (qx_max >= img->width) qx_max = img->width - 1;

        int qy_min = iqy;
        int qy_max = (int)(qy + qh) - 1;
        if (qy_min < 0) qy_min = 0;
        if (qy_max >= img->height) qy_max = img->height - 1;

        int u_off0 = (u_step > 65536) ? (u_step >> 2) : 0;
        int u_off1 = (u_step > 65536) ? ((u_step * 3) >> 2) : 0;
        int v_off0 = (v_step > 65536) ? (v_step >> 2) : 0;
        int v_off1 = (v_step > 65536) ? ((v_step * 3) >> 2) : 0;

        uint32_t tint_r = s_currentColor.r;
        uint32_t tint_g = s_currentColor.g;
        uint32_t tint_b = s_currentColor.b;
        uint32_t tint_a = s_currentColor.a;
        bool no_tint = (tint_r >= 250 && tint_g >= 250 && tint_b >= 250 && tint_a >= 250);

        int cur_v = (int)(((min_iy - dst_y0) / span_y * qh) * 65536.0f);

#ifdef LOVE_PROFILE
        g_gfx_draw_calls++;
        g_gfx_touched_px += (uint32_t)(max_ix - min_ix + 1) * (uint32_t)(max_iy - min_iy + 1);
#endif

        for (int py = min_iy; py <= max_iy; py++, cur_v += v_step) {
            int sy0 = iqy + ((cur_v + v_off0) >> 16);
            int sy1 = iqy + ((cur_v + v_off1) >> 16);
            if (sy0 < qy_min) sy0 = qy_min; else if (sy0 > qy_max) sy0 = qy_max;
            if (sy1 < qy_min) sy1 = qy_min; else if (sy1 > qy_max) sy1 = qy_max;

            const uint16_t *row0_565 = &img->pixels565[sy0 * img->width];
            const uint8_t  *row0_a   = &img->alpha[sy0 * img->width];
            uint16_t *dst_row = &s_targetBuffer[py * s_targetWidth];

            int cur_u = cur_u_start;

            if (sy0 == sy1) {
                if (no_tint) {
                    for (int px = min_ix; px <= max_ix; px++, cur_u += u_step) {
                        int sx0 = iqx + ((cur_u + u_off0) >> 16);
                        int sx1 = iqx + ((cur_u + u_off1) >> 16);
                        if (sx0 < qx_min) sx0 = qx_min; else if (sx0 > qx_max) sx0 = qx_max;
                        if (sx1 < qx_min) sx1 = qx_min; else if (sx1 > qx_max) sx1 = qx_max;

                        if (sx0 == sx1) {
                            uint8_t a0 = row0_a[sx0];
                            if (a0 == 0) continue;
                            if (a0 >= 250) dst_row[px] = row0_565[sx0];
                            else dst_row[px] = blend_rgb565(dst_row[px], row0_565[sx0], a0);
                        } else {
                            uint8_t a00 = row0_a[sx0];
                            uint8_t a01 = row0_a[sx1];
                            uint32_t a_sum = (uint32_t)a00 + a01;
                            if (a_sum == 0) continue;
                            if (a_sum >= 500) {
                                dst_row[px] = avg2_rgb565(row0_565[sx0], row0_565[sx1]);
                            } else {
                                uint16_t bg = dst_row[px];
                                uint16_t p00 = (a00 >= 250) ? row0_565[sx0] : (a00 == 0 ? bg : blend_rgb565(bg, row0_565[sx0], a00));
                                uint16_t p01 = (a01 >= 250) ? row0_565[sx1] : (a01 == 0 ? bg : blend_rgb565(bg, row0_565[sx1], a01));
                                dst_row[px] = avg2_rgb565(p00, p01);
                            }
                        }
                    }
                } else if (tint_r >= 250 && tint_g >= 250 && tint_b >= 250) {
                    for (int px = min_ix; px <= max_ix; px++, cur_u += u_step) {
                        int sx0 = iqx + ((cur_u + u_off0) >> 16);
                        int sx1 = iqx + ((cur_u + u_off1) >> 16);
                        if (sx0 < qx_min) sx0 = qx_min; else if (sx0 > qx_max) sx0 = qx_max;
                        if (sx1 < qx_min) sx1 = qx_min; else if (sx1 > qx_max) sx1 = qx_max;

                        uint8_t a00 = row0_a[sx0];
                        uint8_t a01 = row0_a[sx1];
                        uint32_t a_sum = (uint32_t)a00 + a01;
                        if (a_sum == 0) continue;

                        uint16_t bg = dst_row[px];
                        uint16_t p00 = (a00 > 0) ? row0_565[sx0] : bg;
                        uint16_t p01 = (a01 > 0) ? row0_565[sx1] : bg;
                        uint16_t avg = (sx0 == sx1) ? p00 : avg2_rgb565(p00, p01);
                        uint8_t final_a = (uint8_t)(((a_sum >> 1) * tint_a) >> 8);
                        dst_row[px] = blend_rgb565(bg, avg, final_a);
                    }
                } else {
                    for (int px = min_ix; px <= max_ix; px++, cur_u += u_step) {
                        int sx0 = iqx + ((cur_u + u_off0) >> 16);
                        int sx1 = iqx + ((cur_u + u_off1) >> 16);
                        if (sx0 < qx_min) sx0 = qx_min; else if (sx0 > qx_max) sx0 = qx_max;
                        if (sx1 < qx_min) sx1 = qx_min; else if (sx1 > qx_max) sx1 = qx_max;

                        uint8_t a00 = row0_a[sx0];
                        uint8_t a01 = row0_a[sx1];
                        uint32_t a_sum = (uint32_t)a00 + a01;
                        if (a_sum == 0) continue;

                        uint16_t c0 = (a00 > 0) ? row0_565[sx0] : row0_565[sx1];
                        uint16_t c1 = (a01 > 0) ? row0_565[sx1] : c0;
                        uint16_t pix = (sx0 == sx1) ? c0 : avg2_rgb565(c0, c1);

                        uint32_t r = (((pix >> 11) & 0x1F) * tint_r) >> 8;
                        uint32_t g = (((pix >> 5)  & 0x3F) * tint_g) >> 8;
                        uint32_t b = (((pix)       & 0x1F) * tint_b) >> 8;
                        uint16_t s565 = (uint16_t)((r << 11) | (g << 5) | b);

                        uint32_t avg_a = a_sum >> 1;
                        uint8_t final_a = (tint_a < 255) ? (uint8_t)((avg_a * tint_a) >> 8) : (uint8_t)avg_a;
                        dst_row[px] = blend_rgb565(dst_row[px], s565, final_a);
                    }
                }
                continue;
            }

            const uint16_t *row1_565 = &img->pixels565[sy1 * img->width];
            const uint8_t  *row1_a   = &img->alpha[sy1 * img->width];

            if (no_tint) {
                for (int px = min_ix; px <= max_ix; px++, cur_u += u_step) {
                    int sx0 = iqx + ((cur_u + u_off0) >> 16);
                    int sx1 = iqx + ((cur_u + u_off1) >> 16);
                    if (sx0 < qx_min) sx0 = qx_min; else if (sx0 > qx_max) sx0 = qx_max;
                    if (sx1 < qx_min) sx1 = qx_min; else if (sx1 > qx_max) sx1 = qx_max;

                    if (sx0 == sx1) {
                        uint8_t a00 = row0_a[sx0];
                        uint8_t a10 = row1_a[sx0];
                        uint32_t a_sum = (uint32_t)a00 + a10;
                        if (a_sum == 0) continue;
                        if (a_sum >= 500) {
                            dst_row[px] = avg2_rgb565(row0_565[sx0], row1_565[sx0]);
                        } else {
                            uint16_t bg = dst_row[px];
                            uint16_t p00 = (a00 >= 250) ? row0_565[sx0] : (a00 == 0 ? bg : blend_rgb565(bg, row0_565[sx0], a00));
                            uint16_t p10 = (a10 >= 250) ? row1_565[sx0] : (a10 == 0 ? bg : blend_rgb565(bg, row1_565[sx0], a10));
                            dst_row[px] = avg2_rgb565(p00, p10);
                        }
                    } else {
                        uint8_t a00 = row0_a[sx0];
                        uint8_t a01 = row0_a[sx1];
                        uint8_t a10 = row1_a[sx0];
                        uint8_t a11 = row1_a[sx1];
                        uint32_t a_sum = (uint32_t)a00 + a01 + a10 + a11;
                        if (a_sum == 0) continue;

                        if (a_sum >= 1000) {
                            dst_row[px] = avg4_rgb565(row0_565[sx0], row0_565[sx1], row1_565[sx0], row1_565[sx1]);
                        } else {
                            uint16_t bg = dst_row[px];
                            uint16_t p00 = (a00 >= 250) ? row0_565[sx0] : (a00 == 0 ? bg : blend_rgb565(bg, row0_565[sx0], a00));
                            uint16_t p01 = (a01 >= 250) ? row0_565[sx1] : (a01 == 0 ? bg : blend_rgb565(bg, row0_565[sx1], a01));
                            uint16_t p10 = (a10 >= 250) ? row1_565[sx0] : (a10 == 0 ? bg : blend_rgb565(bg, row1_565[sx0], a10));
                            uint16_t p11 = (a11 >= 250) ? row1_565[sx1] : (a11 == 0 ? bg : blend_rgb565(bg, row1_565[sx1], a11));
                            dst_row[px] = avg4_rgb565(p00, p01, p10, p11);
                        }
                    }
                }
            } else if (tint_r >= 250 && tint_g >= 250 && tint_b >= 250) {
                for (int px = min_ix; px <= max_ix; px++, cur_u += u_step) {
                    int sx0 = iqx + ((cur_u + u_off0) >> 16);
                    int sx1 = iqx + ((cur_u + u_off1) >> 16);
                    if (sx0 < qx_min) sx0 = qx_min; else if (sx0 > qx_max) sx0 = qx_max;
                    if (sx1 < qx_min) sx1 = qx_min; else if (sx1 > qx_max) sx1 = qx_max;

                    uint8_t a00 = row0_a[sx0];
                    uint8_t a01 = row0_a[sx1];
                    uint8_t a10 = row1_a[sx0];
                    uint8_t a11 = row1_a[sx1];
                    uint32_t a_sum = (uint32_t)a00 + a01 + a10 + a11;
                    if (a_sum == 0) continue;

                    uint16_t bg = dst_row[px];
                    uint16_t p00 = (a00 > 0) ? row0_565[sx0] : bg;
                    uint16_t p01 = (a01 > 0) ? row0_565[sx1] : bg;
                    uint16_t p10 = (a10 > 0) ? row1_565[sx0] : bg;
                    uint16_t p11 = (a11 > 0) ? row1_565[sx1] : bg;
                    uint16_t avg = avg4_rgb565(p00, p01, p10, p11);
                    uint8_t final_a = (uint8_t)(((a_sum >> 2) * tint_a) >> 8);
                    dst_row[px] = blend_rgb565(bg, avg, final_a);
                }
            } else {
                for (int px = min_ix; px <= max_ix; px++, cur_u += u_step) {
                    int sx0 = iqx + ((cur_u + u_off0) >> 16);
                    int sx1 = iqx + ((cur_u + u_off1) >> 16);
                    if (sx0 < qx_min) sx0 = qx_min; else if (sx0 > qx_max) sx0 = qx_max;
                    if (sx1 < qx_min) sx1 = qx_min; else if (sx1 > qx_max) sx1 = qx_max;

                    uint8_t a00 = row0_a[sx0];
                    uint8_t a01 = row0_a[sx1];
                    uint8_t a10 = row1_a[sx0];
                    uint8_t a11 = row1_a[sx1];
                    uint32_t a_sum = (uint32_t)a00 + a01 + a10 + a11;
                    if (a_sum == 0) continue;

                    uint16_t c0 = (a00 > 0) ? row0_565[sx0] : ((a01 > 0) ? row0_565[sx1] : ((a10 > 0) ? row1_565[sx0] : row1_565[sx1]));
                    uint16_t c1 = (a01 > 0) ? row0_565[sx1] : c0;
                    uint16_t c2 = (a10 > 0) ? row1_565[sx0] : c0;
                    uint16_t c3 = (a11 > 0) ? row1_565[sx1] : c0;
                    uint16_t pix = avg4_rgb565(c0, c1, c2, c3);

                    uint32_t r = (((pix >> 11) & 0x1F) * tint_r) >> 8;
                    uint32_t g = (((pix >> 5)  & 0x3F) * tint_g) >> 8;
                    uint32_t b = (((pix)       & 0x1F) * tint_b) >> 8;
                    uint16_t s565 = (uint16_t)((r << 11) | (g << 5) | b);

                    uint32_t avg_a = a_sum >> 2;
                    uint8_t final_a = (tint_a < 255) ? (uint8_t)((avg_a * tint_a) >> 8) : (uint8_t)avg_a;
                    dst_row[px] = blend_rgb565(dst_row[px], s565, final_a);
                }
            }
        }
    } else {
        // Rotated quad rendering via inverse affine mapping
        float det = M->a * M->d - M->b * M->c;
        if (fabsf(det) < 1e-6f) return;
        float inv_det = 1.0f / det;
        float ia =  M->d * inv_det;
        float ib = -M->b * inv_det;
        float ic = -M->c * inv_det;
        float id =  M->a * inv_det;
        float itx = (M->c * M->ty - M->d * M->tx) * inv_det;
        float ity = (M->b * M->tx - M->a * M->ty) * inv_det;

        float min_x = fminf(fminf(p0x, p1x), fminf(p2x, p3x));
        float max_x = fmaxf(fmaxf(p0x, p1x), fmaxf(p2x, p3x));
        float min_y = fminf(fminf(p0y, p1y), fminf(p2y, p3y));
        float max_y = fmaxf(fmaxf(p0y, p1y), fmaxf(p2y, p3y));

        int min_ix = (int)floorf(min_x);
        int max_ix = (int)ceilf(max_x);
        int min_iy = (int)floorf(min_y);
        int max_iy = (int)ceilf(max_y);

#ifdef LOVE_PROFILE
        g_gfx_draw_calls++;
        g_gfx_touched_px += (uint32_t)(max_ix - min_ix + 1) * (uint32_t)(max_iy - min_iy + 1);
#endif

        int clip_x0 = 0, clip_x1 = s_targetWidth - 1;
        int clip_y0 = 0, clip_y1 = s_targetHeight - 1;
        if (s_scissorActive) {
            if (s_scissorX > clip_x0) clip_x0 = s_scissorX;
            if (s_scissorX + s_scissorW - 1 < clip_x1) clip_x1 = s_scissorX + s_scissorW - 1;
            if (s_scissorY > clip_y0) clip_y0 = s_scissorY;
            if (s_scissorY + s_scissorH - 1 < clip_y1) clip_y1 = s_scissorY + s_scissorH - 1;
        }

        if (min_ix < clip_x0) min_ix = clip_x0;
        if (max_ix > clip_x1) max_ix = clip_x1;
        if (min_iy < clip_y0) min_iy = clip_y0;
        if (max_iy > clip_y1) max_iy = clip_y1;

        if (min_ix > max_ix || min_iy > max_iy) return;

        uint32_t tint_r = s_currentColor.r;
        uint32_t tint_g = s_currentColor.g;
        uint32_t tint_b = s_currentColor.b;
        uint32_t tint_a = s_currentColor.a;
        bool no_tint = (tint_r >= 250 && tint_g >= 250 && tint_b >= 250 && tint_a >= 250);

        int ia_fx = (int)(ia * 65536.0f);
        int ib_fx = (int)(ib * 65536.0f);
        int ic_fx = (int)(ic * 65536.0f);
        int id_fx = (int)(id * 65536.0f);
        int start_lx = (int)((ia * min_ix + ic * min_iy + itx) * 65536.0f);
        int start_ly = (int)((ib * min_ix + id * min_iy + ity) * 65536.0f);
        int qw_fx = (int)(qw * 65536.0f);
        int qh_fx = (int)(qh * 65536.0f);
        int iqx = (int)qx;
        int iqy = (int)qy;

        int row_lx = start_lx;
        int row_ly = start_ly;

        for (int py = min_iy; py <= max_iy; py++) {
            uint16_t *dst_row = &s_targetBuffer[py * s_targetWidth];
            int cur_lx = row_lx;
            int cur_ly = row_ly;

            if (no_tint) {
                for (int px = min_ix; px <= max_ix; px++, cur_lx += ia_fx, cur_ly += ib_fx) {
                    if ((unsigned)cur_lx < (unsigned)qw_fx && (unsigned)cur_ly < (unsigned)qh_fx) {
                        int src_x = iqx + (cur_lx >> 16);
                        int src_y = iqy + (cur_ly >> 16);
                        if ((unsigned)src_x < (unsigned)img->width && (unsigned)src_y < (unsigned)img->height) {
                            int src_idx = src_y * img->width + src_x;
                            uint8_t a = img->alpha[src_idx];
                            if (a == 0) continue;
                            if (a >= 250) {
                                dst_row[px] = img->pixels565[src_idx];
                            } else {
                                dst_row[px] = blend_rgb565(dst_row[px], img->pixels565[src_idx], a);
                            }
                        }
                    }
                }
            } else if (tint_r >= 250 && tint_g >= 250 && tint_b >= 250) {
                for (int px = min_ix; px <= max_ix; px++, cur_lx += ia_fx, cur_ly += ib_fx) {
                    if ((unsigned)cur_lx < (unsigned)qw_fx && (unsigned)cur_ly < (unsigned)qh_fx) {
                        int src_x = iqx + (cur_lx >> 16);
                        int src_y = iqy + (cur_ly >> 16);
                        if ((unsigned)src_x < (unsigned)img->width && (unsigned)src_y < (unsigned)img->height) {
                            int src_idx = src_y * img->width + src_x;
                            uint8_t a = img->alpha[src_idx];
                            if (a == 0) continue;
                            uint8_t final_a = (uint8_t)((a * tint_a) >> 8);
                            dst_row[px] = blend_rgb565(dst_row[px], img->pixels565[src_idx], final_a);
                        }
                    }
                }
            } else {
                for (int px = min_ix; px <= max_ix; px++, cur_lx += ia_fx, cur_ly += ib_fx) {
                    if ((unsigned)cur_lx < (unsigned)qw_fx && (unsigned)cur_ly < (unsigned)qh_fx) {
                        int src_x = iqx + (cur_lx >> 16);
                        int src_y = iqy + (cur_ly >> 16);
                        if ((unsigned)src_x < (unsigned)img->width && (unsigned)src_y < (unsigned)img->height) {
                            int src_idx = src_y * img->width + src_x;
                            uint8_t a = img->alpha[src_idx];
                            if (a == 0) continue;

                            uint16_t pix = img->pixels565[src_idx];
                            uint32_t r = (((pix >> 11) & 0x1F) * tint_r) >> 8;
                            uint32_t g = (((pix >> 5)  & 0x3F) * tint_g) >> 8;
                            uint32_t b = (((pix)       & 0x1F) * tint_b) >> 8;
                            uint16_t s565 = (uint16_t)((r << 11) | (g << 5) | b);

                            uint8_t final_a = (tint_a < 255) ? (uint8_t)((a * tint_a) >> 8) : a;
                            dst_row[px] = blend_rgb565(dst_row[px], s565, final_a);
                        }
                    }
                }
            }
            row_lx += ic_fx;
            row_ly += id_fx;
        }
    }
}

void LoveGraphics::drawCanvas(LoveCanvas *canv, float x, float y, float r, float sx, float sy, float ox, float oy) {
    if (!canv || !canv->pixels || !s_targetBuffer) return;

    // Fast path: if 1:1 blit to target buffer
    if (r == 0.0f && sx == 1.0f && sy == 1.0f && ox == 0.0f && oy == 0.0f && x == 0.0f && y == 0.0f) {
        int w = (canv->width < s_targetWidth) ? canv->width : s_targetWidth;
        int h = (canv->height < s_targetHeight) ? canv->height : s_targetHeight;
        for (int row = 0; row < h; row++) {
            memcpy(&s_targetBuffer[row * s_targetWidth], &canv->pixels[row * canv->width], w * sizeof(uint16_t));
        }
#ifdef LOVE_PROFILE
        g_gfx_canvas_px += (uint32_t)w * (uint32_t)h;
#endif
        return;
    }

    // General scaled blit (fixed-point stepping, no per-pixel division)
    int dst_w = (int)(canv->width * sx);
    int dst_h = (int)(canv->height * sy);
    int start_x = (int)x;
    int start_y = (int)y;
    if (dst_w <= 0 || dst_h <= 0) return;

    int u_step = (int)(((float)canv->width / (float)dst_w) * 65536.0f);
    int v_step = (int)(((float)canv->height / (float)dst_h) * 65536.0f);
    int cur_v = 0;

    for (int dy = 0; dy < dst_h; dy++, cur_v += v_step) {
        int py = start_y + dy;
        if (py < 0 || py >= s_targetHeight) continue;
        int sy_coord = (cur_v >> 16);
        if (sy_coord >= canv->height) sy_coord = canv->height - 1;
        const uint16_t *src_row = &canv->pixels[sy_coord * canv->width];
        uint16_t *dst_row = &s_targetBuffer[py * s_targetWidth];

        int cur_u = 0;
        for (int dx = 0; dx < dst_w; dx++, cur_u += u_step) {
            int px = start_x + dx;
            if (px < 0 || px >= s_targetWidth) continue;
            int sx_coord = (cur_u >> 16);
            if (sx_coord >= canv->width) sx_coord = canv->width - 1;
            dst_row[px] = src_row[sx_coord];
        }
    }
#ifdef LOVE_PROFILE
    g_gfx_canvas_px += (uint32_t)dst_w * (uint32_t)dst_h;
#endif
}

static Glyph* get_glyph(LoveFont *font, unsigned char c) {
    if (!font || !font->ttf_buffer) return NULL;
    if (font->glyph_loaded[c]) return &font->glyphs[c];

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, font->ttf_buffer, 0)) return NULL;

    Glyph *g = &font->glyphs[c];
    int g_idx = stbtt_FindGlyphIndex(&info, c);
    if (!g_idx) {
        font->glyph_loaded[c] = true;
        g->xadvance = (int)(font->size * 0.5f);
        return g;
    }

    int adv, lsb;
    stbtt_GetGlyphHMetrics(&info, g_idx, &adv, &lsb);
    g->xadvance = (int)(adv * font->scale);

    int w, h, xoff, yoff;
    unsigned char *bmp = stbtt_GetGlyphBitmap(&info, font->scale, font->scale, g_idx, &w, &h, &xoff, &yoff);
    g->width = w;
    g->height = h;
    g->xoff = xoff;
    g->yoff = yoff;
    g->bitmap = bmp;
    font->glyph_loaded[c] = true;
    return g;
}

void LoveGraphics::print(const char *text, float x, float y, float r, float sx, float sy, float ox, float oy) {
    if (!text || !s_currentFont || !s_targetBuffer) return;

    push();
    translate(x, y);
    if (r != 0.0f) rotate(r);
    if (sx != 1.0f || sy != 1.0f) scale(sx, sy);
    if (ox != 0.0f || oy != 0.0f) translate(-ox, -oy);

    float cur_x = 0;
    float cur_y = (float)s_currentFont->ascent;

    const Matrix2D *M = &s_matrixStack[s_matrixTop];
    bool is_axis_aligned = (fabsf(M->b) < 0.15f && fabsf(M->c) < 0.15f);

    uint16_t text_color565 = rgb_to_rgb565(s_currentColor.r, s_currentColor.g, s_currentColor.b);
    uint32_t tint_a = s_currentColor.a;

    int clip_x0 = 0, clip_x1 = s_targetWidth - 1;
    int clip_y0 = 0, clip_y1 = s_targetHeight - 1;
    if (s_scissorActive) {
        if (s_scissorX > clip_x0) clip_x0 = s_scissorX;
        if (s_scissorX + s_scissorW - 1 < clip_x1) clip_x1 = s_scissorX + s_scissorW - 1;
        if (s_scissorY > clip_y0) clip_y0 = s_scissorY;
        if (s_scissorY + s_scissorH - 1 < clip_y1) clip_y1 = s_scissorY + s_scissorH - 1;
    }

    const char *p = text;
    while (*p) {
        if (*p == '\n') {
            cur_x = 0;
            cur_y += s_currentFont->size * 1.1f;
            p++;
            continue;
        }

        unsigned char c = (unsigned char)*p;
        Glyph *g = get_glyph(s_currentFont, c);
        if (g && g->bitmap && g->width > 0 && g->height > 0) {
            if (is_axis_aligned) {
                float glyph_x = cur_x + g->xoff;
                float dst_x = M->tx + glyph_x * M->a;
                float baseline_y = floorf(M->ty + cur_y * M->d + 0.5f);
                float dst_y = baseline_y + (float)g->yoff * M->d;
                float dst_w = (float)g->width * M->a;
                float dst_h = (float)g->height * M->d;

                if (dst_w > 0.01f && dst_h > 0.01f) {
                    int min_ix = (int)floorf(dst_x);
                    int max_ix = (int)ceilf(dst_x + dst_w);
                    int min_iy = (int)floorf(dst_y);
                    int max_iy = (int)ceilf(dst_y + dst_h);

                    if (min_ix < clip_x0) min_ix = clip_x0;
                    if (max_ix > clip_x1) max_ix = clip_x1;
                    if (min_iy < clip_y0) min_iy = clip_y0;
                    if (max_iy > clip_y1) max_iy = clip_y1;

                    if (min_ix <= max_ix && min_iy <= max_iy) {
                        static const uint8_t zero_row[1024] = {0};
                        int u_step = (int)((g->width / dst_w) * 65536.0f);
                        int cur_u_start = (int)(((min_ix - dst_x) / dst_w * g->width) * 65536.0f);
                        int v_step = (int)((g->height / dst_h) * 65536.0f);
                        int cur_v = (int)(((min_iy - dst_y) / dst_h * g->height) * 65536.0f);

                        int u_off0 = (u_step > 65536) ? (u_step >> 2) : 0;
                        int u_off1 = (u_step > 65536) ? ((u_step * 3) >> 2) : 0;
                        int v_off0 = (v_step > 65536) ? (v_step >> 2) : 0;
                        int v_off1 = (v_step > 65536) ? ((v_step * 3) >> 2) : 0;

                        uint32_t gw = (uint32_t)g->width;
                        uint32_t gh = (uint32_t)g->height;

                        for (int py = min_iy; py <= max_iy; py++, cur_v += v_step) {
                            uint32_t gy0 = (uint32_t)((cur_v + v_off0) >> 16);
                            uint32_t gy1 = (uint32_t)((cur_v + v_off1) >> 16);

                            const uint8_t *r0 = (gy0 < gh) ? &g->bitmap[gy0 * gw] : zero_row;
                            const uint8_t *r1 = (gy1 < gh) ? &g->bitmap[gy1 * gw] : zero_row;
                            uint16_t *dst_row = &s_targetBuffer[py * s_targetWidth];
                            int cur_u = cur_u_start;

                            if (r0 == r1) {
                                if (r0 == zero_row) continue;
                                for (int px = min_ix; px <= max_ix; px++, cur_u += u_step) {
                                    uint32_t gx0 = (uint32_t)((cur_u + u_off0) >> 16);
                                    uint32_t gx1 = (uint32_t)((cur_u + u_off1) >> 16);
                                    uint32_t sum = 0;
                                    if (gx0 < gw) sum += r0[gx0];
                                    if (gx1 < gw) sum += r0[gx1];
                                    uint32_t val = sum >> 1;
                                    if (val > 12) {
                                        uint32_t boosted_a = ((val - 12) * 310) >> 8;
                                        if (boosted_a > 255) boosted_a = 255;
                                        uint8_t final_a = (uint8_t)((boosted_a * tint_a) >> 8);
                                        if (final_a >= 252) dst_row[px] = text_color565;
                                        else dst_row[px] = blend_rgb565(dst_row[px], text_color565, final_a);
                                    }
                                }
                            } else {
                                for (int px = min_ix; px <= max_ix; px++, cur_u += u_step) {
                                    uint32_t gx0 = (uint32_t)((cur_u + u_off0) >> 16);
                                    uint32_t gx1 = (uint32_t)((cur_u + u_off1) >> 16);

                                    uint32_t sum = 0;
                                    if (gx0 < gw) sum += (uint32_t)r0[gx0] + r1[gx0];
                                    if (gx1 < gw) sum += (uint32_t)r0[gx1] + r1[gx1];
                                    uint32_t val = sum >> 2;

                                    if (val > 12) {
                                        uint32_t boosted_a = ((val - 12) * 310) >> 8;
                                        if (boosted_a > 255) boosted_a = 255;
                                        uint8_t final_a = (uint8_t)((boosted_a * tint_a) >> 8);
                                        if (final_a >= 252) dst_row[px] = text_color565;
                                        else dst_row[px] = blend_rgb565(dst_row[px], text_color565, final_a);
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                float sc_approx = fabsf(M->a) + fabsf(M->b);
                int step = (sc_approx > 0.001f) ? (int)(1.0f / sc_approx) : 1;
                if (step < 1) step = 1;
                if (step > 8) step = 8;
                for (int gy = 0; gy < g->height; gy += step) {
                    for (int gx = 0; gx < g->width; gx += step) {
                        uint8_t a = g->bitmap[gy * g->width + gx];
                        if (a > 15) {
                            float px, py;
                            transformPoint(cur_x + g->xoff + gx, cur_y + g->yoff + gy, &px, &py);
                            int ipx = (int)px;
                            int ipy = (int)py;
                            if (ipx >= clip_x0 && ipx <= clip_x1 && ipy >= clip_y0 && ipy <= clip_y1) {
                                uint8_t final_a = (uint8_t)((a * tint_a) >> 8);
                                uint16_t *dst = &s_targetBuffer[ipy * s_targetWidth + ipx];
                                *dst = blend_rgb565(*dst, text_color565, final_a);
                            }
                        }
                    }
                }
            }
        }
        if (g) cur_x += g->xadvance;
        p++;
    }

    pop();
}

void LoveGraphics::printf(const char *text, float x, float y, float wrap_limit, const char *align) {
    // Basic word-wrapping printf
    print(text, x, y, 0, 1, 1, 0, 0);
}

LoveImage *LoveGraphics::newImage(const uint8_t *data, size_t size) {
    if (!data || size == 0) return NULL;
    int w, h, channels;
    stbi_uc *pixels = stbi_load_from_memory(data, (int)size, &w, &h, &channels, 4);
    if (!pixels) return NULL;
    LoveImage *img = new LoveImage();
    img->width = w;
    img->height = h;
    int count = w * h;
    img->pixels565 = (uint16_t*)malloc(count * sizeof(uint16_t));
    img->alpha = (uint8_t*)malloc(count);
    if (!img->pixels565 || !img->alpha) {
        if (img->pixels565) free(img->pixels565);
        if (img->alpha) free(img->alpha);
        stbi_image_free(pixels);
        delete img;
        return NULL;
    }

    const uint8_t *src = pixels;
    for (int i = 0; i < count; i++, src += 4) {
        img->pixels565[i] = rgb_to_rgb565(src[0], src[1], src[2]);
        img->alpha[i] = src[3];
    }
    stbi_image_free(pixels); // Immediately free temporary 32-bit RGBA decompressed buffer
    return img;
}

LoveImage *LoveGraphics::newImageFromFile(const char *path) {
    void *data = NULL;
    size_t sz = 0;
    if (!LoveFilesystem::read(path, &data, &sz)) return NULL;
    LoveImage *img = newImage((const uint8_t*)data, sz);
    LoveFilesystem::freeData(data);
    return img;
}

void LoveGraphics::destroyImage(LoveImage *img) {
    if (!img) return;
    if (img->pixels565) free(img->pixels565);
    if (img->alpha) free(img->alpha);
    delete img;
}

LoveCanvas *LoveGraphics::newCanvas(int w, int h) {
    if (w <= 0 || h <= 0) return NULL;
    LoveCanvas *c = new LoveCanvas();
    c->width = w;
    c->height = h;
    c->pixels = (uint16_t*)calloc(w * h, sizeof(uint16_t));
    return c;
}

void LoveGraphics::destroyCanvas(LoveCanvas *c) {
    if (!c) return;
    if (c == s_currentCanvas) setCanvas(NULL);
    if (c->pixels) free(c->pixels);
    delete c;
}

LoveFont *LoveGraphics::newFont(const uint8_t *data, size_t size, float font_size) {
    if (!data || size == 0) return NULL;
    LoveFont *f = new LoveFont();
    memset(f, 0, sizeof(LoveFont));
    f->ttf_buffer = (unsigned char*)malloc(size);
    memcpy(f->ttf_buffer, data, size);
    f->size = font_size;

    stbtt_fontinfo info;
    if (stbtt_InitFont(&info, f->ttf_buffer, 0)) {
        f->scale = stbtt_ScaleForPixelHeight(&info, font_size);
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        f->ascent = (int)(ascent * f->scale);
        f->descent = (int)(descent * f->scale);
        f->line_gap = (int)(lineGap * f->scale);
    } else {
        f->scale = 1.0f;
    }
    return f;
}

LoveFont *LoveGraphics::newFontFromFile(const char *path, float font_size) {
    void *data = NULL;
    size_t sz = 0;
    if (!LoveFilesystem::read(path, &data, &sz)) return NULL;
    LoveFont *f = newFont((const uint8_t*)data, sz, font_size);
    LoveFilesystem::freeData(data);
    return f;
}

void LoveGraphics::destroyFont(LoveFont *font) {
    if (!font) return;
    if (font == s_currentFont) s_currentFont = NULL;
    if (font == s_defaultFont) s_defaultFont = NULL;
    for (int i = 0; i < 256; i++) {
        if (font->glyph_loaded[i] && font->glyphs[i].bitmap) {
            stbtt_FreeBitmap(font->glyphs[i].bitmap, NULL);
        }
    }
    if (font->ttf_buffer) free(font->ttf_buffer);
    delete font;
}

float LoveGraphics::getFontWidth(LoveFont *font, const char *text) {
    if (!font || !text) return 0;
    float max_w = 0;
    float cur_w = 0;
    const char *p = text;
    while (*p) {
        if (*p == '\n') {
            if (cur_w > max_w) max_w = cur_w;
            cur_w = 0;
            p++;
            continue;
        }
        Glyph *g = get_glyph(font, (unsigned char)*p);
        if (g) cur_w += g->xadvance;
        p++;
    }
    if (cur_w > max_w) max_w = cur_w;
    return max_w;
}

float LoveGraphics::getFontHeight(LoveFont *font) {
    if (!font) return 12.0f;
    return font->size;
}

void LoveGraphics::circle(const char *mode, float cx, float cy, float radius) {
    if (radius <= 0) return;
    int r = (int)radius;
    int r2 = r * r;
    bool fill = (strcmp(mode, "fill") == 0);
    uint16_t c565 = rgb_to_rgb565(s_currentColor.r, s_currentColor.g, s_currentColor.b);

    if (fill) {
        for (int dy = -r; dy <= r; dy++) {
            int dx_bound = (int)sqrtf((float)(r2 - dy * dy));
            fillRectClipped((int)(cx - dx_bound), (int)(cy + dy), (int)(cx + dx_bound + 1), (int)(cy + dy + 1), c565, s_currentColor.a);
        }
    } else {
        for (int dy = -r; dy <= r; dy++) {
            int dx = (int)sqrtf((float)(r2 - dy * dy));
            putPixelBlend((int)(cx + dx), (int)(cy + dy), s_currentColor.r, s_currentColor.g, s_currentColor.b, s_currentColor.a);
            putPixelBlend((int)(cx - dx), (int)(cy + dy), s_currentColor.r, s_currentColor.g, s_currentColor.b, s_currentColor.a);
        }
    }
}

void LoveGraphics::line(const float *points, int count) {
    if (!points || count < 4) return;
    for (int i = 0; i < count - 3; i += 2) {
        float fx0, fy0, fx1, fy1;
        transformPoint(points[i], points[i+1], &fx0, &fy0);
        transformPoint(points[i+2], points[i+3], &fx1, &fy1);
        int x0 = (int)fx0;
        int y0 = (int)fy0;
        int x1 = (int)fx1;
        int y1 = (int)fy1;

        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;
        while (1) {
            putPixelBlend(x0, y0, s_currentColor.r, s_currentColor.g, s_currentColor.b, s_currentColor.a);
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}
