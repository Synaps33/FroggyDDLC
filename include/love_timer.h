#ifndef LOVE_TIMER_H
#define LOVE_TIMER_H

#include <stdint.h>

class LoveTimer {
public:
    static void init();
    static double getTime();
    static double step();
    static double getDelta() { return s_delta; }
    static int getFPS() { return s_fps; }
    static void sleep(double seconds);

private:
    static uint64_t s_startTime;
    static uint64_t s_lastTime;
    static double s_delta;
    static int s_fps;
    static int s_frames;
    static double s_fpsTimer;
};

#endif // LOVE_TIMER_H
