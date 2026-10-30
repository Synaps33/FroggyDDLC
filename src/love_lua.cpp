#include "love_lua.h"
#include "love_graphics.h"
#include "love_filesystem.h"
#include "love_audio.h"
#include "love_timer.h"
#include "love_window.h"
#include "love_input.h"
#include "love_thread.h"
#include "miniz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern "C" {
int luaopen_bit(lua_State *L);
}

#if defined(SF2000) || defined(GB300) || defined(GB300V2)
extern "C" void xlog(const char *fmt, ...);
#define LOVE_LOG(...) do { printf(__VA_ARGS__); xlog(__VA_ARGS__); } while(0)
#else
#define LOVE_LOG(...) printf(__VA_ARGS__)
#endif

static void *luaL_testudata(lua_State *L, int ud, const char *tname) {
    void *p = lua_touserdata(L, ud);
    if (p != NULL) {
        if (lua_getmetatable(L, ud)) {
            lua_getfield(L, LUA_REGISTRYINDEX, tname);
            if (lua_rawequal(L, -1, -2)) {
                lua_pop(L, 2);
                return p;
            }
            lua_pop(L, 2);
        }
    }
    return NULL;
}

lua_State *LoveLua::s_L = NULL;
int LoveLua::s_runFuncRef = LUA_NOREF;

#define LUA_MAGIC_IMAGE  0x4C494D47U
#define LUA_MAGIC_QUAD   0x4C515544U
#define LUA_MAGIC_CANVAS 0x4C434E56U
#define LUA_MAGIC_FONT   0x4C464E54U
#define LUA_MAGIC_TEXT   0x4C545854U

// Forward declarations of userdata types
struct LuaImage {
    uint32_t magic;
    LoveImage *img;
};

struct LuaQuad {
    uint32_t magic;
    LoveQuad quad;
};

struct LuaCanvas {
    uint32_t magic;
    LoveCanvas *canvas;
};

struct LuaFont {
    uint32_t magic;
    LoveFont *font;
};

struct LuaText {
    uint32_t magic;
    LoveFont *font;
    char *str;
    float wrap_limit;
};

struct LuaShader {
    char name[64];
};

struct LuaSource {
    LoveSource *source;
};

// ============================================================================
// love.graphics bindings
// ============================================================================

static int l_img_getWidth(lua_State *L) {
    LuaImage *u = (LuaImage*)luaL_checkudata(L, 1, "LoveImage");
    lua_pushnumber(L, u->img ? u->img->width : 0);
    return 1;
}

static int l_img_getHeight(lua_State *L) {
    LuaImage *u = (LuaImage*)luaL_checkudata(L, 1, "LoveImage");
    lua_pushnumber(L, u->img ? u->img->height : 0);
    return 1;
}

static int l_img_getDimensions(lua_State *L) {
    LuaImage *u = (LuaImage*)luaL_checkudata(L, 1, "LoveImage");
    lua_pushnumber(L, u->img ? u->img->width : 0);
    lua_pushnumber(L, u->img ? u->img->height : 0);
    return 2;
}

static int l_img_setFilter(lua_State *L) { return 0; }
static int l_img_getFilter(lua_State *L) {
    lua_pushstring(L, "linear");
    lua_pushstring(L, "linear");
    return 2;
}
static int l_img_setWrap(lua_State *L) { return 0; }
static int l_img_getWrap(lua_State *L) {
    lua_pushstring(L, "clamp");
    lua_pushstring(L, "clamp");
    return 2;
}
static int l_img_release(lua_State *L) {
    LuaImage *u = (LuaImage*)luaL_checkudata(L, 1, "LoveImage");
    if (u->img) {
        LoveGraphics::destroyImage(u->img);
        u->img = NULL;
    }
    return 0;
}

static int l_img_gc(lua_State *L) {
    return l_img_release(L);
}

static int l_quad_getViewport(lua_State *L) {
    LuaQuad *q = (LuaQuad*)luaL_checkudata(L, 1, "LoveQuad");
    lua_pushnumber(L, q->quad.x);
    lua_pushnumber(L, q->quad.y);
    lua_pushnumber(L, q->quad.w);
    lua_pushnumber(L, q->quad.h);
    return 4;
}

static int l_quad_setViewport(lua_State *L) {
    LuaQuad *q = (LuaQuad*)luaL_checkudata(L, 1, "LoveQuad");
    q->quad.x = (float)luaL_checknumber(L, 2);
    q->quad.y = (float)luaL_checknumber(L, 3);
    q->quad.w = (float)luaL_checknumber(L, 4);
    q->quad.h = (float)luaL_checknumber(L, 5);
    if (!lua_isnoneornil(L, 6)) q->quad.sw = (float)luaL_checknumber(L, 6);
    if (!lua_isnoneornil(L, 7)) q->quad.sh = (float)luaL_checknumber(L, 7);
    return 0;
}

static int l_quad_getTextureDimensions(lua_State *L) {
    LuaQuad *q = (LuaQuad*)luaL_checkudata(L, 1, "LoveQuad");
    lua_pushnumber(L, q->quad.sw);
    lua_pushnumber(L, q->quad.sh);
    return 2;
}

static int l_quad_release(lua_State *L) { return 0; }

static int l_canv_getWidth(lua_State *L) {
    LuaCanvas *c = (LuaCanvas*)luaL_checkudata(L, 1, "LoveCanvas");
    lua_pushnumber(L, c->canvas ? c->canvas->width : 0);
    return 1;
}

static int l_canv_getHeight(lua_State *L) {
    LuaCanvas *c = (LuaCanvas*)luaL_checkudata(L, 1, "LoveCanvas");
    lua_pushnumber(L, c->canvas ? c->canvas->height : 0);
    return 1;
}

static int l_canv_getDimensions(lua_State *L) {
    LuaCanvas *c = (LuaCanvas*)luaL_checkudata(L, 1, "LoveCanvas");
    lua_pushnumber(L, c->canvas ? c->canvas->width : 0);
    lua_pushnumber(L, c->canvas ? c->canvas->height : 0);
    return 2;
}

static int l_canv_getPixelWidth(lua_State *L) { return l_canv_getWidth(L); }
static int l_canv_getPixelHeight(lua_State *L) { return l_canv_getHeight(L); }
static int l_canv_setFilter(lua_State *L) { return 0; }
static int l_canv_getFilter(lua_State *L) {
    lua_pushstring(L, "linear");
    lua_pushstring(L, "linear");
    return 2;
}
static int l_canv_renderTo(lua_State *L) {
    LuaCanvas *c = (LuaCanvas*)luaL_checkudata(L, 1, "LoveCanvas");
    luaL_checktype(L, 2, LUA_TFUNCTION);
    LoveCanvas *prev = LoveGraphics::getCanvas();
    LoveGraphics::setCanvas(c->canvas);
    lua_pushvalue(L, 2);
    if (lua_pcall(L, 0, 0, 0) != 0) {
        printf("[Canvas renderTo error]: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    LoveGraphics::setCanvas(prev);
    return 0;
}
static int l_canv_release(lua_State *L) {
    LuaCanvas *c = (LuaCanvas*)luaL_checkudata(L, 1, "LoveCanvas");
    if (c->canvas) {
        LoveGraphics::destroyCanvas(c->canvas);
        c->canvas = NULL;
    }
    return 0;
}
static int l_canv_gc(lua_State *L) { return l_canv_release(L); }

static int l_font_getWidth(lua_State *L) {
    LuaFont *f = (LuaFont*)lua_touserdata(L, 1);
    if (!f || f->magic != LUA_MAGIC_FONT) f = (LuaFont*)luaL_checkudata(L, 1, "LoveFont");
    const char *str = luaL_checkstring(L, 2);
    float w = LoveGraphics::getFontWidth(f->font, str);
    lua_pushnumber(L, w);
    return 1;
}

static int l_font_getHeight(lua_State *L) {
    LuaFont *f = (LuaFont*)lua_touserdata(L, 1);
    if (!f || f->magic != LUA_MAGIC_FONT) f = (LuaFont*)luaL_checkudata(L, 1, "LoveFont");
    float h = LoveGraphics::getFontHeight(f->font);
    lua_pushnumber(L, h);
    return 1;
}

static int l_font_setLineHeight(lua_State *L) { return 0; }
static int l_font_getLineHeight(lua_State *L) { lua_pushnumber(L, 1.0); return 1; }
static int l_font_setFallbacks(lua_State *L) { return 0; }
static int l_font_release(lua_State *L) {
    LuaFont *f = (LuaFont*)luaL_checkudata(L, 1, "LoveFont");
    if (f->font) {
        LoveGraphics::destroyFont(f->font);
        f->font = NULL;
    }
    return 0;
}
static int l_font_gc(lua_State *L) { return l_font_release(L); }

// Helper to extract string from string or colored-text table
static std::string extract_text_content(lua_State *L, int idx) {
    if (lua_isstring(L, idx)) {
        return lua_tostring(L, idx);
    }
    if (lua_istable(L, idx)) {
        std::string res;
        int len = lua_objlen(L, idx);
        for (int i = 1; i <= len; i++) {
            lua_rawgeti(L, idx, i);
            if (lua_isstring(L, -1)) {
                res += lua_tostring(L, -1);
            }
            lua_pop(L, 1);
        }
        return res;
    }
    return "";
}

static int l_text_set(lua_State *L) {
    LuaText *t = (LuaText*)luaL_checkudata(L, 1, "LoveText");
    std::string s = extract_text_content(L, 2);
    if (t->str) free(t->str);
    t->str = strdup(s.c_str());
    return 0;
}

static int l_text_setf(lua_State *L) {
    LuaText *t = (LuaText*)luaL_checkudata(L, 1, "LoveText");
    std::string s = extract_text_content(L, 2);
    if (t->str) free(t->str);
    t->str = strdup(s.c_str());
    t->wrap_limit = (float)luaL_optnumber(L, 3, 0);
    return 0;
}

static int l_text_clear(lua_State *L) {
    LuaText *t = (LuaText*)luaL_checkudata(L, 1, "LoveText");
    if (t->str) {
        free(t->str);
        t->str = strdup("");
    }
    return 0;
}

static int l_text_setFont(lua_State *L) {
    LuaText *t = (LuaText*)luaL_checkudata(L, 1, "LoveText");
    LuaFont *f = (LuaFont*)luaL_checkudata(L, 2, "LoveFont");
    t->font = f->font;
    return 0;
}

static int l_text_getFont(lua_State *L) {
    LuaText *t = (LuaText*)luaL_checkudata(L, 1, "LoveText");
    LuaFont *f = (LuaFont*)lua_newuserdata(L, sizeof(LuaFont));
    f->magic = LUA_MAGIC_FONT;
    f->font = t->font;
    luaL_getmetatable(L, "LoveFont");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_text_getWidth(lua_State *L) {
    LuaText *t = (LuaText*)luaL_checkudata(L, 1, "LoveText");
    float w = LoveGraphics::getFontWidth(t->font, t->str ? t->str : "");
    lua_pushnumber(L, w);
    return 1;
}

static int l_text_getHeight(lua_State *L) {
    LuaText *t = (LuaText*)luaL_checkudata(L, 1, "LoveText");
    float h = LoveGraphics::getFontHeight(t->font);
    lua_pushnumber(L, h);
    return 1;
}

static int l_text_getDimensions(lua_State *L) {
    LuaText *t = (LuaText*)luaL_checkudata(L, 1, "LoveText");
    float w = LoveGraphics::getFontWidth(t->font, t->str ? t->str : "");
    float h = LoveGraphics::getFontHeight(t->font);
    lua_pushnumber(L, w);
    lua_pushnumber(L, h);
    return 2;
}

static int l_text_release(lua_State *L) {
    LuaText *t = (LuaText*)luaL_checkudata(L, 1, "LoveText");
    if (t->str) {
        free(t->str);
        t->str = NULL;
    }
    return 0;
}

static int l_text_gc(lua_State *L) { return l_text_release(L); }

static int l_shader_send(lua_State *L) { return 0; }
static int l_shader_hasUniform(lua_State *L) { lua_pushboolean(L, 0); return 1; }
static int l_shader_release(lua_State *L) { return 0; }

static int f_gfx_clear(lua_State *L) {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
    if (lua_istable(L, 1)) {
        lua_rawgeti(L, 1, 1); r = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, 1, 2); g = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, 1, 3); b = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, 1, 4); if (!lua_isnil(L, -1)) a = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    } else if (lua_isnumber(L, 1)) {
        r = (float)lua_tonumber(L, 1);
        g = (float)lua_tonumber(L, 2);
        b = (float)lua_tonumber(L, 3);
        if (lua_isnumber(L, 4)) a = (float)lua_tonumber(L, 4);
    }
    LoveGraphics::clear(r, g, b, a);
    return 0;
}

