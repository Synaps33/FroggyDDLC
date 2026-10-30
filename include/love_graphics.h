#ifndef LOVE_GRAPHICS_H
#define LOVE_GRAPHICS_H

#include "love_common.h"

#define MAX_TRANSFORM_STACK 32
#define MAX_CANVAS_WIDTH 640
#define MAX_CANVAS_HEIGHT 480

// 2D Affine Transform Matrix (3x3 row-major: [a c tx; b d ty; 0 0 1])
typedef struct {
    float a, c, tx;
    float b, d, ty;
} Matrix2D;

typedef struct {
    float x, y, w, h;
    float sw, sh;
} LoveQuad;

typedef struct {
    int width;
    int height;
    uint16_t *pixels565; // RGB565 (2 bytes/pixel)
    uint8_t  *alpha;     // Alpha 0-255 (1 byte/pixel)
} LoveImage;

typedef struct {
    int width;
    int height;
    uint16_t *pixels; // RGB565
} LoveCanvas;

typedef struct {
    int width, height;
    int xoff, yoff;
    int xadvance;
    uint8_t *bitmap;
} Glyph;

typedef struct {
    unsigned char *ttf_buffer;
    float size;
    float scale;
    int ascent, descent, line_gap;
    Glyph glyphs[256]; // ASCII cache
    bool glyph_loaded[256];
} LoveFont;

typedef struct {
    char name[64];
} LoveShader;

class LoveGraphics {
public:
    static void init();
    static void deinit();

    static void setScreenBuffer(uint16_t *buffer, int width, int height);
    static uint16_t *getScreenBuffer() { return s_screenBuffer; }

    static void clear(float r, float g, float b, float a);
    static void present();

    static void setColor(float r, float g, float b, float a);
    static void getColor(float *r, float *g, float *b, float *a);
    static void setBackgroundColor(float r, float g, float b, float a);
    static void getBackgroundColor(float *r, float *g, float *b, float *a);
    static void clearBackground();

    static void push();
    static void pop();
    static void origin();
    static void translate(float dx, float dy);
    static void scale(float sx, float sy);
    static void rotate(float rad);

    static void setScissor(int x, int y, int w, int h);
    static void clearScissor();

    static void setCanvas(LoveCanvas *canvas);
    static LoveCanvas *getCanvas() { return s_currentCanvas; }

    static void setFont(LoveFont *font);
    static LoveFont *getFont() { return s_currentFont; }

    static void setLineWidth(float w) { s_lineWidth = w; }
    static float getLineWidth() { return s_lineWidth; }

    // Factory & utility methods
    static LoveImage *newImage(const uint8_t *data, size_t size);
    static LoveImage *newImageFromFile(const char *path);
    static void destroyImage(LoveImage *img);

    static LoveCanvas *newCanvas(int w, int h);
    static void destroyCanvas(LoveCanvas *c);

    static LoveFont *newFont(const uint8_t *data, size_t size, float font_size);
    static LoveFont *newFontFromFile(const char *path, float font_size);
    static void destroyFont(LoveFont *font);
    static float getFontWidth(LoveFont *font, const char *text);
    static float getFontHeight(LoveFont *font);

    // Drawing primitives
    static void draw(LoveImage *img, LoveQuad *quad, float x, float y, float r, float sx, float sy, float ox, float oy);
    static void drawCanvas(LoveCanvas *canv, float x, float y, float r, float sx, float sy, float ox, float oy);
    static void rectangle(const char *mode, float x, float y, float w, float h, float rx = 0, float ry = 0);
    static void polygon(const char *mode, const float *points, int count);
    static void circle(const char *mode, float x, float y, float radius);
    static void line(const float *points, int count);
    static void print(const char *text, float x, float y, float r, float sx, float sy, float ox, float oy);
    static void printf(const char *text, float x, float y, float wrap_limit, const char *align);

    static int getWidth() { return s_targetWidth; }
    static int getHeight() { return s_targetHeight; }

private:
    static uint16_t *s_screenBuffer;
    static int s_screenWidth;
    static int s_screenHeight;

    static uint16_t *s_targetBuffer;
    static int s_targetWidth;
    static int s_targetHeight;

    static LoveCanvas *s_currentCanvas;
    static LoveFont *s_currentFont;
    static LoveFont *s_defaultFont;

    static ColorRGBA s_currentColor;
    static ColorRGBA s_backgroundColor;
    static ColorRGBA s_colorStack[MAX_TRANSFORM_STACK];
    static float s_lineWidth;

    static Matrix2D s_matrixStack[MAX_TRANSFORM_STACK];
    static int s_matrixTop;

    static int s_scissorX, s_scissorY, s_scissorW, s_scissorH;
    static bool s_scissorActive;

    static void transformPoint(float x, float y, float *out_x, float *out_y);
    static void putPixelBlend(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    static void fillRectClipped(int x0, int y0, int x1, int y1, uint16_t c565, uint8_t a);
};

#endif // LOVE_GRAPHICS_H
