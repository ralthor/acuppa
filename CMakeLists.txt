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

set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0af55ccecd98d4e5a8d1fad7de25ba429d60e863)
FetchContent_MakeAvailable(glm)

find_package(glfw3 3.3 CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(THREEPOINT_GLSLANG_VALIDATOR glslangValidator REQUIRED)
find_library(THREEPOINT_VULKAN_LOADER NAMES vulkan REQUIRED)

set(THREEPOINT_SHADER_HEADERS)
foreach(stage IN ITEMS vert frag)
  set(shader_source "${PROJECT_SOURCE_DIR}/src/render/shaders/cube.${stage}")
  set(shader_binary "${PROJECT_BINARY_DIR}/shaders/cube.${stage}.spv")
  set(shader_header "${PROJECT_BINARY_DIR}/generated/cube_${stage}_spv.hpp")
  add_custom_command(OUTPUT "${shader_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/shaders" "${PROJECT_BINARY_DIR}/generated"
    COMMAND "${THREEPOINT_GLSLANG_VALIDATOR}" -V "${shader_source}" -o "${shader_binary}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/embed_spirv.py"
            "${shader_binary}" "${shader_header}" "cube_${stage}_spv"
    DEPENDS "${shader_source}" "${PROJECT_SOURCE_DIR}/scripts/embed_spirv.py"
    VERBATIM)
  list(APPEND THREEPOINT_SHADER_HEADERS "${shader_header}")
endforeach()

add_library(threepoint_core STATIC src/core/project.cpp src/core/camera.cpp)
target_compile_features(threepoint_core PUBLIC cxx_std_17)
target_include_directories(threepoint_core PUBLIC include)
target_link_libraries(threepoint_core PUBLIC glm::glm)
target_compile_definitions(threepoint_core PUBLIC THREEPOINT_VERSION="${PROJECT_VERSION}")

add_executable(threepoint src/runtime/main.cpp)
add_library(threepoint_render STATIC src/render/vulkan_window.cpp src/render/cube_demo.cpp
            ${THREEPOINT_SHADER_HEADERS})
target_compile_features(threepoint_render PUBLIC cxx_std_17)
target_include_directories(threepoint_render PUBLIC include PRIVATE "${PROJECT_BINARY_DIR}/generated")
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
  add_executable(threepoint_camera_test tests/camera_test.cpp)
  target_link_libraries(threepoint_camera_test PRIVATE threepoint_core)
  add_test(NAME camera_math COMMAND threepoint_camera_test)
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
    add_test(NAME cube_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/cube_capture_test.py" $<TARGET_FILE:threepoint>)
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