static int f_gfx_present(lua_State *L) {
    LoveGraphics::present();
    return 0;
}

static int f_gfx_setColor(lua_State *L) {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    if (lua_istable(L, 1)) {
        lua_rawgeti(L, 1, 1); r = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, 1, 2); g = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, 1, 3); b = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, 1, 4); if (!lua_isnil(L, -1)) a = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    } else if (lua_isnumber(L, 1)) {
        r = (float)lua_tonumber(L, 1);
        g = (float)lua_tonumber(L, 2);
        b = (float)lua_tonumber(L, 3);
        if (lua_isnumber(L, 4)) a = (float)lua_tonumber(L, 4);
    }
    LoveGraphics::setColor(r, g, b, a);
    return 0;
}

static int f_gfx_getColor(lua_State *L) {
    float r, g, b, a;
    LoveGraphics::getColor(&r, &g, &b, &a);
    lua_pushnumber(L, r);
    lua_pushnumber(L, g);
    lua_pushnumber(L, b);
    lua_pushnumber(L, a);
    return 4;
}

static int f_gfx_setBackgroundColor(lua_State *L) {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
    if (lua_istable(L, 1)) {
        lua_rawgeti(L, 1, 1); r = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, 1, 2); g = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, 1, 3); b = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, 1, 4); if (!lua_isnil(L, -1)) a = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    } else if (lua_isnumber(L, 1)) {
        r = (float)lua_tonumber(L, 1);
        g = (float)lua_tonumber(L, 2);
        b = (float)lua_tonumber(L, 3);
        if (lua_isnumber(L, 4)) a = (float)lua_tonumber(L, 4);
    }
    LoveGraphics::setBackgroundColor(r, g, b, a);
    return 0;
}

static int f_gfx_getBackgroundColor(lua_State *L) {
    float r, g, b, a;
    LoveGraphics::getBackgroundColor(&r, &g, &b, &a);
    lua_pushnumber(L, r);
    lua_pushnumber(L, g);
    lua_pushnumber(L, b);
    lua_pushnumber(L, a);
    return 4;
}

static int f_gfx_push(lua_State *L) { LoveGraphics::push(); return 0; }
static int f_gfx_pop(lua_State *L) { LoveGraphics::pop(); return 0; }
static int f_gfx_origin(lua_State *L) { LoveGraphics::origin(); return 0; }
static int f_gfx_translate(lua_State *L) {
    float x = (float)luaL_optnumber(L, 1, 0.0);
    float y = (float)luaL_optnumber(L, 2, 0.0);
    LoveGraphics::translate(x, y);
    return 0;
}
static int f_gfx_scale(lua_State *L) {
    float sx = (float)luaL_optnumber(L, 1, 1.0);
    float sy = (float)luaL_optnumber(L, 2, sx);
    LoveGraphics::scale(sx, sy);
    return 0;
}
static int f_gfx_rotate(lua_State *L) {
    float rad = (float)luaL_optnumber(L, 1, 0.0);
    LoveGraphics::rotate(rad);
    return 0;
}

static int f_gfx_setScissor(lua_State *L) {
    if (lua_gettop(L) == 0 || lua_isnil(L, 1)) {
        LoveGraphics::clearScissor();
    } else {
        int x = (int)luaL_checknumber(L, 1);
        int y = (int)luaL_checknumber(L, 2);
        int w = (int)luaL_checknumber(L, 3);
        int h = (int)luaL_checknumber(L, 4);
        LoveGraphics::setScissor(x, y, w, h);
    }
    return 0;
}

static int f_gfx_intersectScissor(lua_State *L) {
    int x = (int)luaL_checknumber(L, 1);
    int y = (int)luaL_checknumber(L, 2);
    int w = (int)luaL_checknumber(L, 3);
    int h = (int)luaL_checknumber(L, 4);
    LoveGraphics::setScissor(x, y, w, h);
    return 0;
}

static int f_gfx_getScissor(lua_State *L) {
    // Return dummy full screen or nil if none
    return 0;
}

static int f_gfx_setCanvas(lua_State *L) {
    if (lua_gettop(L) == 0 || lua_isnil(L, 1)) {
        LoveGraphics::setCanvas(NULL);
        return 0;
    }
    if (lua_istable(L, 1)) {
        lua_rawgeti(L, 1, 1);
        if (lua_isuserdata(L, -1)) {
            LuaCanvas *c = (LuaCanvas*)luaL_checkudata(L, -1, "LoveCanvas");
            LoveGraphics::setCanvas(c ? c->canvas : NULL);
        } else {
            LoveGraphics::setCanvas(NULL);
        }
        lua_pop(L, 1);
        return 0;
    }
    if (lua_isuserdata(L, 1)) {
        LuaCanvas *c = (LuaCanvas*)luaL_checkudata(L, 1, "LoveCanvas");
        LoveGraphics::setCanvas(c ? c->canvas : NULL);
        return 0;
    }
    LoveGraphics::setCanvas(NULL);
    return 0;
}

static int f_gfx_getCanvas(lua_State *L) {
    LoveCanvas *c = LoveGraphics::getCanvas();
    if (!c) {
        lua_pushnil(L);
        return 1;
    }
    LuaCanvas *u = (LuaCanvas*)lua_newuserdata(L, sizeof(LuaCanvas));
    u->magic = LUA_MAGIC_CANVAS;
    u->canvas = c;
    luaL_getmetatable(L, "LoveCanvas");
    lua_setmetatable(L, -2);
    return 1;
}

static int f_gfx_newCanvas(lua_State *L) {
    int w = (int)luaL_optnumber(L, 1, SCREEN_WIDTH);
    int h = (int)luaL_optnumber(L, 2, SCREEN_HEIGHT);
    LoveCanvas *canv = LoveGraphics::newCanvas(w, h);
    if (!canv) {
        return luaL_error(L, "Failed to create canvas");
    }
    LuaCanvas *u = (LuaCanvas*)lua_newuserdata(L, sizeof(LuaCanvas));
    u->magic = LUA_MAGIC_CANVAS;
    u->canvas = canv;
    luaL_getmetatable(L, "LoveCanvas");
    lua_setmetatable(L, -2);
    return 1;
}

static int f_gfx_newImage(lua_State *L) {
    LoveImage *img = NULL;
    if (lua_isstring(L, 1)) {
        const char *path = lua_tostring(L, 1);
        img = LoveGraphics::newImageFromFile(path);
    }
    if (!img) {
        // Fallback placeholder image 16x16
        img = new LoveImage();
        img->width = 16;
        img->height = 16;
        img->pixels565 = (uint16_t*)calloc(16 * 16, sizeof(uint16_t));
        img->alpha = (uint8_t*)malloc(16 * 16);
        if (img->pixels565) {
            for (int i = 0; i < 256; i++) img->pixels565[i] = 0xFFFF;
        }
        if (img->alpha) {
            memset(img->alpha, 0xFF, 256);
        }
    }
    LuaImage *u = (LuaImage*)lua_newuserdata(L, sizeof(LuaImage));
    u->magic = LUA_MAGIC_IMAGE;
    u->img = img;
    luaL_getmetatable(L, "LoveImage");
    lua_setmetatable(L, -2);
    return 1;
}

static int f_gfx_newQuad(lua_State *L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    float sw = (float)luaL_checknumber(L, 5);
    float sh = (float)luaL_checknumber(L, 6);

    LuaQuad *q = (LuaQuad*)lua_newuserdata(L, sizeof(LuaQuad));
    q->magic = LUA_MAGIC_QUAD;
    q->quad.x = x; q->quad.y = y; q->quad.w = w; q->quad.h = h;
    q->quad.sw = sw; q->quad.sh = sh;
    luaL_getmetatable(L, "LoveQuad");
    lua_setmetatable(L, -2);
    return 1;
}

static int f_gfx_newFont(lua_State *L) {
    LoveFont *f = NULL;
    float size = 12.0f;
    if (lua_isnumber(L, 1)) {
        size = (float)lua_tonumber(L, 1);
        f = LoveGraphics::newFontFromFile("resources/fonts/m6x11plus.ttf", size);
    } else if (lua_isstring(L, 1)) {
        const char *path = lua_tostring(L, 1);
        if (lua_isnumber(L, 2)) size = (float)lua_tonumber(L, 2);
        f = LoveGraphics::newFontFromFile(path, size);
    }
    if (!f) {
        // Try fallback default font
        f = LoveGraphics::newFontFromFile("resources/fonts/m6x11plus.ttf", size);
    }
    LuaFont *u = (LuaFont*)lua_newuserdata(L, sizeof(LuaFont));
    u->magic = LUA_MAGIC_FONT;
    u->font = f;
    luaL_getmetatable(L, "LoveFont");
    lua_setmetatable(L, -2);
    return 1;
}

static int f_gfx_setFont(lua_State *L) {
    LuaFont *f = (LuaFont*)lua_touserdata(L, 1);
    if (!f || f->magic != LUA_MAGIC_FONT) f = (LuaFont*)luaL_checkudata(L, 1, "LoveFont");
    LoveGraphics::setFont(f->font);
    return 0;
}

static int f_gfx_setNewFont(lua_State *L) {
    f_gfx_newFont(L);
    LuaFont *u = (LuaFont*)lua_touserdata(L, -1);
    if (u && u->magic == LUA_MAGIC_FONT && u->font) LoveGraphics::setFont(u->font);
    return 1;
}

