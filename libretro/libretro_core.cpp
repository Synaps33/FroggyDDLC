#include "libretro.h"
#include "love_lua.h"
#include "love_graphics.h"
#include "love_audio.h"
#include "love_input.h"
#include "love_filesystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>

static char *get_dirname(char *path) {
    char *p = strrchr(path, '/');
    if (!p) {
        static char dot[] = ".";
        return dot;
    }
    if (p == path) {
        *(p + 1) = '\0';
    } else {
        *p = '\0';
    }
    return path;
}

#if defined(SF2000) || defined(GB300) || defined(GB300V2)
extern "C" void xlog(const char *fmt, ...);
#define CORE_LOG(...) do { printf(__VA_ARGS__); xlog(__VA_ARGS__); } while(0)
#else
#define CORE_LOG(...) printf(__VA_ARGS__)
#endif

#define CORE_NAME "DDLC-SF2000"
#define CORE_VERSION "11.4"
#define CORE_EXTS "ddlc|love|gba|sf2k|zip"

#define SCREEN_W SCREEN_WIDTH
#define SCREEN_H SCREEN_HEIGHT
#define AUDIO_RATE 22050
#define AUDIO_FRAMES_MAX 368 // ceil(22050 / 60)
#define AUDIO_FPS 60

static retro_environment_t        s_environ_cb = NULL;
static retro_video_refresh_t      s_video_cb = NULL;
static retro_audio_sample_t       s_audio_cb = NULL;
static retro_audio_sample_batch_t s_audio_batch_cb = NULL;
static retro_input_poll_t         s_input_poll_cb = NULL;
static retro_input_state_t        s_input_state_cb = NULL;

static uint16_t s_framebuffer[SCREEN_W * SCREEN_H];
static int16_t  s_audio_buffer[AUDIO_FRAMES_MAX * 2];
static uint32_t s_audio_remainder = 0;
static bool     s_game_loaded = false;
static char     s_game_path[512] = {0};

void retro_init(void) {
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    LoveGraphics::init();
    LoveAudio::init(AUDIO_RATE);
    s_audio_remainder = 0;
    LoveInput::init();
    LoveGraphics::setScreenBuffer(s_framebuffer, SCREEN_W, SCREEN_H);
}

void retro_deinit(void) {
    if (s_game_loaded) {
        LoveLua::deinit();
        s_game_loaded = false;
    }
    LoveAudio::deinit();
    LoveGraphics::deinit();
}

unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name     = CORE_NAME;
    info->library_version  = CORE_VERSION;
    info->valid_extensions = CORE_EXTS;
    info->need_fullpath    = true;
    info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    memset(info, 0, sizeof(*info));
    info->geometry.base_width   = SCREEN_W;
    info->geometry.base_height  = SCREEN_H;
    info->geometry.max_width    = SCREEN_W;
    info->geometry.max_height   = SCREEN_H;
    info->geometry.aspect_ratio = 4.0f / 3.0f;

    info->timing.fps         = 60.0;
    info->timing.sample_rate = AUDIO_RATE;
}

void retro_set_environment(retro_environment_t cb) {
    s_environ_cb = cb;
    if (cb) {
        enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
        cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
    }
}

