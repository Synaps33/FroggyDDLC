#ifndef LOVE_LUA_H
#define LOVE_LUA_H

#include "love_common.h"

class LoveLua {
public:
    static bool init(const char *game_path);
    static void deinit();
    static bool step();

    static lua_State *getState() { return s_L; }

private:
    static lua_State *s_L;
    static int s_runFuncRef;

    static void registerModules(lua_State *L);
    static void registerGraphics(lua_State *L);
    static void registerFilesystem(lua_State *L);
    static void registerAudio(lua_State *L);
    static void registerTimer(lua_State *L);
    static void registerWindow(lua_State *L);
    static void registerMouse(lua_State *L);
    static void registerKeyboard(lua_State *L);
    static void registerJoystick(lua_State *L);
    static void registerMath(lua_State *L);
    static void registerSystem(lua_State *L);
    static void registerData(lua_State *L);
    static void registerThread(lua_State *L);
    static void registerEvent(lua_State *L);
};

#endif // LOVE_LUA_H