static int f_gfx_getFont(lua_State *L) {
    LoveFont *f = LoveGraphics::getFont();
    if (!f) {
        lua_pushnil(L);
        return 1;
    }
    LuaFont *u = (LuaFont*)lua_newuserdata(L, sizeof(LuaFont));
    u->magic = LUA_MAGIC_FONT;
    u->font = f;
    luaL_getmetatable(L, "LoveFont");
    lua_setmetatable(L, -2);
    return 1;
}

static int f_gfx_newText(lua_State *L) {
    LuaFont *f = (LuaFont*)luaL_checkudata(L, 1, "LoveFont");
    std::string text_str = "";
    if (lua_gettop(L) >= 2) {
        text_str = extract_text_content(L, 2);
    }
    LuaText *t = (LuaText*)lua_newuserdata(L, sizeof(LuaText));
    t->magic = LUA_MAGIC_TEXT;
    t->font = f->font;
    t->str = strdup(text_str.c_str());
    t->wrap_limit = 0;
    luaL_getmetatable(L, "LoveText");
    lua_setmetatable(L, -2);
    return 1;
}

static int f_gfx_newShader(lua_State *L) {
    LuaShader *s = (LuaShader*)lua_newuserdata(L, sizeof(LuaShader));
    const char *name = lua_isstring(L, 1) ? lua_tostring(L, 1) : "shader";
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->name[sizeof(s->name) - 1] = '\0';
    luaL_getmetatable(L, "LoveShader");
    lua_setmetatable(L, -2);
    return 1;
}

static int f_gfx_setShader(lua_State *L) { return 0; }
static int f_gfx_getShader(lua_State *L) { lua_pushnil(L); return 1; }

static int f_gfx_draw(lua_State *L) {
    int top = lua_gettop(L);
    if (top < 1) return 0;

#ifdef LOVE_SKIP_DRAW
    return 0;
#endif

    uint32_t *ud = (uint32_t*)lua_touserdata(L, 1);
    if (!ud) return 0;
    uint32_t magic = *ud;

    if (magic == LUA_MAGIC_IMAGE) {
        LuaImage *img = (LuaImage*)ud;
        if (!img->img) return 0;

        LoveQuad *q = NULL;
        int arg_idx = 2;
        if (top >= 2) {
            uint32_t *ud2 = (uint32_t*)lua_touserdata(L, 2);
            if (ud2 && *ud2 == LUA_MAGIC_QUAD) {
                q = &((LuaQuad*)ud2)->quad;
                arg_idx = 3;
            }
        }

        float x = (float)luaL_optnumber(L, arg_idx + 0, 0);
        float y = (float)luaL_optnumber(L, arg_idx + 1, 0);
        float r = (float)luaL_optnumber(L, arg_idx + 2, 0);
        float sx = (float)luaL_optnumber(L, arg_idx + 3, 1);
        float sy = (float)luaL_optnumber(L, arg_idx + 4, sx);
        float ox = (float)luaL_optnumber(L, arg_idx + 5, 0);
        float oy = (float)luaL_optnumber(L, arg_idx + 6, 0);

        LoveGraphics::draw(img->img, q, x, y, r, sx, sy, ox, oy);
        return 0;
    }

    if (magic == LUA_MAGIC_TEXT) {
        LuaText *t = (LuaText*)ud;
        float x = (float)luaL_optnumber(L, 2, 0);
        float y = (float)luaL_optnumber(L, 3, 0);
        float r = (float)luaL_optnumber(L, 4, 0);
        float sx = (float)luaL_optnumber(L, 5, 1);
        float sy = (float)luaL_optnumber(L, 6, sx);
        float ox = (float)luaL_optnumber(L, 7, 0);
        float oy = (float)luaL_optnumber(L, 8, 0);

        LoveFont *prev = LoveGraphics::getFont();
        if (t->font) LoveGraphics::setFont(t->font);
        LoveGraphics::print(t->str ? t->str : "", x, y, r, sx, sy, ox, oy);
        LoveGraphics::setFont(prev);
        return 0;
    }

    if (magic == LUA_MAGIC_CANVAS) {
        LuaCanvas *c = (LuaCanvas*)ud;
        float x = (float)luaL_optnumber(L, 2, 0);
        float y = (float)luaL_optnumber(L, 3, 0);
        float r = (float)luaL_optnumber(L, 4, 0);
        float sx = (float)luaL_optnumber(L, 5, 1);
        float sy = (float)luaL_optnumber(L, 6, sx);
        float ox = (float)luaL_optnumber(L, 7, 0);
        float oy = (float)luaL_optnumber(L, 8, 0);
        LoveGraphics::drawCanvas(c->canvas, x, y, r, sx, sy, ox, oy);
        return 0;
    }

    return 0;
}

static int f_gfx_rectangle(lua_State *L) {
    const char *mode = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    float rx = (float)luaL_optnumber(L, 6, 0);
    float ry = (float)luaL_optnumber(L, 7, rx);
    LoveGraphics::rectangle(mode, x, y, w, h, rx, ry);
    return 0;
}

static int f_gfx_circle(lua_State *L) {
    const char *mode = luaL_checkstring(L, 1);
    float cx = (float)luaL_checknumber(L, 2);
    float cy = (float)luaL_checknumber(L, 3);
    float r = (float)luaL_checknumber(L, 4);
    LoveGraphics::circle(mode, cx, cy, r);
    return 0;
}

static int f_gfx_polygon(lua_State *L) {
    const char *mode = luaL_checkstring(L, 1);
    float points[64];
    int count = 0;
    if (lua_istable(L, 2)) {
        int n = lua_objlen(L, 2);
        if (n > 64) n = 64;
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, 2, i);
            points[count++] = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
    } else {
        int top = lua_gettop(L);
        for (int i = 2; i <= top && count < 64; i++) {
            points[count++] = (float)lua_tonumber(L, i);
        }
    }
    LoveGraphics::polygon(mode, points, count);
    return 0;
}

static int f_gfx_line(lua_State *L) {
    float points[64];
    int count = 0;
    if (lua_istable(L, 1)) {
        int n = lua_objlen(L, 1);
        if (n > 64) n = 64;
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, 1, i);
            points[count++] = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
    } else {
        int top = lua_gettop(L);
        for (int i = 1; i <= top && count < 64; i++) {
            points[count++] = (float)lua_tonumber(L, i);
        }
    }
    LoveGraphics::line(points, count);
    return 0;
}

static int f_gfx_print(lua_State *L) {
    std::string text = extract_text_content(L, 1);
    float x = (float)luaL_optnumber(L, 2, 0);
    float y = (float)luaL_optnumber(L, 3, 0);
    float r = (float)luaL_optnumber(L, 4, 0);
    float sx = (float)luaL_optnumber(L, 5, 1);
    float sy = (float)luaL_optnumber(L, 6, sx);
    float ox = (float)luaL_optnumber(L, 7, 0);
    float oy = (float)luaL_optnumber(L, 8, 0);
    LoveGraphics::print(text.c_str(), x, y, r, sx, sy, ox, oy);
    return 0;
}

static int f_gfx_printf(lua_State *L) {
    std::string text = extract_text_content(L, 1);
    float x = (float)luaL_optnumber(L, 2, 0);
    float y = (float)luaL_optnumber(L, 3, 0);
    float limit = (float)luaL_checknumber(L, 4);
    const char *align = luaL_optstring(L, 5, "left");
    LoveGraphics::printf(text.c_str(), x, y, limit, align);
    return 0;
}

static int f_gfx_getWidth(lua_State *L) { lua_pushnumber(L, LoveGraphics::getWidth()); return 1; }
static int f_gfx_getHeight(lua_State *L) { lua_pushnumber(L, LoveGraphics::getHeight()); return 1; }
static int f_gfx_getDimensions(lua_State *L) {
    lua_pushnumber(L, LoveGraphics::getWidth());
    lua_pushnumber(L, LoveGraphics::getHeight());
    return 2;
}
static int f_gfx_isActive(lua_State *L) { lua_pushboolean(L, 1); return 1; }
static int f_gfx_isCreated(lua_State *L) { lua_pushboolean(L, 1); return 1; }
static int f_gfx_reset(lua_State *L) {
    LoveGraphics::init();
    return 0;
}
static int f_gfx_setDefaultFilter(lua_State *L) { return 0; }
static int f_gfx_getDefaultFilter(lua_State *L) {
    lua_pushstring(L, "linear");
    lua_pushstring(L, "linear");
    return 2;
}
static int f_gfx_setLineWidth(lua_State *L) {
    LoveGraphics::setLineWidth((float)luaL_checknumber(L, 1));
    return 0;
}
static int f_gfx_setLineStyle(lua_State *L) { return 0; }
static int f_gfx_newVideo(lua_State *L) {
    lua_newtable(L);
    lua_pushstring(L, "dummy_video");
    lua_setfield(L, -2, "tag");
    return 1;
}

// ============================================================================
// love.filesystem bindings
// ============================================================================

static int f_fs_read(lua_State *L) {
    const char *path = NULL;
    if (lua_isstring(L, 1) && lua_isstring(L, 2)) {
        path = lua_tostring(L, 2);
    } else {
        path = luaL_checkstring(L, 1);
    }
    void *data = NULL;
    size_t sz = 0;
    if (!LoveFilesystem::read(path, &data, &sz)) {
        lua_pushnil(L);
        lua_pushstring(L, "Could not open file");
        return 2;
    }
    lua_pushlstring(L, (const char*)data, sz);
    lua_pushnumber(L, (lua_Number)sz);
    LoveFilesystem::freeData(data);
    return 2;
}

