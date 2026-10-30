#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libretro.h"

#define MAX_W 640
#define MAX_H 480

static int      s_w = 320;
static int      s_h = 240;
static uint16_t s_frame[MAX_W * MAX_H];
static int      s_frames_dumped = 0;

static void video_refresh_cb(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (!data) return;
    s_w = width;
    s_h = height;
    if (s_w > MAX_W) s_w = MAX_W;
    if (s_h > MAX_H) s_h = MAX_H;

    const uint8_t *src = (const uint8_t*)data;
    for (int y = 0; y < s_h; y++) {
        memcpy(&s_frame[y * MAX_W], src + y * pitch, s_w * sizeof(uint16_t));
    }
}

static long s_audio_nonzero = 0;
static FILE *s_wav = NULL;

static void audio_sample_cb(int16_t left, int16_t right) {
    if (left || right) s_audio_nonzero++;
}

static size_t audio_batch_cb(const int16_t *data, size_t frames) {
    for (size_t i = 0; i < frames * 2; i++) if (data[i]) s_audio_nonzero++;
    if (s_wav) fwrite(data, sizeof(int16_t), frames * 2, s_wav);
    return frames;
}

static void wav_header(FILE *f, int rate) {
    unsigned char h[44] = {0};
    memcpy(h, "RIFF", 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[20] = 1; h[22] = 2;
    h[24] = rate & 0xff; h[25] = (rate >> 8) & 0xff; h[26] = (rate >> 16) & 0xff; h[27] = (rate >> 24) & 0xff;
    int br = rate * 4;
    h[28] = br & 0xff; h[29] = (br >> 8) & 0xff; h[30] = (br >> 16) & 0xff; h[31] = (br >> 24) & 0xff;
    h[32] = 4; h[34] = 16;
    memcpy(h + 36, "data", 4);
    fwrite(h, 1, 44, f);
}
static void input_poll_cb(void) {}
static uint32_t s_pad_state = 0;

enum {
    BTN_A      = (1 << 0),
    BTN_START  = (1 << 1),
    BTN_B      = (1 << 2),
    BTN_X      = (1 << 3),
    BTN_Y      = (1 << 4),
    BTN_L      = (1 << 5),
    BTN_R      = (1 << 6),
    BTN_UP     = (1 << 7),
    BTN_DOWN   = (1 << 8),
    BTN_LEFT   = (1 << 9),
    BTN_RIGHT  = (1 << 10),
    BTN_SELECT = (1 << 11),
};

static int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port == 0 && device == RETRO_DEVICE_JOYPAD) {
        if (id == RETRO_DEVICE_ID_JOYPAD_A      && (s_pad_state & BTN_A)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_START  && (s_pad_state & BTN_START)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_B      && (s_pad_state & BTN_B)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_X      && (s_pad_state & BTN_X)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_Y      && (s_pad_state & BTN_Y)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_L      && (s_pad_state & BTN_L)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_R      && (s_pad_state & BTN_R)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_UP     && (s_pad_state & BTN_UP)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_DOWN   && (s_pad_state & BTN_DOWN)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_LEFT   && (s_pad_state & BTN_LEFT)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_RIGHT  && (s_pad_state & BTN_RIGHT)) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_SELECT && (s_pad_state & BTN_SELECT)) return 1;
    }
    return 0;
}

static bool environ_cb(unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            enum retro_pixel_format *fmt = (enum retro_pixel_format*)data;
            return (*fmt == RETRO_PIXEL_FORMAT_RGB565);
        }
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *(const char**)data = ".";
            return true;
        default:
            return false;
    }
}

