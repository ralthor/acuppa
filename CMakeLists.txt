cmake_minimum_required(VERSION 3.25)
project(ThreePoint VERSION 0.1.0 LANGUAGES CXX)

add_library(threepoint_core STATIC src/core/project.cpp)
target_compile_features(threepoint_core PUBLIC cxx_std_17)
target_include_directories(threepoint_core PUBLIC include)
target_compile_definitions(threepoint_core PUBLIC THREEPOINT_VERSION="${PROJECT_VERSION}")

add_executable(threepoint src/runtime/main.cpp)
target_link_libraries(threepoint PRIVATE threepoint_core)

add_executable(threepoint_editor src/editor/main.cpp)
set_target_properties(threepoint_editor PROPERTIES OUTPUT_NAME threepoint-editor)
target_link_libraries(threepoint_editor PRIVATE threepoint_core)

include(CTest)
if(BUILD_TESTING)
  add_executable(threepoint_core_test tests/core_test.cpp)
  target_link_libraries(threepoint_core_test PRIVATE threepoint_core)
  add_test(NAME core_project_check COMMAND threepoint_core_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME runtime_project_check COMMAND threepoint --check-project
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME editor_project_check COMMAND threepoint_editor --check-project
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
endif()

option(THREEPOINT_BUILD_2D_PROTOTYPE "Build the older SDL3/Lua prototype" OFF)
if(THREEPOINT_BUILD_2D_PROTOTYPE)
  find_package(SDL3 3.4 CONFIG REQUIRED)
  find_package(Lua 5.4 EXACT REQUIRED)
  add_executable(threepoint_2d_prototype src/legacy/main.cpp)
  set_target_properties(threepoint_2d_prototype PROPERTIES OUTPUT_NAME threepoint-2d-prototype)
  target_compile_features(threepoint_2d_prototype PRIVATE cxx_std_17)
  target_include_directories(threepoint_2d_prototype PRIVATE "${LUA_INCLUDE_DIR}")
  target_link_libraries(threepoint_2d_prototype PRIVATE SDL3::SDL3 ${LUA_LIBRARIES})
  if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME legacy_cli COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/cli_test.py" $<TARGET_FILE:threepoint_2d_prototype>)
    add_test(NAME legacy_graphics COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/graphics_test.py" $<TARGET_FILE:threepoint_2d_prototype>)
  endif()
endif()