static int f_fs_write(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    size_t sz = 0;
    const char *data = luaL_checklstring(L, 2, &sz);
    bool ok = LoveFilesystem::write(path, data, sz);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int f_fs_append(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    size_t sz = 0;
    const char *data = luaL_checklstring(L, 2, &sz);
    bool ok = LoveFilesystem::append(path, data, sz);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int f_fs_getInfo(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    FileInfo info;
    if (!LoveFilesystem::getInfo(path, &info)) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushstring(L, info.type.c_str());
    lua_setfield(L, -2, "type");
    lua_pushnumber(L, (lua_Number)info.size);
    lua_setfield(L, -2, "size");
    lua_pushnumber(L, (lua_Number)info.modtime);
    lua_setfield(L, -2, "modtime");
    return 1;
}

static int f_fs_getDirectoryItems(lua_State *L) {
    const char *path = luaL_optstring(L, 1, "");
    std::vector<std::string> items;
    LoveFilesystem::getDirectoryItems(path, items);
    lua_newtable(L);
    for (size_t i = 0; i < items.size(); i++) {
        lua_pushstring(L, items[i].c_str());
        lua_rawseti(L, -2, (int)(i + 1));
    }
    return 1;
}

static int f_fs_createDirectory(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    lua_pushboolean(L, LoveFilesystem::createDirectory(path) ? 1 : 0);
    return 1;
}

static int f_fs_remove(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    lua_pushboolean(L, LoveFilesystem::remove(path) ? 1 : 0);
    return 1;
}

static int f_fs_getSourceBaseDirectory(lua_State *L) {
    lua_pushstring(L, LoveFilesystem::getSourceBaseDirectory());
    return 1;
}

static int f_fs_getSaveDirectory(lua_State *L) {
    lua_pushstring(L, "./saves");
    return 1;
}

static int f_fs_setIdentity(lua_State *L) { return 0; }
static int f_fs_mount(lua_State *L) { lua_pushboolean(L, 1); return 1; }
static int f_fs_unmount(lua_State *L) { lua_pushboolean(L, 1); return 1; }

static int f_fs_load(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    void *data = NULL;
    size_t sz = 0;
    if (!LoveFilesystem::read(path, &data, &sz)) {
        lua_pushnil(L);
        lua_pushfstring(L, "Could not open file: %s", path);
        return 2;
    }
    int status = luaL_loadbuffer(L, (const char*)data, sz, path);
    LoveFilesystem::freeData(data);
    if (status != 0) {
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    }
    return 1;
}

static int f_fs_lines_it(lua_State *L) {
    // iterator closure: upvalue 1 is file data, upvalue 2 is cursor
    size_t len = 0;
    const char *data = lua_tolstring(L, lua_upvalueindex(1), &len);
    size_t cur = (size_t)lua_tonumber(L, lua_upvalueindex(2));
    if (cur >= len) return 0;

    size_t next_nl = cur;
    while (next_nl < len && data[next_nl] != '\n' && data[next_nl] != '\r') next_nl++;

    lua_pushlstring(L, data + cur, next_nl - cur);

    // skip \r\n
    if (next_nl < len && data[next_nl] == '\r') next_nl++;
    if (next_nl < len && data[next_nl] == '\n') next_nl++;

    lua_pushnumber(L, (lua_Number)next_nl);
    lua_replace(L, lua_upvalueindex(2));
    return 1;
}

static int f_fs_lines(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    void *data = NULL;
    size_t sz = 0;
    if (!LoveFilesystem::read(path, &data, &sz)) {
        return luaL_error(L, "Could not read file: %s", path);
    }
    lua_pushlstring(L, (const char*)data, sz);
    LoveFilesystem::freeData(data);
    lua_pushnumber(L, 0);
    lua_pushcclosure(L, f_fs_lines_it, 2);
    return 1;
}

// ============================================================================
// love.data bindings (Compression / Decompression using miniz)
// ============================================================================

static int f_data_compress(lua_State *L) {
    const char *container = luaL_checkstring(L, 1);
    const char *format = luaL_checkstring(L, 2);
    size_t in_len = 0;
    const char *in_data = luaL_checklstring(L, 3, &in_len);
    int level = luaL_optint(L, 4, 1);

    int flags = TDEFL_DEFAULT_MAX_PROBES;
    if (strcmp(format, "zlib") == 0) {
        flags |= TDEFL_WRITE_ZLIB_HEADER;
    }

    size_t out_len = 0;
    void *out_buf = tdefl_compress_mem_to_heap(in_data, in_len, &out_len, flags);
    if (!out_buf) {
        return luaL_error(L, "Compression failed");
    }
    lua_pushlstring(L, (const char*)out_buf, out_len);
    free(out_buf);
    return 1;
}

static int f_data_decompress(lua_State *L) {
    const char *container = luaL_checkstring(L, 1);
    const char *format = luaL_checkstring(L, 2);
    size_t in_len = 0;
    const char *in_data = luaL_checklstring(L, 3, &in_len);

    int flags = 0;
    if (strcmp(format, "zlib") == 0) {
        flags = TINFL_FLAG_PARSE_ZLIB_HEADER;
    }

    size_t out_len = 0;
    void *out_buf = tinfl_decompress_mem_to_heap(in_data, in_len, &out_len, flags);
    if (!out_buf && flags == 0) {
        // Fallback: try parsing zlib header
        out_buf = tinfl_decompress_mem_to_heap(in_data, in_len, &out_len, TINFL_FLAG_PARSE_ZLIB_HEADER);
    }
    if (!out_buf) {
        return luaL_error(L, "Decompression failed");
    }
    lua_pushlstring(L, (const char*)out_buf, out_len);
    free(out_buf);
    return 1;
}

// ============================================================================
// love.audio bindings
// ============================================================================

static int l_src_play(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    if (s->source) s->source->play();
    return 0;
}

static int l_src_stop(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    if (s->source) s->source->stop();
    return 0;
}

static int l_src_pause(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    if (s->source) s->source->pause();
    return 0;
}

static int l_src_isPlaying(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    lua_pushboolean(L, s->source && s->source->isPlaying() ? 1 : 0);
    return 1;
}

static int l_src_setVolume(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    if (s->source) s->source->setVolume((float)luaL_checknumber(L, 2));
    return 0;
}

static int l_src_getVolume(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    lua_pushnumber(L, s->source ? s->source->getVolume() : 1.0);
    return 1;
}

static int l_src_setPitch(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    if (s->source) s->source->setPitch((float)luaL_checknumber(L, 2));
    return 0;
}

static int l_src_getPitch(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    lua_pushnumber(L, s->source ? s->source->getPitch() : 1.0);
    return 1;
}

static int l_src_setLooping(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    if (s->source) s->source->setLooping(lua_toboolean(L, 2) != 0);
    return 0;
}

static int l_src_isLooping(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    lua_pushboolean(L, s->source && s->source->isLooping() ? 1 : 0);
    return 1;
}

static int l_src_seek(lua_State *L) { return 0; }
static int l_src_tell(lua_State *L) { lua_pushnumber(L, 0); return 1; }
static int l_src_clone(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    LuaSource *u = (LuaSource*)lua_newuserdata(L, sizeof(LuaSource));
    u->source = s->source ? s->source->clone() : NULL;
    luaL_getmetatable(L, "LoveSource");
    lua_setmetatable(L, -2);
    return 1;
}
static int l_src_release(lua_State *L) {
    LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
    if (s->source) {
        delete s->source;
        s->source = NULL;
    }
    return 0;
}
static int l_src_gc(lua_State *L) { return l_src_release(L); }

static int f_audio_newSource(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    const char *type_str = luaL_optstring(L, 2, "static");
    SourceType st = (strcmp(type_str, "stream") == 0) ? SOURCE_STREAM : SOURCE_STATIC;
    LoveSource *src = new LoveSource(path, st);
    LuaSource *u = (LuaSource*)lua_newuserdata(L, sizeof(LuaSource));
    u->source = src;
    luaL_getmetatable(L, "LoveSource");
    lua_setmetatable(L, -2);
    return 1;
}

static int f_audio_play(lua_State *L) {
    if (lua_gettop(L) >= 1 && lua_isuserdata(L, 1)) {
        LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
        if (s && s->source) LoveAudio::play(s->source);
    }
    return 0;
}

static int f_audio_stop(lua_State *L) {
    if (lua_gettop(L) >= 1 && lua_isuserdata(L, 1)) {
        LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
        if (s && s->source) LoveAudio::stop(s->source);
    } else {
        LoveAudio::stop(NULL);
    }
    return 0;
}

static int f_audio_pause(lua_State *L) {
    if (lua_gettop(L) >= 1 && lua_isuserdata(L, 1)) {
        LuaSource *s = (LuaSource*)luaL_checkudata(L, 1, "LoveSource");
        if (s && s->source) s->source->pause();
    }
    return 0;
}

static int f_audio_setVolume(lua_State *L) { return 0; }
static int f_audio_getVolume(lua_State *L) { lua_pushnumber(L, 1.0); return 1; }

// ============================================================================
// love.timer bindings
// ============================================================================

static int f_timer_step(lua_State *L) { lua_pushnumber(L, LoveTimer::step()); return 1; }
static int f_timer_getTime(lua_State *L) { lua_pushnumber(L, LoveTimer::getTime()); return 1; }
static int f_timer_getDelta(lua_State *L) { lua_pushnumber(L, LoveTimer::getDelta()); return 1; }
static int f_timer_getFPS(lua_State *L) { lua_pushnumber(L, LoveTimer::getFPS()); return 1; }
static int f_timer_sleep(lua_State *L) {
    LoveTimer::sleep(luaL_checknumber(L, 1));
    return 0;
}

// ============================================================================
// love.window bindings
// ============================================================================

static int f_win_getMode(lua_State *L) {
    lua_pushnumber(L, LoveWindow::getWidth());
    lua_pushnumber(L, LoveWindow::getHeight());
    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "fullscreen");
    return 3;
}

static int f_win_setMode(lua_State *L) {
    int w = (int)luaL_checknumber(L, 1);
    int h = (int)luaL_checknumber(L, 2);
    LoveWindow::setMode(w, h);
    lua_pushboolean(L, 1);
    return 1;
}

static int f_win_setFullscreen(lua_State *L) { lua_pushboolean(L, 1); return 1; }

static int f_win_updateMode(lua_State *L) { lua_pushboolean(L, 1); return 1; }
static int f_win_getWidth(lua_State *L) { lua_pushnumber(L, LoveWindow::getWidth()); return 1; }
static int f_win_getHeight(lua_State *L) { lua_pushnumber(L, LoveWindow::getHeight()); return 1; }
static int f_win_getDesktopDimensions(lua_State *L) {
    lua_pushnumber(L, LoveWindow::getWidth());
    lua_pushnumber(L, LoveWindow::getHeight());
    return 2;
}
static int f_win_getDisplayCount(lua_State *L) { lua_pushnumber(L, 1); return 1; }
static int f_win_getFullscreenModes(lua_State *L) { lua_newtable(L); return 1; }
static int f_win_getTitle(lua_State *L) { lua_pushstring(L, LoveWindow::getTitle()); return 1; }
static int f_win_setTitle(lua_State *L) { return 0; }
static int f_win_isOpen(lua_State *L) { lua_pushboolean(L, LoveWindow::isOpen() ? 1 : 0); return 1; }
static int f_win_toPixels(lua_State *L) { lua_pushvalue(L, 1); return 1; }
static int f_win_fromPixels(lua_State *L) { lua_pushvalue(L, 1); return 1; }
static int f_win_getDPIScale(lua_State *L) { lua_pushnumber(L, 1.0); return 1; }
static int f_win_showMessageBox(lua_State *L) { lua_pushnumber(L, 1); return 1; }

// ============================================================================
// love.mouse bindings
// ============================================================================

static int f_mouse_getPosition(lua_State *L) {
    lua_pushnumber(L, LoveInput::getMouseX());
    lua_pushnumber(L, LoveInput::getMouseY());
    return 2;
}

static int f_mouse_getX(lua_State *L) { lua_pushnumber(L, LoveInput::getMouseX()); return 1; }
static int f_mouse_getY(lua_State *L) { lua_pushnumber(L, LoveInput::getMouseY()); return 1; }
static int f_mouse_isDown(lua_State *L) {
    int btn = luaL_checkint(L, 1);
    lua_pushboolean(L, LoveInput::isMouseDown(btn) ? 1 : 0);
    return 1;
}
static int f_mouse_setVisible(lua_State *L) {
    LoveInput::setMouseVisible(lua_toboolean(L, 1) != 0);
    return 0;
}
static int f_mouse_isVisible(lua_State *L) {
    lua_pushboolean(L, LoveInput::isMouseVisible() ? 1 : 0);
    return 1;
}
static int f_mouse_setGrabbed(lua_State *L) { return 0; }
static int f_mouse_setRelativeMode(lua_State *L) { return 0; }

// ============================================================================
// love.keyboard bindings
// ============================================================================

static int f_kb_isDown(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    lua_pushboolean(L, LoveInput::isDownKey(key) ? 1 : 0);
    return 1;
}
static int f_kb_setKeyRepeat(lua_State *L) { return 0; }
static int f_kb_setTextInput(lua_State *L) { return 0; }

// ============================================================================
// love.joystick bindings
// ============================================================================

static int l_joy_isGamepad(lua_State *L) { lua_pushboolean(L, 1); return 1; }
static int l_joy_isGamepadDown(lua_State *L) { lua_pushboolean(L, 0); return 1; }
static int l_joy_getGamepadAxis(lua_State *L) { lua_pushnumber(L, 0.0); return 1; }
static int l_joy_getGamepadMappingString(lua_State *L) {
    lua_pushstring(L, "03000000790000001100000000010000,Nintendo Switch Controller,a:b1,b:b2,x:b0,y:b3,back:b8,start:b9,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,platform:Linux,");
    return 1;
}
static int l_joy_getName(lua_State *L) { lua_pushstring(L, "Nintendo Switch Controller"); return 1; }
static int l_joy_getGUID(lua_State *L) { lua_pushstring(L, "sf2000_guid"); return 1; }
static int l_joy_getID(lua_State *L) { lua_pushnumber(L, 1); return 1; }
static int l_joy_isConnected(lua_State *L) { lua_pushboolean(L, 1); return 1; }
static int l_joy_getButtonCount(lua_State *L) { lua_pushnumber(L, 10); return 1; }
static int l_joy_getAxisCount(lua_State *L) { lua_pushnumber(L, 2); return 1; }
static int l_joy_getHatCount(lua_State *L) { lua_pushnumber(L, 0); return 1; }
static int l_joy_isDown(lua_State *L) { lua_pushboolean(L, 0); return 1; }

static int f_joy_getJoysticks(lua_State *L) {
    lua_newtable(L);
    lua_newuserdata(L, sizeof(int));
    luaL_getmetatable(L, "LoveJoystick");
    lua_setmetatable(L, -2);
    lua_rawseti(L, -2, 1);
    return 1;
}

static int f_joy_getJoystickCount(lua_State *L) { lua_pushnumber(L, 1); return 1; }
static int f_joy_loadGamepadMappings(lua_State *L) { lua_pushboolean(L, 1); return 1; }

// ============================================================================
// love.system bindings
// ============================================================================

static int f_sys_getOS(lua_State *L) {
    lua_pushstring(L, "Handheld");
    return 1;
}

static int f_sys_getUsername(lua_State *L) {
    lua_pushstring(L, "Player");
    return 1;
}

static int f_sys_getPowerInfo(lua_State *L) {
    lua_pushstring(L, "battery");
    lua_pushnumber(L, 100);
    lua_pushnumber(L, 3600);
    return 3;
}

static int f_sys_getProcessorCount(lua_State *L) { lua_pushnumber(L, 1); return 1; }
static int f_sys_getClipboardText(lua_State *L) { lua_pushstring(L, ""); return 1; }
static int f_sys_setClipboardText(lua_State *L) { return 0; }
static int f_sys_openURL(lua_State *L) { lua_pushboolean(L, 0); return 1; }

// ============================================================================
// love.event bindings
// ============================================================================

static int f_ev_pump(lua_State *L) { return 0; }

static int f_ev_poll_it(lua_State *L) {
    LoveEventMsg ev;
    if (!LoveInput::popEvent(&ev)) {
        return 0;
    }

    lua_pushstring(L, ev.name.c_str());
    if (ev.kind == 1) { // key: key, scancode, isrepeat
        lua_pushstring(L, ev.str[0].c_str());
        lua_pushstring(L, ev.str[0].c_str());
        lua_pushboolean(L, 0);
        return 4;
    } else if (ev.kind == 3) { // mouse btn: x, y, btn, istouch, presses
        lua_pushnumber(L, ev.num[0]);
        lua_pushnumber(L, ev.num[1]);
        lua_pushnumber(L, ev.num[2]);
        lua_pushboolean(L, 0);
        lua_pushnumber(L, 1);
        return 6;
    } else if (ev.kind == 4) { // mouse move: x, y, dx, dy, istouch
        lua_pushnumber(L, ev.num[0]);
        lua_pushnumber(L, ev.num[1]);
        lua_pushnumber(L, ev.num[2]);
        lua_pushnumber(L, ev.num[3]);
        lua_pushboolean(L, 0);
        return 6;
    } else if (ev.kind == 5) { // gamepad: joystick, button
        lua_newuserdata(L, sizeof(int));
        luaL_getmetatable(L, "LoveJoystick");
        lua_setmetatable(L, -2);
        lua_pushstring(L, ev.str[0].c_str());
        return 3;
    }
    return 1;
}

static int f_ev_poll(lua_State *L) {
    lua_pushcfunction(L, f_ev_poll_it);
    return 1;
}

static int f_ev_push(lua_State *L) { return 0; }
static int f_ev_quit(lua_State *L) {
    printf("[LOVE] quit requested\n");
    return 0;
}
static int f_ev_clear(lua_State *L) {
    LoveInput::clearEvents();
    return 0;
}

// ============================================================================
// love.touch bindings
// ============================================================================
static int f_touch_getTouches(lua_State *L) {
    lua_newtable(L);
    return 1;
}

// ============================================================================
// love.arg bindings
// ============================================================================
static int f_arg_parseGameArguments(lua_State *L) {
    lua_newtable(L);
    return 1;
}

// ============================================================================
// love.math bindings
// ============================================================================
static int f_math_gammaToLinear(lua_State *L) { lua_pushvalue(L, 1); return 1; }
static int f_math_linearToGamma(lua_State *L) { lua_pushvalue(L, 1); return 1; }

static int f_math_random(lua_State *L) {
    int nargs = lua_gettop(L);
    lua_getglobal(L, "math");
    lua_getfield(L, -1, "random");
    lua_insert(L, 1);
    lua_pop(L, 1);
    lua_call(L, nargs, LUA_MULTRET);
    return lua_gettop(L);
}

static int f_math_setRandomSeed(lua_State *L) {
    int nargs = lua_gettop(L);
    lua_getglobal(L, "math");
    lua_getfield(L, -1, "randomseed");
    lua_insert(L, 1);
    lua_pop(L, 1);
    lua_call(L, nargs, 0);
    return 0;
}

static int f_love_getVersion(lua_State *L) {
    lua_pushnumber(L, 10.2);
    return 1;
}

// ============================================================================
// Custom package loader for LoveFilesystem
// ============================================================================

static int love_package_loader(lua_State *L) {
    const char *module_name = luaL_checkstring(L, 1);
    std::string path = module_name;
    for (size_t i = 0; i < path.length(); i++) {
        if (path[i] == '.') path[i] = '/';
    }

    std::string attempts[3] = {
        path + ".lua",
        path + "/init.lua",
        path
    };

    void *data = NULL;
    size_t sz = 0;
    std::string found_path;

    for (int i = 0; i < 3; i++) {
        if (LoveFilesystem::read(attempts[i].c_str(), &data, &sz)) {
            found_path = attempts[i];
            break;
        }
    }

    if (!data) {
        lua_pushfstring(L, "\n\tno file '%s' in love filesystem", path.c_str());
        return 1;
    }

    std::string chunk_name = "@" + found_path;
    int status = luaL_loadbuffer(L, (const char*)data, sz, chunk_name.c_str());
    LoveFilesystem::freeData(data);

    if (status != 0) {
        return lua_error(L);
    }
    return 1;
}

// Dummy modules for https and luasteam
static int dummy_open(lua_State *L) {
    lua_newtable(L);
    return 1;
}

// ============================================================================
// Register all LOVE modules and metatables
// ============================================================================

void LoveLua::registerModules(lua_State *L) {
    // Metatables
    luaL_newmetatable(L, "LoveImage");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_img_gc); lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, l_img_getWidth); lua_setfield(L, -2, "getWidth");
    lua_pushcfunction(L, l_img_getHeight); lua_setfield(L, -2, "getHeight");
    lua_pushcfunction(L, l_img_getDimensions); lua_setfield(L, -2, "getDimensions");
    lua_pushcfunction(L, l_img_setFilter); lua_setfield(L, -2, "setFilter");
    lua_pushcfunction(L, l_img_getFilter); lua_setfield(L, -2, "getFilter");
    lua_pushcfunction(L, l_img_setWrap); lua_setfield(L, -2, "setWrap");
    lua_pushcfunction(L, l_img_getWrap); lua_setfield(L, -2, "getWrap");
    lua_pushcfunction(L, l_img_release); lua_setfield(L, -2, "release");
    lua_pop(L, 1);

    luaL_newmetatable(L, "LoveQuad");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_quad_getViewport); lua_setfield(L, -2, "getViewport");
    lua_pushcfunction(L, l_quad_setViewport); lua_setfield(L, -2, "setViewport");
    lua_pushcfunction(L, l_quad_getTextureDimensions); lua_setfield(L, -2, "getTextureDimensions");
    lua_pushcfunction(L, l_quad_release); lua_setfield(L, -2, "release");
    lua_pop(L, 1);

    luaL_newmetatable(L, "LoveCanvas");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_canv_gc); lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, l_canv_getWidth); lua_setfield(L, -2, "getWidth");
    lua_pushcfunction(L, l_canv_getHeight); lua_setfield(L, -2, "getHeight");
    lua_pushcfunction(L, l_canv_getDimensions); lua_setfield(L, -2, "getDimensions");
    lua_pushcfunction(L, l_canv_getPixelWidth); lua_setfield(L, -2, "getPixelWidth");
    lua_pushcfunction(L, l_canv_getPixelHeight); lua_setfield(L, -2, "getPixelHeight");
    lua_pushcfunction(L, l_canv_setFilter); lua_setfield(L, -2, "setFilter");
    lua_pushcfunction(L, l_canv_getFilter); lua_setfield(L, -2, "getFilter");
    lua_pushcfunction(L, l_canv_renderTo); lua_setfield(L, -2, "renderTo");
    lua_pushcfunction(L, l_canv_release); lua_setfield(L, -2, "release");
    lua_pop(L, 1);

    luaL_newmetatable(L, "LoveFont");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_font_gc); lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, l_font_getWidth); lua_setfield(L, -2, "getWidth");
    lua_pushcfunction(L, l_font_getHeight); lua_setfield(L, -2, "getHeight");
    lua_pushcfunction(L, l_font_setLineHeight); lua_setfield(L, -2, "setLineHeight");
    lua_pushcfunction(L, l_font_getLineHeight); lua_setfield(L, -2, "getLineHeight");
    lua_pushcfunction(L, l_font_setFallbacks); lua_setfield(L, -2, "setFallbacks");
    lua_pushcfunction(L, l_font_release); lua_setfield(L, -2, "release");
    lua_pop(L, 1);

    luaL_newmetatable(L, "LoveText");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_text_gc); lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, l_text_set); lua_setfield(L, -2, "set");
    lua_pushcfunction(L, l_text_setf); lua_setfield(L, -2, "setf");
    lua_pushcfunction(L, l_text_clear); lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, l_text_setFont); lua_setfield(L, -2, "setFont");
    lua_pushcfunction(L, l_text_getFont); lua_setfield(L, -2, "getFont");
    lua_pushcfunction(L, l_text_getWidth); lua_setfield(L, -2, "getWidth");
    lua_pushcfunction(L, l_text_getHeight); lua_setfield(L, -2, "getHeight");
    lua_pushcfunction(L, l_text_getDimensions); lua_setfield(L, -2, "getDimensions");
    lua_pushcfunction(L, l_text_release); lua_setfield(L, -2, "release");
    lua_pop(L, 1);

    luaL_newmetatable(L, "LoveShader");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_shader_send); lua_setfield(L, -2, "send");
    lua_pushcfunction(L, l_shader_hasUniform); lua_setfield(L, -2, "hasUniform");
    lua_pushcfunction(L, l_shader_release); lua_setfield(L, -2, "release");
    lua_pop(L, 1);

    luaL_newmetatable(L, "LoveSource");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_src_gc); lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, l_src_play); lua_setfield(L, -2, "play");
    lua_pushcfunction(L, l_src_stop); lua_setfield(L, -2, "stop");
    lua_pushcfunction(L, l_src_pause); lua_setfield(L, -2, "pause");
    lua_pushcfunction(L, l_src_isPlaying); lua_setfield(L, -2, "isPlaying");
    lua_pushcfunction(L, l_src_setVolume); lua_setfield(L, -2, "setVolume");
    lua_pushcfunction(L, l_src_getVolume); lua_setfield(L, -2, "getVolume");
    lua_pushcfunction(L, l_src_setPitch); lua_setfield(L, -2, "setPitch");
    lua_pushcfunction(L, l_src_getPitch); lua_setfield(L, -2, "getPitch");
    lua_pushcfunction(L, l_src_setLooping); lua_setfield(L, -2, "setLooping");
    lua_pushcfunction(L, l_src_isLooping); lua_setfield(L, -2, "isLooping");
    lua_pushcfunction(L, l_src_seek); lua_setfield(L, -2, "seek");
    lua_pushcfunction(L, l_src_tell); lua_setfield(L, -2, "tell");
    lua_pushcfunction(L, l_src_clone); lua_setfield(L, -2, "clone");
    lua_pushcfunction(L, l_src_release); lua_setfield(L, -2, "release");
    lua_pop(L, 1);

    luaL_newmetatable(L, "LoveJoystick");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_joy_isGamepad); lua_setfield(L, -2, "isGamepad");
    lua_pushcfunction(L, l_joy_isGamepadDown); lua_setfield(L, -2, "isGamepadDown");
    lua_pushcfunction(L, l_joy_getGamepadAxis); lua_setfield(L, -2, "getGamepadAxis");
    lua_pushcfunction(L, l_joy_getGamepadMappingString); lua_setfield(L, -2, "getGamepadMappingString");
    lua_pushcfunction(L, l_joy_getName); lua_setfield(L, -2, "getName");
    lua_pushcfunction(L, l_joy_getGUID); lua_setfield(L, -2, "getGUID");
    lua_pushcfunction(L, l_joy_getID); lua_setfield(L, -2, "getID");
    lua_pushcfunction(L, l_joy_isConnected); lua_setfield(L, -2, "isConnected");
    lua_pushcfunction(L, l_joy_getButtonCount); lua_setfield(L, -2, "getButtonCount");
    lua_pushcfunction(L, l_joy_getAxisCount); lua_setfield(L, -2, "getAxisCount");
    lua_pushcfunction(L, l_joy_getHatCount); lua_setfield(L, -2, "getHatCount");
    lua_pushcfunction(L, l_joy_isDown); lua_setfield(L, -2, "isDown");
    lua_pop(L, 1);

    // Create love table
    lua_newtable(L); // love

    lua_pushstring(L, "11.4"); lua_setfield(L, -2, "_version");
    lua_pushstring(L, "Handheld"); lua_setfield(L, -2, "_os");
    lua_pushcfunction(L, f_love_getVersion); lua_setfield(L, -2, "getVersion");

    // love.graphics
    lua_newtable(L);
    lua_pushcfunction(L, f_gfx_clear); lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, f_gfx_present); lua_setfield(L, -2, "present");
    lua_pushcfunction(L, f_gfx_setColor); lua_setfield(L, -2, "setColor");
    lua_pushcfunction(L, f_gfx_getColor); lua_setfield(L, -2, "getColor");
    lua_pushcfunction(L, f_gfx_setBackgroundColor); lua_setfield(L, -2, "setBackgroundColor");
    lua_pushcfunction(L, f_gfx_getBackgroundColor); lua_setfield(L, -2, "getBackgroundColor");
    lua_pushcfunction(L, f_gfx_push); lua_setfield(L, -2, "push");
    lua_pushcfunction(L, f_gfx_pop); lua_setfield(L, -2, "pop");
    lua_pushcfunction(L, f_gfx_origin); lua_setfield(L, -2, "origin");
    lua_pushcfunction(L, f_gfx_translate); lua_setfield(L, -2, "translate");
    lua_pushcfunction(L, f_gfx_scale); lua_setfield(L, -2, "scale");
    lua_pushcfunction(L, f_gfx_rotate); lua_setfield(L, -2, "rotate");
    lua_pushcfunction(L, f_gfx_setScissor); lua_setfield(L, -2, "setScissor");
    lua_pushcfunction(L, f_gfx_intersectScissor); lua_setfield(L, -2, "intersectScissor");
    lua_pushcfunction(L, f_gfx_getScissor); lua_setfield(L, -2, "getScissor");
    lua_pushcfunction(L, f_gfx_setCanvas); lua_setfield(L, -2, "setCanvas");
    lua_pushcfunction(L, f_gfx_getCanvas); lua_setfield(L, -2, "getCanvas");
    lua_pushcfunction(L, f_gfx_newCanvas); lua_setfield(L, -2, "newCanvas");
    lua_pushcfunction(L, f_gfx_newImage); lua_setfield(L, -2, "newImage");
    lua_pushcfunction(L, f_gfx_newQuad); lua_setfield(L, -2, "newQuad");
    lua_pushcfunction(L, f_gfx_newFont); lua_setfield(L, -2, "newFont");
    lua_pushcfunction(L, f_gfx_setNewFont); lua_setfield(L, -2, "setNewFont");
    lua_pushcfunction(L, f_gfx_setFont); lua_setfield(L, -2, "setFont");
    lua_pushcfunction(L, f_gfx_getFont); lua_setfield(L, -2, "getFont");
    lua_pushcfunction(L, f_gfx_newText); lua_setfield(L, -2, "newText");
    lua_pushcfunction(L, f_gfx_newShader); lua_setfield(L, -2, "newShader");
    lua_pushcfunction(L, f_gfx_setShader); lua_setfield(L, -2, "setShader");
    lua_pushcfunction(L, f_gfx_getShader); lua_setfield(L, -2, "getShader");
    lua_pushcfunction(L, f_gfx_draw); lua_setfield(L, -2, "draw");
    lua_pushcfunction(L, f_gfx_rectangle); lua_setfield(L, -2, "rectangle");
    lua_pushcfunction(L, f_gfx_circle); lua_setfield(L, -2, "circle");
    lua_pushcfunction(L, f_gfx_polygon); lua_setfield(L, -2, "polygon");
    lua_pushcfunction(L, f_gfx_line); lua_setfield(L, -2, "line");
    lua_pushcfunction(L, f_gfx_print); lua_setfield(L, -2, "print");
    lua_pushcfunction(L, f_gfx_printf); lua_setfield(L, -2, "printf");
    lua_pushcfunction(L, f_gfx_getWidth); lua_setfield(L, -2, "getWidth");
    lua_pushcfunction(L, f_gfx_getHeight); lua_setfield(L, -2, "getHeight");
    lua_pushcfunction(L, f_gfx_getDimensions); lua_setfield(L, -2, "getDimensions");
    lua_pushcfunction(L, f_gfx_isActive); lua_setfield(L, -2, "isActive");
    lua_pushcfunction(L, f_gfx_isCreated); lua_setfield(L, -2, "isCreated");
    lua_pushcfunction(L, f_gfx_reset); lua_setfield(L, -2, "reset");
    lua_pushcfunction(L, f_gfx_setDefaultFilter); lua_setfield(L, -2, "setDefaultFilter");
    lua_pushcfunction(L, f_gfx_getDefaultFilter); lua_setfield(L, -2, "getDefaultFilter");
    lua_pushcfunction(L, f_gfx_setLineWidth); lua_setfield(L, -2, "setLineWidth");
    lua_pushcfunction(L, f_gfx_setLineStyle); lua_setfield(L, -2, "setLineStyle");
    lua_pushcfunction(L, f_gfx_newVideo); lua_setfield(L, -2, "newVideo");
    lua_setfield(L, -2, "graphics");

    // love.filesystem
    lua_newtable(L);
    lua_pushcfunction(L, f_fs_read); lua_setfield(L, -2, "read");
    lua_pushcfunction(L, f_fs_write); lua_setfield(L, -2, "write");
    lua_pushcfunction(L, f_fs_append); lua_setfield(L, -2, "append");
    lua_pushcfunction(L, f_fs_getInfo); lua_setfield(L, -2, "getInfo");
    lua_pushcfunction(L, f_fs_getDirectoryItems); lua_setfield(L, -2, "getDirectoryItems");
    lua_pushcfunction(L, f_fs_createDirectory); lua_setfield(L, -2, "createDirectory");
    lua_pushcfunction(L, f_fs_remove); lua_setfield(L, -2, "remove");
    lua_pushcfunction(L, f_fs_getSourceBaseDirectory); lua_setfield(L, -2, "getSourceBaseDirectory");
    lua_pushcfunction(L, f_fs_getSaveDirectory); lua_setfield(L, -2, "getSaveDirectory");
    lua_pushcfunction(L, f_fs_setIdentity); lua_setfield(L, -2, "setIdentity");
    lua_pushcfunction(L, f_fs_mount); lua_setfield(L, -2, "mount");
    lua_pushcfunction(L, f_fs_unmount); lua_setfield(L, -2, "unmount");
    lua_pushcfunction(L, f_fs_load); lua_setfield(L, -2, "load");
    lua_pushcfunction(L, f_fs_lines); lua_setfield(L, -2, "lines");
    lua_setfield(L, -2, "filesystem");

    // love.data
    lua_newtable(L);
    lua_pushcfunction(L, f_data_compress); lua_setfield(L, -2, "compress");
    lua_pushcfunction(L, f_data_decompress); lua_setfield(L, -2, "decompress");
    lua_setfield(L, -2, "data");

    // love.audio
    lua_newtable(L);
    lua_pushcfunction(L, f_audio_newSource); lua_setfield(L, -2, "newSource");
    lua_pushcfunction(L, f_audio_play); lua_setfield(L, -2, "play");
    lua_pushcfunction(L, f_audio_stop); lua_setfield(L, -2, "stop");
    lua_pushcfunction(L, f_audio_pause); lua_setfield(L, -2, "pause");
    lua_pushcfunction(L, f_audio_setVolume); lua_setfield(L, -2, "setVolume");
    lua_pushcfunction(L, f_audio_getVolume); lua_setfield(L, -2, "getVolume");
    lua_setfield(L, -2, "audio");

    // love.sound
    lua_newtable(L);
    lua_setfield(L, -2, "sound");

    // love.timer
    lua_newtable(L);
    lua_pushcfunction(L, f_timer_step); lua_setfield(L, -2, "step");
    lua_pushcfunction(L, f_timer_getTime); lua_setfield(L, -2, "getTime");
    lua_pushcfunction(L, f_timer_getDelta); lua_setfield(L, -2, "getDelta");
    lua_pushcfunction(L, f_timer_getFPS); lua_setfield(L, -2, "getFPS");
    lua_pushcfunction(L, f_timer_sleep); lua_setfield(L, -2, "sleep");
    lua_setfield(L, -2, "timer");

    // love.window
    lua_newtable(L);
    lua_pushcfunction(L, f_win_getMode); lua_setfield(L, -2, "getMode");
    lua_pushcfunction(L, f_win_setMode); lua_setfield(L, -2, "setMode");
    lua_pushcfunction(L, f_win_setFullscreen); lua_setfield(L, -2, "setFullscreen");
    lua_pushcfunction(L, f_win_updateMode); lua_setfield(L, -2, "updateMode");
    lua_pushcfunction(L, f_win_getWidth); lua_setfield(L, -2, "getWidth");
    lua_pushcfunction(L, f_win_getHeight); lua_setfield(L, -2, "getHeight");
    lua_pushcfunction(L, f_win_getDesktopDimensions); lua_setfield(L, -2, "getDesktopDimensions");
    lua_pushcfunction(L, f_win_getDisplayCount); lua_setfield(L, -2, "getDisplayCount");
    lua_pushcfunction(L, f_win_getFullscreenModes); lua_setfield(L, -2, "getFullscreenModes");
    lua_pushcfunction(L, f_win_getTitle); lua_setfield(L, -2, "getTitle");
    lua_pushcfunction(L, f_win_setTitle); lua_setfield(L, -2, "setTitle");
    lua_pushcfunction(L, f_win_isOpen); lua_setfield(L, -2, "isOpen");
    lua_pushcfunction(L, f_win_toPixels); lua_setfield(L, -2, "toPixels");
    lua_pushcfunction(L, f_win_fromPixels); lua_setfield(L, -2, "fromPixels");
    lua_pushcfunction(L, f_win_getDPIScale); lua_setfield(L, -2, "getDPIScale");
    lua_pushcfunction(L, f_win_showMessageBox); lua_setfield(L, -2, "showMessageBox");
    lua_setfield(L, -2, "window");

    // love.mouse
    lua_newtable(L);
    lua_pushcfunction(L, f_mouse_getPosition); lua_setfield(L, -2, "getPosition");
    lua_pushcfunction(L, f_mouse_getX); lua_setfield(L, -2, "getX");
    lua_pushcfunction(L, f_mouse_getY); lua_setfield(L, -2, "getY");
    lua_pushcfunction(L, f_mouse_isDown); lua_setfield(L, -2, "isDown");
    lua_pushcfunction(L, f_mouse_setVisible); lua_setfield(L, -2, "setVisible");
    lua_pushcfunction(L, f_mouse_isVisible); lua_setfield(L, -2, "isVisible");
    lua_pushcfunction(L, f_mouse_setGrabbed); lua_setfield(L, -2, "setGrabbed");
    lua_pushcfunction(L, f_mouse_setRelativeMode); lua_setfield(L, -2, "setRelativeMode");
    lua_setfield(L, -2, "mouse");

    // love.keyboard
    lua_newtable(L);
    lua_pushcfunction(L, f_kb_isDown); lua_setfield(L, -2, "isDown");
    lua_pushcfunction(L, f_kb_setKeyRepeat); lua_setfield(L, -2, "setKeyRepeat");
    lua_pushcfunction(L, f_kb_setTextInput); lua_setfield(L, -2, "setTextInput");
    lua_setfield(L, -2, "keyboard");

    // love.joystick
    lua_newtable(L);
    lua_pushcfunction(L, f_joy_getJoysticks); lua_setfield(L, -2, "getJoysticks");
    lua_pushcfunction(L, f_joy_getJoystickCount); lua_setfield(L, -2, "getJoystickCount");
    lua_pushcfunction(L, f_joy_loadGamepadMappings); lua_setfield(L, -2, "loadGamepadMappings");
    lua_setfield(L, -2, "joystick");

    // love.system
    lua_newtable(L);
    lua_pushcfunction(L, f_sys_getOS); lua_setfield(L, -2, "getOS");
    lua_pushcfunction(L, f_sys_getUsername); lua_setfield(L, -2, "getUsername");
    lua_pushcfunction(L, f_sys_getPowerInfo); lua_setfield(L, -2, "getPowerInfo");
    lua_pushcfunction(L, f_sys_getProcessorCount); lua_setfield(L, -2, "getProcessorCount");
    lua_pushcfunction(L, f_sys_getClipboardText); lua_setfield(L, -2, "getClipboardText");
    lua_pushcfunction(L, f_sys_setClipboardText); lua_setfield(L, -2, "setClipboardText");
    lua_pushcfunction(L, f_sys_openURL); lua_setfield(L, -2, "openURL");
    lua_setfield(L, -2, "system");

    // love.event
    lua_newtable(L);
    lua_pushcfunction(L, f_ev_pump); lua_setfield(L, -2, "pump");
    lua_pushcfunction(L, f_ev_poll); lua_setfield(L, -2, "poll");
    lua_pushcfunction(L, f_ev_push); lua_setfield(L, -2, "push");
    lua_pushcfunction(L, f_ev_quit); lua_setfield(L, -2, "quit");
    lua_pushcfunction(L, f_ev_clear); lua_setfield(L, -2, "clear");
    lua_setfield(L, -2, "event");

    // love.touch
    lua_newtable(L);
    lua_pushcfunction(L, f_touch_getTouches); lua_setfield(L, -2, "getTouches");
    lua_setfield(L, -2, "touch");

    // love.arg
    lua_newtable(L);
    lua_pushcfunction(L, f_arg_parseGameArguments); lua_setfield(L, -2, "parseGameArguments");
    lua_setfield(L, -2, "arg");

    // love.math
    lua_newtable(L);
    lua_pushcfunction(L, f_math_gammaToLinear); lua_setfield(L, -2, "gammaToLinear");
    lua_pushcfunction(L, f_math_linearToGamma); lua_setfield(L, -2, "linearToGamma");
    lua_pushcfunction(L, f_math_random); lua_setfield(L, -2, "random");
    lua_pushcfunction(L, f_math_setRandomSeed); lua_setfield(L, -2, "setRandomSeed");
    lua_setfield(L, -2, "math");

    // love.handlers table with default handlers that dispatch to love.<handler>
    lua_newtable(L);
    lua_setfield(L, -2, "handlers");
    lua_setglobal(L, "love");

    const char *handlers_lua =
        "love.handlers = {\n"
        "  keypressed = function(b, s, r) if love.keypressed then return love.keypressed(b, s, r) end end,\n"
        "  keyreleased = function(b, s) if love.keyreleased then return love.keyreleased(b, s) end end,\n"
        "  mousepressed = function(x, y, b, t, p) if love.mousepressed then return love.mousepressed(x, y, b, t, p) end end,\n"
        "  mousereleased = function(x, y, b, t, p) if love.mousereleased then return love.mousereleased(x, y, b, t, p) end end,\n"
        "  mousemoved = function(x, y, dx, dy, t) if love.mousemoved then return love.mousemoved(x, y, dx, dy, t) end end,\n"
        "  gamepadpressed = function(j, b) if love.gamepadpressed then return love.gamepadpressed(j, b) end end,\n"
        "  gamepadreleased = function(j, b) if love.gamepadreleased then return love.gamepadreleased(j, b) end end,\n"
        "  joystickaxis = function(j, a, v) if love.joystickaxis then return love.joystickaxis(j, a, v) end end,\n"
        "  touchpressed = function(id, x, y, dx, dy, p) if love.touchpressed then return love.touchpressed(id, x, y, dx, dy, p) end end,\n"
        "  touchreleased = function(id, x, y, dx, dy, p) if love.touchreleased then return love.touchreleased(id, x, y, dx, dy, p) end end,\n"
        "  touchmoved = function(id, x, y, dx, dy, p) if love.touchmoved then return love.touchmoved(id, x, y, dx, dy, p) end end,\n"
        "  quit = function() if love.quit then return love.quit() end end\n"
        "}\n";
    luaL_dostring(L, handlers_lua);

    // Setup pure Lua thread and channel system
    const char *thread_lua = 
        "love._channels = {}\n"
        "local Channel = {}\n"
        "Channel.__index = Channel\n"
        "function Channel:push(val)\n"
        "    table.insert(self.queue, val)\n"
        "    if self.name == 'save_request' and type(val) == 'table' then\n"
        "        if val.type == 'save_progress' and val.save_progress then\n"
        "            local p = tostring(val.save_progress.SETTINGS.profile or 1)\n"
        "            if not love.filesystem.getInfo(p) then love.filesystem.createDirectory(p) end\n"
        "            p = p .. '/'\n"
        "            if not love.filesystem.getInfo(p..'meta.jkr') then love.filesystem.append(p..'meta.jkr', 'return {}') end\n"
        "            compress_and_save('settings.jkr', val.save_progress.SETTINGS)\n"
        "            compress_and_save(p..'profile.jkr', val.save_progress.PROFILE)\n"
        "        elseif val.type == 'save_settings' and val.save_settings then\n"
        "            compress_and_save('settings.jkr', val.save_settings)\n"
        "            if val.profile_num and val.save_profile then\n"
        "                compress_and_save(tostring(val.profile_num)..'/profile.jkr', val.save_profile)\n"
        "            end\n"
        "        end\n"
        "    end\n"
        "    return #self.queue\n"
        "end\n"
        "function Channel:pop()\n"
        "    if self.name == 'load_channel' and #self.queue == 0 then return 'finished' end\n"
        "    return table.remove(self.queue, 1)\n"
        "end\n"
        "function Channel:demand()\n"
        "    return self:pop()\n"
        "end\n"
        "function Channel:peek()\n"
        "    return self.queue[1]\n"
        "end\n"
        "function Channel:getCount()\n"
        "    return #self.queue\n"
        "end\n"
        "function Channel:clear()\n"
        "    self.queue = {}\n"
        "end\n"
        "love.thread = {}\n"
        "function love.thread.getChannel(name)\n"
        "    if not love._channels[name] then\n"
        "        love._channels[name] = setmetatable({name = name, queue = {}}, Channel)\n"
        "    end\n"
        "    return love._channels[name]\n"
        "end\n"
        "local Thread = {}\n"
        "Thread.__index = Thread\n"
        "function Thread:start(...) return true end\n"
        "function Thread:wait() return true end\n"
        "function Thread:isRunning() return false end\n"
        "function Thread:getError() return nil end\n"
        "function love.thread.newThread(src)\n"
        "    return setmetatable({src = src}, Thread)\n"
        "end\n";
    luaL_dostring(L, thread_lua);

    // Register preloads for bit, https, luasteam, and love modules
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");

    lua_pushcfunction(L, luaopen_bit);
    lua_setfield(L, -2, "bit");

    lua_pushcfunction(L, dummy_open);
    lua_setfield(L, -2, "https");

    lua_pushcfunction(L, dummy_open);
    lua_setfield(L, -2, "luasteam");

    // Preload love.* submodules
    luaL_dostring(L,
        "for _, sub in ipairs({'graphics','filesystem','data','audio','sound','timer','window','mouse','keyboard','joystick','system','thread','event','touch','arg','math'}) do\n"
        "    package.preload['love.' .. sub] = function() return love[sub] end\n"
        "end\n"
    );
    lua_pop(L, 1); // pop preload

    // Insert love loader into package.loaders at position 2
    lua_getfield(L, -1, "loaders");
    int num_loaders = lua_objlen(L, -1);
    for (int i = num_loaders; i >= 2; i--) {
        lua_rawgeti(L, -1, i);
        lua_rawseti(L, -2, i + 1);
    }
    lua_pushcfunction(L, love_package_loader);
    lua_rawseti(L, -2, 2);
    lua_pop(L, 2); // pop loaders & package

    // Set jit dummy table so jit.arch or jit.off() doesn't fail
    luaL_dostring(L, "jit = jit or { arch = 'mips', off = function() end, on = function() end }");
}

