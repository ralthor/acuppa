#include <SDL3/SDL_version.h>

extern "C" {
#include <lauxlib.h>
}

#include <iostream>

int main() {
  lua_State* lua = luaL_newstate();
  if (!lua) {
    std::cerr << "ThreePoint: could not initialize Lua\n";
    return 1;
  }

  const int sdl = SDL_GetVersion();
  std::cout << "ThreePoint bootstrap: SDL " << SDL_VERSIONNUM_MAJOR(sdl) << '.'
            << SDL_VERSIONNUM_MINOR(sdl) << '.' << SDL_VERSIONNUM_MICRO(sdl)
            << ", " << LUA_VERSION << '\n';
  lua_close(lua);
}
