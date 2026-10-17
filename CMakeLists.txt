cmake_minimum_required(VERSION 3.25)
project(ThreePoint LANGUAGES CXX)

find_package(SDL3 3.4 CONFIG REQUIRED)
find_package(Lua 5.4 EXACT REQUIRED)

add_executable(threepoint src/main.cpp)
target_compile_features(threepoint PRIVATE cxx_std_17)
target_include_directories(threepoint PRIVATE "${LUA_INCLUDE_DIR}")
target_link_libraries(threepoint PRIVATE SDL3::SDL3 ${LUA_LIBRARIES})