void retro_set_video_refresh(retro_video_refresh_t cb) { s_video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { s_audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { s_audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { s_input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { s_input_state_cb = cb; }
void retro_set_controller_port_device(unsigned port, unsigned device) {}

void retro_reset(void) {
    if (s_game_loaded) {
        LoveLua::deinit();
        s_game_loaded = LoveLua::init(s_game_path);
    }
}

static bool file_readable(const char *filepath) {
    if (!filepath || !filepath[0]) return false;
    struct stat st;
    if (stat(filepath, &st) == 0) return true;
    FILE *f = fopen(filepath, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

bool retro_load_game(const struct retro_game_info *game) {
    if (!game || !game->path) return false;

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    if (s_environ_cb) {
        s_environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
    }

    LoveGraphics::setScreenBuffer(s_framebuffer, SCREEN_W, SCREEN_H);

    const char *path = game->path;
    strncpy(s_game_path, path, sizeof(s_game_path) - 1);
    s_game_path[sizeof(s_game_path) - 1] = '\0';

    CORE_LOG("[Libretro] Loading game: %s\n", path);

    // Check directory of path
    char dir_buf[512];
    strncpy(dir_buf, path, sizeof(dir_buf) - 1);
    dir_buf[sizeof(dir_buf) - 1] = '\0';
    char *dir = get_dirname(dir_buf);

    // PRIORITY 1: Unpacked directories with main.lua
    // Running from loose files on FAT32 avoids miniz central-dir parsing & memory overhead
    char probe_dir_main[512];
    snprintf(probe_dir_main, sizeof(probe_dir_main), "%s/main.lua", dir);
    if (file_readable(probe_dir_main)) {
        CORE_LOG("[Libretro] Found unpacked game directory with main.lua: %s\n", dir);
        s_game_loaded = LoveLua::init(dir);
        if (s_game_loaded) return true;
    }

    char probe_path_main[512];
    snprintf(probe_path_main, sizeof(probe_path_main), "%s/main.lua", path);
    if (file_readable(probe_path_main)) {
        CORE_LOG("[Libretro] Found unpacked game directory at path: %s\n", path);
        s_game_loaded = LoveLua::init(path);
        if (s_game_loaded) return true;
    }

    const char *unpacked_candidates[] = {
        "/mnt/sda1/ROMS/ddlc",
        "ROMS/ddlc",
        "ddlc_data",
        "game",
        "."
    };
    for (size_t i = 0; i < sizeof(unpacked_candidates) / sizeof(unpacked_candidates[0]); i++) {
        char test_main[512];
        snprintf(test_main, sizeof(test_main), "%s/main.lua", unpacked_candidates[i]);
        if (file_readable(test_main)) {
            CORE_LOG("[Libretro] Found unpacked game candidate: %s\n", unpacked_candidates[i]);
            s_game_loaded = LoveLua::init(unpacked_candidates[i]);
            if (s_game_loaded) return true;
        }
    }

    // PRIORITY 2: If path is a .love or .zip file directly, load it
    size_t len = strlen(path);
    if ((len > 5 && strcmp(path + len - 5, ".love") == 0) ||
        (len > 4 && strcmp(path + len - 4, ".zip") == 0)) {
        CORE_LOG("[Libretro] Loading zip archive directly: %s\n", path);
        s_game_loaded = LoveLua::init(path);
        return s_game_loaded;
    }

    // PRIORITY 3: Zip / .love archives in standard locations (fallback)
    char probe1[512], probe2[512], probe3[512];
    snprintf(probe1, sizeof(probe1), "%s/DDLC-LOVE.love", dir);
    snprintf(probe2, sizeof(probe2), "%s/ddlc.love", dir);
    snprintf(probe3, sizeof(probe3), "%s/DDLC.love", dir);

    const char *zip_candidates[] = {
        probe1,
        probe2,
        probe3,
        "/mnt/sda1/ROMS/ddlc/DDLC-LOVE.love",
        "ROMS/ddlc/DDLC-LOVE.love",
        "ddlc_data/DDLC-LOVE.love",
        "DDLC-LOVE.love",
        "ddlc.love"
    };

    for (size_t i = 0; i < sizeof(zip_candidates) / sizeof(zip_candidates[0]); i++) {
        if (zip_candidates[i][0] && file_readable(zip_candidates[i])) {
            CORE_LOG("[Libretro] Found zip archive candidate: %s\n", zip_candidates[i]);
            s_game_loaded = LoveLua::init(zip_candidates[i]);
            if (s_game_loaded) return true;
        }
    }

    // Fallback directly to given path
    s_game_loaded = LoveLua::init(path);
    return s_game_loaded;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) {
    return false;
}

void retro_unload_game(void) {
    if (s_game_loaded) {
        LoveLua::deinit();
        s_game_loaded = false;
    }
}

unsigned retro_get_region(void) {
    return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id) { return NULL; }
size_t retro_get_memory_size(unsigned id) { return 0; }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void *data, size_t size) { return false; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}

void retro_run(void) {
    if (!s_game_loaded) return;

    if (s_input_poll_cb) {
        s_input_poll_cb();
    }

    GamepadState pad;
    memset(&pad, 0, sizeof(pad));

    if (s_input_state_cb) {
        pad.up     = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) != 0;
        pad.down   = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0;
        pad.left   = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) != 0;
        pad.right  = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) != 0;
        pad.a      = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) != 0;
        pad.b      = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) != 0;
        pad.x      = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) != 0;
        pad.y      = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y) != 0;
        pad.l      = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L) != 0;
        pad.r      = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R) != 0;
        pad.start  = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START) != 0;
        pad.select = s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT) != 0;
    }

    LoveInput::update(LoveLua::getState(), pad);

#ifdef LOVE_PROFILE
    extern uint32_t g_gfx_draw_calls, g_gfx_touched_px, g_gfx_fill_px, g_gfx_glyph_px, g_gfx_img_conv_px, g_gfx_canvas_px;
    extern unsigned long long g_lua_vm_insns;
    struct timespec _t0, _t1;
    clock_gettime(CLOCK_MONOTONIC, &_t0);
    uint32_t _d0 = g_gfx_draw_calls, _p0 = g_gfx_touched_px, _f0 = g_gfx_fill_px, _g0 = g_gfx_glyph_px, _c0 = g_gfx_canvas_px;
#endif

    LoveLua::step();

