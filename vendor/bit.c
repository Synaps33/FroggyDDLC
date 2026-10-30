/*
** LuaBitOp implementation for Lua 5.1
*/
#include <stdint.h>
#include "lua.h"
#include "lauxlib.h"

typedef int32_t SBits;
typedef uint32_t UBits;

static SBits bit_check(lua_State *L, int idx) {
    lua_Number n = luaL_checknumber(L, idx);
    return (SBits)(int64_t)n;
}

static int bit_tobit(lua_State *L) {
    lua_pushnumber(L, (lua_Number)bit_check(L, 1));
    return 1;
}

static int bit_bnot(lua_State *L) {
    lua_pushnumber(L, (lua_Number)(~bit_check(L, 1)));
    return 1;
}

static int bit_band(lua_State *L) {
    int i, n = lua_gettop(L);
    UBits b = (UBits)bit_check(L, 1);
    for (i = 2; i <= n; i++)
        b &= (UBits)bit_check(L, i);
    lua_pushnumber(L, (lua_Number)(SBits)b);
    return 1;
}

static int bit_bor(lua_State *L) {
    int i, n = lua_gettop(L);
    UBits b = (UBits)bit_check(L, 1);
    for (i = 2; i <= n; i++)
        b |= (UBits)bit_check(L, i);
    lua_pushnumber(L, (lua_Number)(SBits)b);
    return 1;
}

static int bit_bxor(lua_State *L) {
    int i, n = lua_gettop(L);
    UBits b = (UBits)bit_check(L, 1);
    for (i = 2; i <= n; i++)
        b ^= (UBits)bit_check(L, i);
    lua_pushnumber(L, (lua_Number)(SBits)b);
    return 1;
}

static int bit_lshift(lua_State *L) {
    UBits b = (UBits)bit_check(L, 1);
    SBits sh = bit_check(L, 2) & 31;
    lua_pushnumber(L, (lua_Number)(SBits)(b << sh));
    return 1;
}

static int bit_rshift(lua_State *L) {
    UBits b = (UBits)bit_check(L, 1);
    SBits sh = bit_check(L, 2) & 31;
    lua_pushnumber(L, (lua_Number)(SBits)(b >> sh));
    return 1;
}

static int bit_arshift(lua_State *L) {
    SBits b = bit_check(L, 1);
    SBits sh = bit_check(L, 2) & 31;
    lua_pushnumber(L, (lua_Number)(b >> sh));
    return 1;
}

static int bit_rol(lua_State *L) {
    UBits b = (UBits)bit_check(L, 1);
    SBits sh = bit_check(L, 2) & 31;
    lua_pushnumber(L, (lua_Number)(SBits)((b << sh) | (b >> (32 - sh))));
    return 1;
}

static int bit_ror(lua_State *L) {
    UBits b = (UBits)bit_check(L, 1);
    SBits sh = bit_check(L, 2) & 31;
    lua_pushnumber(L, (lua_Number)(SBits)((b >> sh) | (b << (32 - sh))));
    return 1;
}

static int bit_bswap(lua_State *L) {
    UBits b = (UBits)bit_check(L, 1);
    b = ((b << 24)) | ((b << 8) & 0x00FF0000) | ((b >> 8) & 0x0000FF00) | ((b >> 24));
    lua_pushnumber(L, (lua_Number)(SBits)b);
    return 1;
}

static int bit_tohex(lua_State *L) {
    UBits b = (UBits)bit_check(L, 1);
    SBits n = lua_isnoneornil(L, 2) ? 8 : bit_check(L, 2);
    const char *hexdigits = "0123456789abcdef";
    char buf[8];
    int i;
    if (n < 0) { n = -n; hexdigits = "0123456789ABCDEF"; }
    if (n > 8) n = 8;
    for (i = (int)n - 1; i >= 0; i--) {
        buf[i] = hexdigits[b & 15];
        b >>= 4;
    }
    lua_pushlstring(L, buf, n);
    return 1;
}

static const luaL_Reg bit_funcs[] = {
    {"tobit",   bit_tobit},
    {"bnot",    bit_bnot},
    {"band",    bit_band},
    {"bor",     bit_bor},
    {"bxor",    bit_bxor},
    {"lshift",  bit_lshift},
    {"rshift",  bit_rshift},
    {"arshift", bit_arshift},
    {"rol",     bit_rol},
    {"ror",     bit_ror},
    {"bswap",   bit_bswap},
    {"tohex",   bit_tohex},
    {NULL, NULL}
};

int luaopen_bit(lua_State *L) {
    luaL_register(L, "bit", bit_funcs);
    return 1;
}