static int lua_panic_handler(lua_State *L) {
    const char *err = lua_tostring(L, -1);
    LOVE_LOG("\n==================== LUA PANIC ====================\n%s\n", err ? err : "unknown error");
    lua_Debug ar;
    int level = 0;
    while (lua_getstack(L, level, &ar)) {
        lua_getinfo(L, "nSl", &ar);
        LOVE_LOG("  [%d] %s:%d in %s\n", level, ar.short_src, ar.currentline, ar.name ? ar.name : "?");
        level++;
    }
    LOVE_LOG("====================================================\n\n");
    return 0;
}

bool LoveLua::init(const char *game_path) {
    if (s_L) deinit();

    LOVE_LOG("[LOVE] Initializing engine for path: %s\n", game_path ? game_path : ".");
    LoveFilesystem::init(game_path);
    LoveGraphics::init();
    LoveTimer::init();
    LoveInput::init();

    s_L = luaL_newstate();
    if (!s_L) {
        LOVE_LOG("[LOVE Error] Failed to create Lua state\n");
        return false;
    }

    lua_atpanic(s_L, lua_panic_handler);
    luaL_openlibs(s_L);
    LOVE_LOG("[LOVE Debug] 1. Calling registerModules\n");
    registerModules(s_L);
    LOVE_LOG("[LOVE Debug] 2. registerModules finished\n");

    // Load default font if exists
    LoveFont *default_font = LoveGraphics::newFontFromFile("resources/fonts/m6x11plus.ttf", 16.0f);
    if (default_font) {
        LoveGraphics::setFont(default_font);
    }
    LOVE_LOG("[LOVE Debug] 3. Default font checked\n");

    // Run conf.lua if present
    void *conf_data = NULL;
    size_t conf_size = 0;
    if (LoveFilesystem::read("conf.lua", &conf_data, &conf_size)) {
        LOVE_LOG("[LOVE Debug] 4. Reading conf.lua\n");
        if (luaL_loadbuffer(s_L, (const char*)conf_data, conf_size, "@conf.lua") == 0) {
            if (lua_pcall(s_L, 0, 0, 0) == 0) {
                lua_getglobal(s_L, "love");
                lua_getfield(s_L, -1, "conf");
                if (lua_isfunction(s_L, -1)) {
                    lua_newtable(s_L);
                    lua_newtable(s_L);
                    lua_setfield(s_L, -2, "window");
                    if (lua_pcall(s_L, 1, 0, 0) != 0) {
                        LOVE_LOG("[LOVE Conf Error] %s\n", lua_tostring(s_L, -1));
                        lua_pop(s_L, 1);
                    }
                } else {
                    lua_pop(s_L, 1);
                }
                lua_pop(s_L, 1);
            } else {
                LOVE_LOG("[LOVE Conf Load Error] %s\n", lua_tostring(s_L, -1));
                lua_pop(s_L, 1);
            }
        }
        LoveFilesystem::freeData(conf_data);
    }
    LOVE_LOG("[LOVE Debug] 5. conf.lua step finished\n");

    // Run main.lua
    void *main_data = NULL;
    size_t main_size = 0;
    if (!LoveFilesystem::read("main.lua", &main_data, &main_size)) {
        LOVE_LOG("[LOVE Error] main.lua not found in game path\n");
        return false;
    }
    LOVE_LOG("[LOVE Debug] 6. main.lua read (%zu bytes)\n", main_size);

    if (luaL_loadbuffer(s_L, (const char*)main_data, main_size, "@main.lua") != 0) {
        LOVE_LOG("[LOVE Error] Failed to compile main.lua: %s\n", lua_tostring(s_L, -1));
        lua_pop(s_L, 1);
        LoveFilesystem::freeData(main_data);
        return false;
    }
    LoveFilesystem::freeData(main_data);
    LOVE_LOG("[LOVE Debug] 7. main.lua compiled, calling pcall...\n");

    if (lua_pcall(s_L, 0, 0, 0) != 0) {
        LOVE_LOG("[LOVE Error] Execution failed in main.lua: %s\n", lua_tostring(s_L, -1));
        lua_pop(s_L, 1);
        return false;
    }
    LOVE_LOG("[LOVE Debug] 8. main.lua pcall finished\n");

    // Check love.run
    lua_getglobal(s_L, "love");
    if (lua_istable(s_L, -1)) {
        lua_getfield(s_L, -1, "run");
        if (lua_isfunction(s_L, -1)) {
            // Call love.run()
            if (lua_pcall(s_L, 0, 1, 0) == 0) {
                if (lua_isfunction(s_L, -1)) {
                    // love.run() returned a main loop step function! (Like Balatro does)
                    s_runFuncRef = luaL_ref(s_L, LUA_REGISTRYINDEX);
                    LOVE_LOG("[LOVE] Captured custom love.run loop function\n");
                } else {
                    lua_pop(s_L, 1);
                }
            } else {
                LOVE_LOG("[LOVE Error] love.run() failed: %s\n", lua_tostring(s_L, -1));
                lua_pop(s_L, 1);
            }
        } else {
            lua_pop(s_L, 1);
        }
    }
    lua_pop(s_L, 1);

    // If love.run didn't return a step function, call love.load() if present
    if (s_runFuncRef == LUA_NOREF) {
        lua_getglobal(s_L, "love");
        if (lua_istable(s_L, -1)) {
            lua_getfield(s_L, -1, "load");
            if (lua_isfunction(s_L, -1)) {
                if (lua_pcall(s_L, 0, 0, 0) != 0) {
                    LOVE_LOG("[LOVE Error] love.load() failed: %s\n", lua_tostring(s_L, -1));
                    lua_pop(s_L, 1);
                }
            } else {
                lua_pop(s_L, 1);
            }
        }
        lua_pop(s_L, 1);
    }

    LOVE_LOG("[LOVE] Initialization completed successfully\n");
    return true;
}

