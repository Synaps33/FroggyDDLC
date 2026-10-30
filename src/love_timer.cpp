#include "love_timer.h"
#include <time.h>
#include <unistd.h>

#ifdef SF2000
extern "C" uint32_t os_get_tick_count(void);
extern "C" void dly_tsk(uint32_t ms);
#endif

uint64_t LoveTimer::s_startTime = 0;
uint64_t LoveTimer::s_lastTime = 0;
double LoveTimer::s_delta = 1.0 / 60.0;
int LoveTimer::s_fps = 60;
int LoveTimer::s_frames = 0;
double LoveTimer::s_fpsTimer = 0.0;

static uint64_t get_time_ns() {
#ifdef SF2000
    return (uint64_t)os_get_tick_count() * 1000000ULL; // ms to ns
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

void LoveTimer::init() {
    s_startTime = get_time_ns();
    s_lastTime = s_startTime;
    s_delta = 1.0 / 60.0;
    s_fps = 60;
    s_frames = 0;
    s_fpsTimer = 0.0;
}

double LoveTimer::getTime() {
    uint64_t now = get_time_ns();
    return (double)(now - s_startTime) / 1000000000.0;
}

double LoveTimer::step() {
    uint64_t now = get_time_ns();
    uint64_t diff = now - s_lastTime;
    s_lastTime = now;

    s_delta = (double)diff / 1000000000.0;
    if (s_delta < 1.0 / 60.0) s_delta = 1.0 / 60.0;
    if (s_delta > 0.05) s_delta = 0.05; // clamp delta to prevent giant leaps

    s_frames++;
    s_fpsTimer += s_delta;
    if (s_fpsTimer >= 1.0) {
        s_fps = (int)(s_frames / s_fpsTimer);
        s_frames = 0;
        s_fpsTimer = 0.0;
    }

    return s_delta;
}

void LoveTimer::sleep(double seconds) {
    if (seconds <= 0.0) return;
#ifdef SF2000
    // Libretro frontend throttles execution; dly_tsk causes major stutter/FPS drop
    return;
#else
    usleep((useconds_t)(seconds * 1000000.0));
#endif
}
