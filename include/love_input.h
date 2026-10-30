#ifndef LOVE_INPUT_H
#define LOVE_INPUT_H

#include "love_common.h"
#include <string>
#include <vector>

struct GamepadState {
    bool up, down, left, right;
    bool a, b, x, y;
    bool l, r;
    bool start, select;
};

struct LoveEventMsg {
    std::string name;
    float num[4];
    std::string str[2];
    int kind; // 0: no args, 1: str[0], 2: num[0..1], 3: num[0..2] (mouse btn), 4: num[0..3] (mouse move), 5: joystick+btn
};

class LoveInput {
public:
    static void init();
    static void update(lua_State *L, const GamepadState &current);

    static float getMouseX() { return s_mouseX; }
    static float getMouseY() { return s_mouseY; }
    static bool isMouseDown(int button) { return (button == 1) ? s_mouseLeft : false; }
    static void setMouseVisible(bool vis) { s_mouseVisible = vis; }
    static bool isMouseVisible() { return s_mouseVisible; }

    static void setCursorSpeed(float spd) { s_cursorSpeed = spd; }
    static bool isDownKey(const char *key);

    static void pushEvent(const LoveEventMsg &ev);
    static bool popEvent(LoveEventMsg *out_ev);
    static void clearEvents();

private:
    static GamepadState s_prev;
    static GamepadState s_current;
    static float s_mouseX;
    static float s_mouseY;
    static bool s_mouseLeft;
    static bool s_mouseVisible;
    static float s_cursorSpeed;
    static std::vector<LoveEventMsg> s_eventQueue;
};

#endif // LOVE_INPUT_H
