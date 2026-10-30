#include "love_input.h"
#include <stdio.h>
#include <string.h>

GamepadState LoveInput::s_prev = {false};
GamepadState LoveInput::s_current = {false};
float LoveInput::s_mouseX = 160.0f;
float LoveInput::s_mouseY = 120.0f;
bool LoveInput::s_mouseLeft = false;
bool LoveInput::s_mouseVisible = false;
float LoveInput::s_cursorSpeed = 4.0f;
std::vector<LoveEventMsg> LoveInput::s_eventQueue;

void LoveInput::init() {
    memset(&s_prev, 0, sizeof(s_prev));
    memset(&s_current, 0, sizeof(s_current));
    s_mouseX = 160.0f;
    s_mouseY = 120.0f;
    s_mouseLeft = false;
    s_mouseVisible = false;
    s_cursorSpeed = 4.0f;
    s_eventQueue.clear();
}

void LoveInput::pushEvent(const LoveEventMsg &ev) {
    if (s_eventQueue.size() < 64) {
        s_eventQueue.push_back(ev);
    }
}

bool LoveInput::popEvent(LoveEventMsg *out_ev) {
    if (s_eventQueue.empty()) return false;
    *out_ev = s_eventQueue.front();
    s_eventQueue.erase(s_eventQueue.begin());
    return true;
}

void LoveInput::clearEvents() {
    s_eventQueue.clear();
}

static void trigger_gamepad(lua_State *L, const char *btn, bool pressed) {
    const char *func_name = pressed ? "gamepadpressed" : "gamepadreleased";
    LoveEventMsg ev;
    ev.name = func_name;
    ev.str[0] = btn;
    ev.kind = 5;
    LoveInput::pushEvent(ev);
}

static void trigger_key(lua_State *L, const char *key, bool pressed) {
    const char *func_name = pressed ? "keypressed" : "keyreleased";
    LoveEventMsg ev;
    ev.name = func_name;
    ev.str[0] = key;
    ev.kind = 1;
    LoveInput::pushEvent(ev);
}

static void trigger_mouse_btn(lua_State *L, float x, float y, int btn, bool pressed) {
    const char *func_name = pressed ? "mousepressed" : "mousereleased";
    LoveEventMsg ev;
    ev.name = func_name;
    ev.num[0] = x;
    ev.num[1] = y;
    ev.num[2] = (float)btn;
    ev.kind = 3;
    LoveInput::pushEvent(ev);
}

static void trigger_mouse_move(lua_State *L, float x, float y, float dx, float dy) {
    LoveEventMsg ev;
    ev.name = "mousemoved";
    ev.num[0] = x;
    ev.num[1] = y;
    ev.num[2] = dx;
    ev.num[3] = dy;
    ev.kind = 4;
    LoveInput::pushEvent(ev);
}

void LoveInput::update(lua_State *L, const GamepadState &curr) {
    // Check D-Pad
    if (curr.up != s_prev.up) {
        trigger_gamepad(L, "dpup", curr.up);
    }
    if (curr.down != s_prev.down) {
        trigger_gamepad(L, "dpdown", curr.down);
    }
    if (curr.left != s_prev.left) {
        trigger_gamepad(L, "dpleft", curr.left);
    }
    if (curr.right != s_prev.right) {
        trigger_gamepad(L, "dpright", curr.right);
    }

    // Action buttons
    if (curr.a != s_prev.a) {
        trigger_gamepad(L, "a", curr.a);
    }
    if (curr.b != s_prev.b) {
        trigger_gamepad(L, "b", curr.b);
    }
    if (curr.x != s_prev.x) {
        trigger_gamepad(L, "x", curr.x);
    }
    if (curr.y != s_prev.y) {
        trigger_gamepad(L, "y", curr.y);
    }

    // Shoulders
    if (curr.l != s_prev.l) {
        trigger_gamepad(L, "leftshoulder", curr.l);
    }
    if (curr.r != s_prev.r) {
        trigger_gamepad(L, "rightshoulder", curr.r);
    }

    // Start / Select
    if (curr.start != s_prev.start) {
        trigger_gamepad(L, "start", curr.start);
    }
    if (curr.select != s_prev.select) {
        trigger_gamepad(L, "back", curr.select);
    }

    s_prev = curr;
    s_current = curr;
}

bool LoveInput::isDownKey(const char *key) {
    if (!key) return false;
    if (strcmp(key, "up") == 0) return s_current.up;
    if (strcmp(key, "down") == 0) return s_current.down;
    if (strcmp(key, "left") == 0) return s_current.left;
    if (strcmp(key, "right") == 0) return s_current.right;
    if (strcmp(key, "a") == 0) return s_current.a;
    if (strcmp(key, "b") == 0) return s_current.b;
    if (strcmp(key, "x") == 0) return s_current.x;
    if (strcmp(key, "y") == 0) return s_current.y;
    if (strcmp(key, "start") == 0) return s_current.start;
    if (strcmp(key, "back") == 0) return s_current.select;
    return false;
}