static void dump_ppm(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Cannot write screenshot %s\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", s_w, s_h);
    for (int y = 0; y < s_h; y++) {
        for (int x = 0; x < s_w; x++) {
            uint16_t p = s_frame[y * MAX_W + x];
            uint8_t rgb[3];
            rgb[0] = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
            rgb[1] = (uint8_t)(((p >> 5)  & 0x3F) * 255 / 63);
            rgb[2] = (uint8_t)((p & 0x1F)         * 255 / 31);
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    s_frames_dumped++;
    printf("  [Screenshot] Saved %s (%dx%d)\n", path, s_w, s_h);
}

typedef void (*ddlc_query_fn)(char *out, int cap);
static ddlc_query_fn s_query = NULL;

static int dbg_int(const char *s, const char *key, int def) {
    const char *p = strstr(s, key);
    if (!p) return def;
    p += strlen(key);
    while (*p == ' ') p++;
    return atoi(p);
}

static const char *dbg_str(const char *s, const char *key, const char *def) {
    static char values[4][64];
    static int slot = 0;
    char *value = values[slot];
    slot = (slot + 1) & 3;
    const char *p = strstr(s, key);
    if (!p) return def;
    p += strlen(key);
    const char *end = p;
    while (*end && *end != ' ') end++;
    size_t n = (size_t)(end - p);
    if (n >= 64) n = 63;
    memcpy(value, p, n);
    value[n] = 0;
    return value;
}

static uint32_t autopilot_pad(const char *dbg, int frame, int *last_press) {
    const char *state = dbg_str(dbg, "state=", "");
    int cl = dbg_int(dbg, "cl=", 0);
    int xaload = dbg_int(dbg, "x=", 0);
    int menu = dbg_int(dbg, "menu=", 0);
    int kb = dbg_int(dbg, "kb=", 0);
    int poem = dbg_int(dbg, "poem=", 0);
    int ev = dbg_int(dbg, "ev=", 0);
    const char *mtype = dbg_str(dbg, "mtype=", "");

    int cooldown = 99999;
    if (strcmp(state, "load") == 0 || strcmp(state, "splash") == 0 || strcmp(state, "splash2") == 0) {
        cooldown = 99999;
    } else if (strcmp(state, "title") == 0) {
        cooldown = 90;
    } else if (strcmp(state, "language") == 0) {
        cooldown = 45;
    } else if (strcmp(state, "poemgame") == 0) {
        cooldown = 10;
    } else if (strcmp(state, "poem_special") == 0 || strcmp(state, "credits") == 0) {
        cooldown = 30;
    } else if (strcmp(state, "s_kill_early") == 0 || strcmp(state, "ghostmenu") == 0) {
        cooldown = 99999;
    } else if (strcmp(state, "newgame") == 0 || strcmp(state, "game") == 0) {
        if (cl == 10021 && xaload < 140) {
            cooldown = 99999;
        } else if (kb) {
            cooldown = 40;
        } else if (menu) {
            cooldown = 30;
        } else if (poem) {
            cooldown = 30;
        } else if (ev) {
            cooldown = 45;
        } else {
            cooldown = 18;
        }
    }

    if (frame - *last_press < cooldown) return 0;
    *last_press = frame;

    if (strcmp(state, "poemgame") == 0) {
        static int phase = 0;
        phase++;
        if (phase % 3 == 0) return BTN_RIGHT;
        if (phase % 7 == 0) return BTN_DOWN;
        return BTN_A;
    }
    if (strcmp(state, "title") == 0 && kb) return BTN_A;
    if (kb) return BTN_A;
    if (menu && strcmp(mtype, "language") == 0) return BTN_A;
    if (menu) return BTN_A;
    return BTN_A;
}

typedef struct { int frame; uint32_t pad; } SeqStep;
static SeqStep s_seq[64];
static int s_seq_count = 0;

static uint32_t pad_name(const char *name) {
    if (strcmp(name, "A") == 0) return BTN_A;
    if (strcmp(name, "B") == 0) return BTN_B;
    if (strcmp(name, "X") == 0) return BTN_X;
    if (strcmp(name, "Y") == 0) return BTN_Y;
    if (strcmp(name, "UP") == 0) return BTN_UP;
    if (strcmp(name, "DOWN") == 0) return BTN_DOWN;
    if (strcmp(name, "LEFT") == 0) return BTN_LEFT;
    if (strcmp(name, "RIGHT") == 0) return BTN_RIGHT;
    if (strcmp(name, "START") == 0) return BTN_START;
    if (strcmp(name, "SELECT") == 0) return BTN_SELECT;
    return 0;
}

static void parse_seq(const char *env) {
    if (!env) return;
    char buf[1024];
    strncpy(buf, env, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char *save = NULL;
    char *tok = strtok_r(buf, ",", &save);
    while (tok && s_seq_count < 64) {
        char *colon = strchr(tok, ':');
        if (colon) {
            *colon = 0;
            s_seq[s_seq_count].frame = atoi(tok);
            s_seq[s_seq_count].pad = pad_name(colon + 1);
            if (s_seq[s_seq_count].pad) s_seq_count++;
        }
        tok = strtok_r(NULL, ",", &save);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <core.so> <game-path> [frames] [dump-every] [shot-prefix]\n", argv[0]);
        return 1;
    }

    const char *core_path = argv[1];
    const char *game_path = argv[2];
    int total_frames = (argc > 3) ? atoi(argv[3]) : 3600;
    int dump_every = (argc > 4) ? atoi(argv[4]) : 300;
    const char *prefix = (argc > 5) ? argv[5] : "screenshots/ddlc";
    parse_seq(getenv("DDLC_SEQ"));

    printf("[Runner] Loading core: %s\n", core_path);
    void *handle = dlopen(core_path, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 2;
    }

    void (*retro_init)(void) = (void(*)(void))dlsym(handle, "retro_init");
    void (*retro_deinit)(void) = (void(*)(void))dlsym(handle, "retro_deinit");
    void (*retro_set_environment)(retro_environment_t) = (void(*)(retro_environment_t))dlsym(handle, "retro_set_environment");
    void (*retro_set_video_refresh)(retro_video_refresh_t) = (void(*)(retro_video_refresh_t))dlsym(handle, "retro_set_video_refresh");
    void (*retro_set_audio_sample)(retro_audio_sample_t) = (void(*)(retro_audio_sample_t))dlsym(handle, "retro_set_audio_sample");
    void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t) = (void(*)(retro_audio_sample_batch_t))dlsym(handle, "retro_set_audio_sample_batch");
    void (*retro_set_input_poll)(retro_input_poll_t) = (void(*)(retro_input_poll_t))dlsym(handle, "retro_set_input_poll");
    void (*retro_set_input_state)(retro_input_state_t) = (void(*)(retro_input_state_t))dlsym(handle, "retro_set_input_state");
    bool (*retro_load_game)(const struct retro_game_info*) = (bool(*)(const struct retro_game_info*))dlsym(handle, "retro_load_game");
    void (*retro_run)(void) = (void(*)(void))dlsym(handle, "retro_run");
    s_query = (ddlc_query_fn)dlsym(handle, "ddlc_debug_query");

    if (!retro_init || !retro_load_game || !retro_run) {
        fprintf(stderr, "Missing essential retro_* symbols in %s\n", core_path);
        dlclose(handle);
        return 3;
    }

    retro_set_environment(environ_cb);
    retro_set_video_refresh(video_refresh_cb);
    retro_set_audio_sample(audio_sample_cb);
    retro_set_audio_sample_batch(audio_batch_cb);
    retro_set_input_poll(input_poll_cb);
    retro_set_input_state(input_state_cb);

    retro_init();

    struct retro_game_info game;
    memset(&game, 0, sizeof(game));
    game.path = game_path;

    printf("[Runner] Loading game from: %s\n", game_path);
    if (!retro_load_game(&game)) {
        fprintf(stderr, "[Runner Error] retro_load_game failed!\n");
        retro_deinit();
        dlclose(handle);
        return 4;
    }

    printf("[Runner] Running %d frames...\n", total_frames);
    mkdir("screenshots", 0755);
    const char *wav_path = getenv("DDLC_WAV");
    if (wav_path) {
        s_wav = fopen(wav_path, "wb");
        if (s_wav) wav_header(s_wav, 22050);
    }

    int last_press = -1000;
    char prev_state[64] = "";
    for (int frame = 0; frame < total_frames; frame++) {
        char dbg[256] = "state=none";
        if (s_query) s_query(dbg, sizeof(dbg));
        const char *state = dbg_str(dbg, "state=", "");
        if (s_seq_count > 0) {
            s_pad_state = 0;
            for (int i = 0; i < s_seq_count; i++) {
                if (frame >= s_seq[i].frame && frame <= s_seq[i].frame + 3) {
                    s_pad_state = s_seq[i].pad;
                    break;
                }
            }
        } else {
            s_pad_state = autopilot_pad(dbg, frame, &last_press);
        }
        const char *freeze_env = getenv("DDLC_FREEZE");
        const char *y_env = getenv("DDLC_Y_AT");
        if (y_env) {
            int yat = atoi(y_env);
            if (frame >= yat && frame <= yat + 3) s_pad_state = BTN_Y;
        }
        if (freeze_env && frame >= atoi(freeze_env)) s_pad_state = 0;
        if (s_pad_state) printf("[RUN] frame %d pad %u\n", frame, s_pad_state);
        retro_run();

        if (s_query) s_query(dbg, sizeof(dbg));
        const char *new_state = dbg_str(dbg, "state=", "");
        int state_changed = strcmp(new_state, prev_state) != 0;
        strncpy(prev_state, new_state, sizeof(prev_state) - 1);
        prev_state[sizeof(prev_state) - 1] = 0;

        if (frame % 60 == 0 || state_changed) {
            printf("[AUTO] f=%d %s\n", frame, dbg);
        }

        if (dump_every > 0 && ((frame % dump_every == 0) || state_changed || frame == total_frames - 1)) {
            char path[256];
            snprintf(path, sizeof(path), "%s_%05d.ppm", prefix, frame);
            dump_ppm(path);
        }
    }

    printf("[Runner] Execution finished successfully. Total dumped frames: %d, non-zero audio samples: %ld\n", s_frames_dumped, s_audio_nonzero);
    if (s_wav) {
        long size = ftell(s_wav) - 44;
        fseek(s_wav, 4, SEEK_SET);
        unsigned int riff = (unsigned int)(size + 36);
        fwrite(&riff, 4, 1, s_wav);
        fseek(s_wav, 40, SEEK_SET);
        unsigned int ds = (unsigned int)size;
        fwrite(&ds, 4, 1, s_wav);
        fclose(s_wav);
        s_wav = NULL;
    }
    retro_deinit();
    dlclose(handle);
    return 0;
}
