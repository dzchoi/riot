// A substitute for loadlib.c

#define loadlib_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"


/* ======================== 'require' function ============================= */

// We provide the preload searcher as the only searcher.
static void findloader(lua_State* L, const char* name)
{
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    if ( lua_getfield(L, -1, name) == LUA_TNIL )  // not found?
        luaL_error(L, "module '%s' not found", name);
}

static int ll_require(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    lua_settop(L, 1);               // LOADED table will be at index 2
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_getfield(L, 2, name);       // LOADED[name]
    if ( lua_toboolean(L, -1) )     // is it there?
        return 1;                   // package is already loaded

    // else must load package
    findloader(L, name);
    lua_pushstring(L, name);        // pass name as argument to module loader
    lua_call(L, 1, 1);              // run loader to load module
    if ( !lua_isnil(L, -1) )        // non-nil return?
        lua_setfield(L, 2, name);   // LOADED[name] = returned value

    if ( lua_getfield(L, 2, name) == LUA_TNIL ) {  // module set no value?
        lua_pushboolean(L, 1);      // use true as result
        lua_pushvalue(L, -1);       // extra copy to be returned
        lua_setfield(L, 2, name);   // LOADED[name] = true
    }
    return 1;
}

/* ====================== 'package' module loader =========================== */

static const luaL_Reg pk_funcs[] = {
    // Tables
    { "preload", NULL },
    { "loaded", NULL },
    { NULL, NULL }
};

LUAMOD_API int luaopen_package(lua_State* L)
{
    // Create the 'package' table.
    luaL_newlib(L, pk_funcs);

    // Create the package.loaded table.
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_setfield(L, -2, "loaded");

    // Create the package.preload table.
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_setfield(L, -2, "preload");

    lua_pushglobaltable(L);
    lua_pushvalue(L, -2);  // Set 'package' as an upvalue for ll_require().
    lua_pushcclosure(L, ll_require, 1);  // C closure of ll_require()
    lua_setfield(L, -2, "require");      // Place require() in the global table.
    lua_pop(L, 1);                       // Pop the global table.

    return 1;                            // Return the 'package' table.
}
