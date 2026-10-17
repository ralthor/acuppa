cmake_minimum_required(VERSION 3.25)
project(ThreePoint VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(vulkan_headers
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG 015e25c3c91b70eb1a754d36fb14c4ba6ad9b0b9)
FetchContent_MakeAvailable(vulkan_headers)

set(NVRHI_WITH_VULKAN ON CACHE BOOL "" FORCE)
set(NVRHI_WITH_VALIDATION ON CACHE BOOL "" FORCE)
set(NVRHI_FETCH_VULKAN_HEADERS OFF CACHE BOOL "" FORCE)
set(NVRHI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NVRHI_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nvrhi
  GIT_REPOSITORY https://github.com/NVIDIA-RTX/NVRHI.git
  GIT_TAG 6b96fb03e07539f08327aea76c56d55f1de9d906)
FetchContent_MakeAvailable(nvrhi)

find_package(glfw3 3.3 CONFIG REQUIRED)
find_library(THREEPOINT_VULKAN_LOADER NAMES vulkan REQUIRED)

add_library(threepoint_core STATIC src/core/project.cpp)
target_compile_features(threepoint_core PUBLIC cxx_std_17)
target_include_directories(threepoint_core PUBLIC include)
target_compile_definitions(threepoint_core PUBLIC THREEPOINT_VERSION="${PROJECT_VERSION}")

add_executable(threepoint src/runtime/main.cpp)
add_library(threepoint_render STATIC src/render/vulkan_window.cpp)
target_compile_features(threepoint_render PUBLIC cxx_std_17)
target_include_directories(threepoint_render PUBLIC include)
target_link_libraries(threepoint_render PUBLIC threepoint_core glfw nvrhi_vk nvrhi Vulkan::Headers
                      "${THREEPOINT_VULKAN_LOADER}")
target_link_libraries(threepoint PRIVATE threepoint_core threepoint_render)

add_executable(threepoint_editor src/editor/main.cpp)
set_target_properties(threepoint_editor PROPERTIES OUTPUT_NAME threepoint-editor)
target_link_libraries(threepoint_editor PRIVATE threepoint_core)

include(CTest)
option(THREEPOINT_ENABLE_GPU_TESTS "Run live Vulkan display checks" OFF)
if(BUILD_TESTING)
  add_executable(threepoint_core_test tests/core_test.cpp)
  target_link_libraries(threepoint_core_test PRIVATE threepoint_core)
  add_test(NAME core_project_check COMMAND threepoint_core_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME runtime_project_check COMMAND threepoint --check-project
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME editor_project_check COMMAND threepoint_editor --check-project
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  if(THREEPOINT_ENABLE_GPU_TESTS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME vulkan_smoke COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/vulkan_smoke_test.py" $<TARGET_FILE:threepoint>)
  endif()
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