void LoveLua::deinit() {
    if (s_L) {
        if (s_runFuncRef != LUA_NOREF) {
            luaL_unref(s_L, LUA_REGISTRYINDEX, s_runFuncRef);
            s_runFuncRef = LUA_NOREF;
        }
        lua_close(s_L);
        s_L = NULL;
    }
    LoveFilesystem::deinit();
    LoveGraphics::deinit();
}

static void dispatch_pending_events(lua_State *L) {
    LoveEventMsg ev;
    while (LoveInput::popEvent(&ev)) {
        lua_getglobal(L, "love");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        lua_getfield(L, -1, "handlers");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            continue;
        }
        lua_getfield(L, -1, ev.name.c_str());
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 3);
            continue;
        }
        int nargs = 0;
        if (ev.kind == 1) {
            lua_pushstring(L, ev.str[0].c_str());
            lua_pushstring(L, ev.str[0].c_str());
            lua_pushboolean(L, 0);
            nargs = 3;
        } else if (ev.kind == 5) {
            lua_newuserdata(L, sizeof(int));
            luaL_getmetatable(L, "LoveJoystick");
            lua_setmetatable(L, -2);
            lua_pushstring(L, ev.str[0].c_str());
            nargs = 2;
        } else {
            lua_pop(L, 3);
            continue;
        }
        if (lua_pcall(L, nargs, 0, 0) != 0) {
            LOVE_LOG("[LOVE Event Error] %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        lua_pop(L, 2);
    }
}