#ifdef LOVE_PROFILE
    clock_gettime(CLOCK_MONOTONIC, &_t1);
    static double _acc_ms = 0.0;
    static int _prof_frames = 0;
    _acc_ms += (_t1.tv_sec - _t0.tv_sec) * 1000.0 + (_t1.tv_nsec - _t0.tv_nsec) / 1e6;
    _prof_frames++;
    static uint32_t _sd = 0, _sp = 0, _sf = 0, _sg = 0, _sc = 0;
    static unsigned long long _vm_last = 0, _sv = 0;
    _sv += g_lua_vm_insns - _vm_last; _vm_last = g_lua_vm_insns;
    _sd += g_gfx_draw_calls - _d0;
    _sp += g_gfx_touched_px - _p0;
    _sf += g_gfx_fill_px - _f0;
    _sg += g_gfx_glyph_px - _g0;
    _sc += g_gfx_canvas_px - _c0;
    if (_prof_frames == 60) {
        CORE_LOG("[PROF] %.2f ms/frame | vm=%.1fM/f | per-frame: draw=%.1f px=%.0f blit=%.0f\n",
            _acc_ms / 60.0, (double)(_sv / 60) / 1e6,
            (double)_sd / 60.0, (double)_sp / 60.0, (double)_sc / 60.0);
        _acc_ms = 0.0; _prof_frames = 0; _sd = _sp = _sf = _sg = _sc = 0;
    }
#endif

    static int s_frame_cnt = 0;
    s_frame_cnt++;
    if (s_frame_cnt % 60 == 0) {
        lua_State *L = LoveLua::getState();
        if (L) {
            lua_getglobal(L, "G");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "STAGE");
                int stage = (int)lua_tointeger(L, -1);
                lua_pop(L, 1);
                lua_getfield(L, -1, "STATE");
                int state = (int)lua_tointeger(L, -1);
                lua_pop(L, 1);
                CORE_LOG("[Core Status] Frame %d: G.STAGE=%d, G.STATE=%d\n", s_frame_cnt, stage, state);
            }
            lua_pop(L, 1);
        }
    }

    if (s_video_cb) {
        s_video_cb(LoveGraphics::getScreenBuffer(), SCREEN_W, SCREEN_H, SCREEN_W * sizeof(uint16_t));
    }

    if (s_audio_batch_cb) {
        s_audio_remainder += AUDIO_RATE;
        size_t frames = s_audio_remainder / AUDIO_FPS;
        s_audio_remainder %= AUDIO_FPS;
        if (frames > AUDIO_FRAMES_MAX) frames = AUDIO_FRAMES_MAX;
        LoveAudio::update(s_audio_buffer, frames);
        s_audio_batch_cb(s_audio_buffer, frames);
    }
}

extern "C" void ddlc_debug_query(char *out, int cap) {
    if (!out || cap <= 0) return;
    lua_State *L = LoveLua::getState();
    if (!L) {
        snprintf(out, cap, "state=none");
        return;
    }
    int base = lua_gettop(L);
    const char *state = "";
    lua_getglobal(L, "state");
    if (lua_isstring(L, -1)) state = lua_tostring(L, -1);
    lua_pop(L, 1);

    int cl = 0, chapter = 0, xaload = 0;
    lua_getglobal(L, "cl");
    if (lua_isnumber(L, -1)) cl = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getglobal(L, "chapter");
    if (lua_isnumber(L, -1)) chapter = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getglobal(L, "xaload");
    if (lua_isnumber(L, -1)) xaload = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    int menu = 0, keyboard = 0, poem = 0, full = 0, event = 0;
    lua_getglobal(L, "menu_enabled");
    menu = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getglobal(L, "keyboard");
    keyboard = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getglobal(L, "poem_enabled");
    poem = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getglobal(L, "print_full_text");
    full = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getglobal(L, "event_enabled");
    event = lua_toboolean(L, -1);
    lua_pop(L, 1);

    const char *mtype = "";
    lua_getglobal(L, "menu_type");
    if (lua_isstring(L, -1)) mtype = lua_tostring(L, -1);
    lua_pop(L, 1);

    const char *ct = "";
    lua_getglobal(L, "ct");
    if (lua_isstring(L, -1)) ct = lua_tostring(L, -1);
    lua_pop(L, 1);
    const char *cdisp = "";
    lua_getglobal(L, "c_disp");
    if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, 1);
        if (lua_isstring(L, -1)) cdisp = lua_tostring(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    int msel = 0;
    lua_getglobal(L, "m_selected");
    if (lua_isnumber(L, -1)) msel = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    snprintf(out, cap, "state=%s cl=%d ch=%d x=%d menu=%d mtype=%s msel=%d kb=%d poem=%d full=%d ev=%d ct=%s disp=%s",
             state, cl, chapter, xaload, menu, mtype, msel, keyboard, poem, full, event, ct, cdisp);
    lua_settop(L, base);
}
