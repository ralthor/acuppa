extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("board and test Lua paths required");
    lua_State* lua = luaL_newstate();
    if (!lua) throw std::runtime_error("cannot start Lua for board rules");
    luaL_openlibs(lua);
    lua_pushstring(lua, argv[1]);
    lua_setglobal(lua, "board_file");
    const int result = luaL_dofile(lua, argv[2]);
    const std::string error = result == LUA_OK ? "" : lua_tostring(lua, -1);
    lua_close(lua);
    if (result != LUA_OK) throw std::runtime_error(error);
}