bool LoveLua::step() {
    if (!s_L) return false;

    if (s_runFuncRef != LUA_NOREF) {
        // Balatro's custom loop step function
        lua_rawgeti(s_L, LUA_REGISTRYINDEX, s_runFuncRef);
        if (lua_pcall(s_L, 0, 1, 0) != 0) {
            LOVE_LOG("[LOVE Step Error]: %s\n", lua_tostring(s_L, -1));
            lua_getglobal(s_L, "debug");
            if (lua_istable(s_L, -1)) {
                lua_getfield(s_L, -1, "traceback");
                if (lua_isfunction(s_L, -1)) {
                    lua_pushvalue(s_L, -3);
                    lua_call(s_L, 1, 1);
                    LOVE_LOG("[LOVE Step Traceback]: %s\n", lua_tostring(s_L, -1));
                    lua_pop(s_L, 1);
                }
                lua_pop(s_L, 1);
            }
            lua_pop(s_L, 1);
            return false;
        }
        if (lua_isnumber(s_L, -1)) {
            // Function returned a quit code
            int code = (int)lua_tonumber(s_L, -1);
            lua_pop(s_L, 1);
            return (code == 0);
        }
        lua_pop(s_L, 1);
        return true;
    }

    // Fallback default update & draw
    double dt = LoveTimer::step();

    LoveGraphics::clearBackground();
    dispatch_pending_events(s_L);

    lua_getglobal(s_L, "love");
    if (!lua_istable(s_L, -1)) {
        lua_pop(s_L, 1);
        return false;
    }

    lua_getfield(s_L, -1, "update");
    if (lua_isfunction(s_L, -1)) {
        lua_pushnumber(s_L, dt);
        if (lua_pcall(s_L, 1, 0, 0) != 0) {
            LOVE_LOG("[LOVE Update Error]: %s\n", lua_tostring(s_L, -1));
            lua_pop(s_L, 1);
        }
    } else {
        lua_pop(s_L, 1);
    }

    lua_getfield(s_L, -1, "draw");
    if (lua_isfunction(s_L, -1)) {
        if (lua_pcall(s_L, 0, 0, 0) != 0) {
            LOVE_LOG("[LOVE Draw Error]: %s\n", lua_tostring(s_L, -1));
            lua_pop(s_L, 1);
        }
    } else {
        lua_pop(s_L, 1);
    }

    lua_pop(s_L, 1); // pop love
    LoveGraphics::present();
    return true;
}
