cmake_minimum_required(VERSION 3.25)
project(ACuppa VERSION 0.1.0 LANGUAGES C CXX)

option(ACUPPA_ENABLE_SANITIZERS "Instrument ACuppa and its tests with ASan and UBSan" OFF)
if(ACUPPA_ENABLE_SANITIZERS)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "ACuppa sanitizers require a Linux Clang or GCC compiler")
  endif()
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

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

set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(USE_SSE4_1 OFF CACHE BOOL "" FORCE)
set(USE_SSE4_2 OFF CACHE BOOL "" FORCE)
set(USE_AVX OFF CACHE BOOL "" FORCE)
set(USE_AVX2 OFF CACHE BOOL "" FORCE)
set(USE_LZCNT OFF CACHE BOOL "" FORCE)
set(USE_TZCNT OFF CACHE BOOL "" FORCE)
set(USE_F16C OFF CACHE BOOL "" FORCE)
set(USE_FMADD OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jolt
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG e77f175595e64cb44218cc9d9d56fc365ad0e36a
  SOURCE_SUBDIR Build)
FetchContent_MakeAvailable(jolt)

FetchContent_Declare(cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG 360db1a95480fe102ae9c69b27c5d101167ff5ba)
FetchContent_MakeAvailable(cgltf)

FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20)
FetchContent_MakeAvailable(stb)

set(MINIAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_EXTRA_NODES ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_LIBVORBIS ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_LIBOPUS ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_ENCODING ON CACHE BOOL "" FORCE)
set(MINIAUDIO_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG 9634bedb5b5a2ca38c1ee7108a9358a4e233f14d)
FetchContent_MakeAvailable(miniaudio)

FetchContent_Declare(lua
  URL https://www.lua.org/ftp/lua-5.4.8.tar.gz
  URL_HASH SHA256=4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae)
FetchContent_MakeAvailable(lua)
file(GLOB ACUPPA_LUA_SOURCES CONFIGURE_DEPENDS "${lua_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM ACUPPA_LUA_SOURCES
  "${lua_SOURCE_DIR}/src/lua.c" "${lua_SOURCE_DIR}/src/luac.c")
add_library(acuppa_lua STATIC ${ACUPPA_LUA_SOURCES})
target_include_directories(acuppa_lua PUBLIC "${lua_SOURCE_DIR}/src")
target_compile_features(acuppa_lua PUBLIC c_std_99)
target_compile_definitions(acuppa_lua PRIVATE LUA_USE_LINUX)
target_link_libraries(acuppa_lua PUBLIC m ${CMAKE_DL_LIBS})

find_package(glfw3 3.3 CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(ACUPPA_GLSLANG_VALIDATOR glslangValidator REQUIRED)
find_library(ACUPPA_VULKAN_LOADER NAMES vulkan REQUIRED)

set(ACUPPA_SHADER_HEADERS)
foreach(shader IN ITEMS cube pbr tonemap shadow_cube shadow_pbr)
  foreach(stage IN ITEMS vert frag)
    set(shader_source "${PROJECT_SOURCE_DIR}/src/render/shaders/${shader}.${stage}")
    set(shader_binary "${PROJECT_BINARY_DIR}/shaders/${shader}.${stage}.spv")
    set(shader_header "${PROJECT_BINARY_DIR}/generated/${shader}_${stage}_spv.hpp")
    add_custom_command(OUTPUT "${shader_header}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/shaders" "${PROJECT_BINARY_DIR}/generated"
      COMMAND "${ACUPPA_GLSLANG_VALIDATOR}" -V "${shader_source}" -o "${shader_binary}"
      COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/embed_spirv.py"
              "${shader_binary}" "${shader_header}" "${shader}_${stage}_spv"
      DEPENDS "${shader_source}" "${PROJECT_SOURCE_DIR}/scripts/embed_spirv.py"
      VERBATIM)
    list(APPEND ACUPPA_SHADER_HEADERS "${shader_header}")
  endforeach()
endforeach()
# Fragment-only passes reuse the fullscreen tonemap vertex shader.
foreach(shader IN ITEMS ssao sky)
  set(shader_source "${PROJECT_SOURCE_DIR}/src/render/shaders/${shader}.frag")
  set(shader_binary "${PROJECT_BINARY_DIR}/shaders/${shader}.frag.spv")
  set(shader_header "${PROJECT_BINARY_DIR}/generated/${shader}_frag_spv.hpp")
  add_custom_command(OUTPUT "${shader_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/shaders" "${PROJECT_BINARY_DIR}/generated"
    COMMAND "${ACUPPA_GLSLANG_VALIDATOR}" -V "${shader_source}" -o "${shader_binary}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/embed_spirv.py"
            "${shader_binary}" "${shader_header}" "${shader}_frag_spv"
    DEPENDS "${shader_source}" "${PROJECT_SOURCE_DIR}/scripts/embed_spirv.py"
    VERBATIM)
  list(APPEND ACUPPA_SHADER_HEADERS "${shader_header}")
endforeach()

add_library(acuppa_core STATIC src/core/project.cpp src/core/scene_document.cpp
            src/core/source_document.cpp
            src/core/audio_world.cpp
            src/core/editor_gizmo.cpp src/core/editor_viewport.cpp
            src/core/camera.cpp src/core/input.cpp
            src/core/ui_canvas.cpp
            src/core/world.cpp src/core/physics_world.cpp src/core/save_data.cpp src/core/data_file.cpp
            src/core/gltf_asset.cpp src/core/cgltf_impl.c src/core/stb_image_impl.c
            src/core/hdr_image.cpp src/core/environment_filter.cpp src/core/lua_project.cpp
            src/core/sky.cpp)
target_compile_features(acuppa_core PUBLIC cxx_std_17)
target_include_directories(acuppa_core PUBLIC include PRIVATE "${cgltf_SOURCE_DIR}"
                           "${stb_SOURCE_DIR}")
target_link_libraries(acuppa_core PUBLIC glm::glm acuppa_lua PRIVATE Jolt miniaudio)
target_compile_definitions(acuppa_core PUBLIC ACUPPA_VERSION="${PROJECT_VERSION}")

add_executable(acuppa src/runtime/main.cpp src/runtime/export_game.cpp)
add_library(acuppa_render STATIC src/render/vulkan_window.cpp src/render/cube_demo.cpp
            ${ACUPPA_SHADER_HEADERS})
target_compile_features(acuppa_render PUBLIC cxx_std_17)
target_include_directories(acuppa_render PUBLIC include PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(acuppa_render PUBLIC acuppa_core glfw nvrhi_vk nvrhi Vulkan::Headers
                      "${ACUPPA_VULKAN_LOADER}")
target_link_libraries(acuppa PRIVATE acuppa_core acuppa_render)
add_executable(acuppa_player src/runtime/player.cpp)
set_target_properties(acuppa_player PROPERTIES OUTPUT_NAME acuppa-player)
target_link_libraries(acuppa_player PRIVATE acuppa_core acuppa_render)
add_dependencies(acuppa acuppa_player)

add_executable(acuppa_editor src/editor/main.cpp)
set_target_properties(acuppa_editor PROPERTIES OUTPUT_NAME acuppa-editor)
target_link_libraries(acuppa_editor PRIVATE acuppa_core acuppa_render)

include(CTest)
option(ACUPPA_ENABLE_GPU_TESTS "Run live Vulkan display checks" OFF)
if(BUILD_TESTING)
  add_test(NAME engine_name_and_tagline COMMAND $<TARGET_FILE:acuppa> --version)
  set_tests_properties(engine_name_and_tagline PROPERTIES
    PASS_REGULAR_EXPRESSION "ACuppa Engine ${PROJECT_VERSION}.*Brew games, one idea at a time[.]")
  add_executable(acuppa_audio_world_test tests/audio_world_test.cpp)
  target_link_libraries(acuppa_audio_world_test PRIVATE acuppa_core)
  add_test(NAME audio_decode_spatial_and_errors COMMAND acuppa_audio_world_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene/assets/contact.wav")
  add_executable(acuppa_audio_scene_test tests/audio_scene_test.cpp)
  target_link_libraries(acuppa_audio_scene_test PRIVATE acuppa_core)
  add_test(NAME lua_audio_contact COMMAND acuppa_audio_scene_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_executable(acuppa_api_contract_scene_test tests/api_contract_scene_test.cpp)
  target_link_libraries(acuppa_api_contract_scene_test PRIVATE acuppa_core)
  add_test(NAME lua_api_contract_scene COMMAND acuppa_api_contract_scene_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME lua_api_coverage_audit COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/api_coverage_audit.py" "${PROJECT_SOURCE_DIR}")
  add_executable(acuppa_editor_viewport_test tests/editor_viewport_test.cpp)
  target_link_libraries(acuppa_editor_viewport_test PRIVATE acuppa_core)
  add_test(NAME editor_viewport_drag COMMAND acuppa_editor_viewport_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/editor_viewport_fixture")
  add_executable(acuppa_editor_gizmo_test tests/editor_gizmo_test.cpp)
  target_link_libraries(acuppa_editor_gizmo_test PRIVATE acuppa_core)
  add_test(NAME editor_gizmo_math COMMAND acuppa_editor_gizmo_test)
  add_executable(acuppa_scene_document_test tests/scene_document_test.cpp)
  target_link_libraries(acuppa_scene_document_test PRIVATE acuppa_core)
  add_test(NAME scene_document_roundtrip COMMAND acuppa_scene_document_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/scene_document_fixture")
  add_test(NAME editor_source_commands COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/editor_cli_test.py" $<TARGET_FILE:acuppa_editor>
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/editor_cli_fixture")
  add_executable(acuppa_source_document_test tests/source_document_test.cpp)
  target_link_libraries(acuppa_source_document_test PRIVATE acuppa_core)
  add_test(NAME editor_raw_source_roundtrip COMMAND acuppa_source_document_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/source_document_fixture")
  add_test(NAME editor_raw_source_commands COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/editor_source_cli_test.py"
           $<TARGET_FILE:acuppa_editor>
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/editor_source_cli_fixture")
  add_executable(acuppa_ui_menu_test tests/ui_menu_test.cpp)
  target_link_libraries(acuppa_ui_menu_test PRIVATE acuppa_core)
  add_test(NAME source_ui_menu_and_save COMMAND acuppa_ui_menu_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/ui_menu_fixture")
  add_executable(acuppa_save_data_test tests/save_data_test.cpp)
  target_link_libraries(acuppa_save_data_test PRIVATE acuppa_core)
  add_test(NAME save_file_integrity COMMAND acuppa_save_data_test
           "${PROJECT_BINARY_DIR}/save_data_fixture")
  add_executable(acuppa_save_lua_test tests/save_lua_test.cpp)
  target_link_libraries(acuppa_save_lua_test PRIVATE acuppa_core)
  add_test(NAME lua_save_roundtrip COMMAND acuppa_save_lua_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/save_lua_fixture")
  add_executable(acuppa_data_file_test tests/data_file_test.cpp)
  target_link_libraries(acuppa_data_file_test PRIVATE acuppa_core)
  add_test(NAME data_file_integrity COMMAND acuppa_data_file_test
           "${PROJECT_BINARY_DIR}/data_file_fixture")
  add_executable(acuppa_data_files_lua_test tests/data_files_lua_test.cpp)
  target_link_libraries(acuppa_data_files_lua_test PRIVATE acuppa_core)
  add_test(NAME lua_data_files COMMAND acuppa_data_files_lua_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/data_files_lua_fixture")
  add_executable(acuppa_physics_test tests/feature_scene/physics_core_test.cpp)
  target_link_libraries(acuppa_physics_test PRIVATE acuppa_core)
  add_test(NAME physics_core COMMAND acuppa_physics_test)
  add_executable(acuppa_vehicle_scene_test tests/vehicle_scene_test.cpp)
  target_link_libraries(acuppa_vehicle_scene_test PRIVATE acuppa_core)
  add_test(NAME lua_vehicle_scene COMMAND acuppa_vehicle_scene_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_executable(acuppa_physics_materials_scene_test tests/physics_materials_scene_test.cpp)
  target_link_libraries(acuppa_physics_materials_scene_test PRIVATE acuppa_core)
  add_test(NAME lua_physics_materials_scene COMMAND acuppa_physics_materials_scene_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_executable(acuppa_core_test tests/core_test.cpp)
  target_link_libraries(acuppa_core_test PRIVATE acuppa_core)
  add_executable(acuppa_camera_test tests/camera_test.cpp)
  target_link_libraries(acuppa_camera_test PRIVATE acuppa_core)
  add_test(NAME camera_math COMMAND acuppa_camera_test)
  add_executable(acuppa_sky_test tests/sky_test.cpp)
  target_link_libraries(acuppa_sky_test PRIVATE acuppa_core)
  add_test(NAME sky_gradient_math COMMAND acuppa_sky_test)
  add_executable(acuppa_lua_test tests/lua_project_test.cpp)
  target_link_libraries(acuppa_lua_test PRIVATE acuppa_core)
  add_test(NAME lua_scene_logic COMMAND acuppa_lua_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME lua_component_errors COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/components_scene_test.py" $<TARGET_FILE:acuppa>
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_executable(acuppa_input_test tests/input_test.cpp)
  target_link_libraries(acuppa_input_test PRIVATE acuppa_core)
  add_test(NAME input_actions_and_ray COMMAND acuppa_input_test)
  add_executable(acuppa_world_test tests/world_test.cpp)
  target_link_libraries(acuppa_world_test PRIVATE acuppa_core)
  add_test(NAME world_components COMMAND acuppa_world_test)
  add_executable(acuppa_scene_flow_test tests/scene_flow_test.cpp)
  target_link_libraries(acuppa_scene_flow_test PRIVATE acuppa_core)
  add_test(NAME source_scene_and_prefabs COMMAND acuppa_scene_flow_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/scene_flow_fixture")
  add_executable(acuppa_gltf_test tests/gltf_asset_test.cpp)
  target_link_libraries(acuppa_gltf_test PRIVATE acuppa_core)
  add_test(NAME gltf_geometry_and_errors COMMAND acuppa_gltf_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/gltf_fixture")
  add_executable(acuppa_hdr_image_test tests/hdr_image_test.cpp)
  target_link_libraries(acuppa_hdr_image_test PRIVATE acuppa_core)
  add_test(NAME hdr_image_validation COMMAND acuppa_hdr_image_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/hdr_image_fixture")
  add_executable(acuppa_environment_filter_test tests/environment_filter_test.cpp)
  target_link_libraries(acuppa_environment_filter_test PRIVATE acuppa_core)
  add_test(NAME environment_prefilter COMMAND acuppa_environment_filter_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene/assets/test_gradient.hdr")
  add_executable(acuppa_shadow_layout_test tests/shadow_layout_test.cpp)
  target_include_directories(acuppa_shadow_layout_test PRIVATE src/render)
  target_link_libraries(acuppa_shadow_layout_test PRIVATE acuppa_core)
  add_test(NAME shadow_atlas_packing COMMAND acuppa_shadow_layout_test)
  add_executable(acuppa_falling_blocks_rules_test tests/falling_blocks_rules_test.cpp)
  target_link_libraries(acuppa_falling_blocks_rules_test PRIVATE acuppa_lua)
  add_test(NAME falling_blocks_rules COMMAND acuppa_falling_blocks_rules_test
           "${PROJECT_SOURCE_DIR}/games/falling_blocks/scripts/board.lua"
           "${PROJECT_SOURCE_DIR}/tests/falling_blocks_rules.lua")
  add_test(NAME falling_blocks_lit_clear_rows COMMAND acuppa_falling_blocks_rules_test
           "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit/scripts/board.lua"
           "${PROJECT_SOURCE_DIR}/tests/falling_blocks_lit_board.lua")
  add_test(NAME falling_blocks_lit_feedback_tiers COMMAND acuppa_falling_blocks_rules_test
           "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit/scripts/clear_feedback.lua"
           "${PROJECT_SOURCE_DIR}/tests/falling_blocks_lit_feedback.lua")
  add_test(NAME falling_blocks_lit_feedback_game COMMAND acuppa_falling_blocks_rules_test
           "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit"
           "${PROJECT_SOURCE_DIR}/tests/falling_blocks_lit_feedback_game.lua")
  add_executable(acuppa_garage_run_rules_test tests/garage_run_rules_test.cpp)
  target_link_libraries(acuppa_garage_run_rules_test PRIVATE acuppa_lua)
  add_test(NAME garage_run_driving_rules COMMAND acuppa_garage_run_rules_test
           "${PROJECT_SOURCE_DIR}/games/garage_run/scripts/driving.lua"
           "${PROJECT_SOURCE_DIR}/tests/garage_run_rules.lua")
  add_executable(acuppa_siege_sling_rules_test tests/siege_sling_rules_test.cpp)
  target_link_libraries(acuppa_siege_sling_rules_test PRIVATE acuppa_lua)
  add_test(NAME siege_sling_rules COMMAND acuppa_siege_sling_rules_test
           "${PROJECT_SOURCE_DIR}/games/siege_sling/scripts/rules.lua"
           "${PROJECT_SOURCE_DIR}/tests/siege_sling_rules.lua")
  add_test(NAME siege_sling_play_modules COMMAND acuppa_siege_sling_rules_test
           "${PROJECT_SOURCE_DIR}/games/siege_sling/scripts"
           "${PROJECT_SOURCE_DIR}/tests/siege_sling_play_modules.lua")
  add_executable(acuppa_siege_sling_game_test tests/siege_sling_game_test.cpp)
  target_link_libraries(acuppa_siege_sling_game_test PRIVATE acuppa_core)
  add_test(NAME siege_sling_gameplay COMMAND acuppa_siege_sling_game_test
           "${PROJECT_SOURCE_DIR}/games/siege_sling"
           "${PROJECT_BINARY_DIR}/siege_sling_game_fixture")
  add_test(NAME siege_sling_assets COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/siege_sling_assets_test.py"
           "${PROJECT_SOURCE_DIR}/games/siege_sling"
           "${PROJECT_BINARY_DIR}/siege_sling_assets_fixture")
  add_test(NAME siege_sling_export COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/siege_sling_export_test.py"
           $<TARGET_FILE:acuppa>
           "${PROJECT_SOURCE_DIR}/games/siege_sling"
           $<TARGET_FILE:acuppa_siege_sling_game_test>
           "${PROJECT_BINARY_DIR}/siege_sling_export_fixture")
  add_test(NAME siege_sling_project_test COMMAND acuppa test
           "${PROJECT_SOURCE_DIR}/games/siege_sling")
  set_tests_properties(siege_sling_project_test PROPERTIES
                       PASS_REGULAR_EXPRESSION "Tested 3 source scenes")
  add_executable(acuppa_garage_run_impact_test tests/garage_run_impact_test.cpp)
  target_link_libraries(acuppa_garage_run_impact_test PRIVATE acuppa_lua)
  add_test(NAME garage_run_impact_camera COMMAND acuppa_garage_run_impact_test
           "${PROJECT_SOURCE_DIR}/games/garage_run/scripts/impact.lua"
           "${PROJECT_SOURCE_DIR}/tests/garage_run_impact.lua")
  add_executable(acuppa_garage_run_game_test tests/garage_run_game_test.cpp)
  target_link_libraries(acuppa_garage_run_game_test PRIVATE acuppa_core)
  add_test(NAME garage_run_gameplay COMMAND acuppa_garage_run_game_test
           "${PROJECT_SOURCE_DIR}/games/garage_run"
           "${PROJECT_BINARY_DIR}/garage_run_game_fixture")
  add_executable(acuppa_garage_run_trajectory_test tests/garage_run_trajectory_test.cpp)
  target_link_libraries(acuppa_garage_run_trajectory_test PRIVATE acuppa_core)
  add_test(NAME garage_run_headless_trajectory COMMAND acuppa_garage_run_trajectory_test
           "${PROJECT_SOURCE_DIR}/games/garage_run"
           "${PROJECT_BINARY_DIR}/garage_run_trajectory_fixture")
  add_test(NAME garage_run_assets COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/garage_run_assets_test.py"
           "${PROJECT_SOURCE_DIR}/games/garage_run"
           "${PROJECT_BINARY_DIR}/garage_run_assets_fixture")
  add_test(NAME garage_run_export COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/garage_run_export_test.py"
           $<TARGET_FILE:acuppa>
           "${PROJECT_SOURCE_DIR}/games/garage_run"
           $<TARGET_FILE:acuppa_garage_run_game_test>
           "${PROJECT_BINARY_DIR}/garage_run_export_fixture")
  add_executable(acuppa_falling_blocks_game_test tests/falling_blocks_game_test.cpp)
  target_link_libraries(acuppa_falling_blocks_game_test PRIVATE acuppa_core)
  add_test(NAME falling_blocks_game_logic COMMAND acuppa_falling_blocks_game_test
           "${PROJECT_SOURCE_DIR}/games/falling_blocks")
  add_executable(acuppa_falling_blocks_lit_game_test tests/falling_blocks_lit_game_test.cpp)
  target_link_libraries(acuppa_falling_blocks_lit_game_test PRIVATE acuppa_core)
  add_test(NAME falling_blocks_lit_game_logic COMMAND acuppa_falling_blocks_lit_game_test
           "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
  add_test(NAME falling_blocks_export COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/falling_blocks_export_test.py"
           $<TARGET_FILE:acuppa>
           "${PROJECT_SOURCE_DIR}/games/falling_blocks"
           "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit"
           "${PROJECT_BINARY_DIR}/falling_blocks_export_fixture")
  add_test(NAME core_project_check COMMAND acuppa_core_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME runtime_project_check COMMAND acuppa --check-project
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME headless_project_test COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/project_test_cli.py" $<TARGET_FILE:acuppa>
           "${PROJECT_SOURCE_DIR}/tests/project_test_fixture"
           "${PROJECT_BINARY_DIR}/headless_project_fixture")
  add_test(NAME standalone_export_files COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/export_game_test.py" $<TARGET_FILE:acuppa>
           "${PROJECT_SOURCE_DIR}/tests/project_test_fixture"
           "${PROJECT_BINARY_DIR}/export_files_fixture")
  if(NOT ACUPPA_ENABLE_SANITIZERS)
    add_test(NAME linux_release_archives COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/package_linux_release_test.py"
             "${PROJECT_SOURCE_DIR}/scripts/package-linux-release.sh"
             $<TARGET_FILE:acuppa>
             "${PROJECT_BINARY_DIR}/release_archive_fixture")
  endif()
  add_test(NAME project_asset_failure_matrix COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/asset_failure_matrix.py" $<TARGET_FILE:acuppa>
           "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit"
           "${PROJECT_SOURCE_DIR}/games/garage_run"
           "${PROJECT_BINARY_DIR}/asset_failure_fixture")
  add_test(NAME editor_project_check COMMAND acuppa_editor --check-project
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  if(ACUPPA_ENABLE_GPU_TESTS)
    add_test(NAME standalone_export_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/export_player_capture_test.py"
             $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/project_test_fixture"
             "${PROJECT_BINARY_DIR}/export_capture_fixture")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME vulkan_smoke COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/vulkan_smoke_test.py" $<TARGET_FILE:acuppa>)
    add_test(NAME cube_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/cube_capture_test.py" $<TARGET_FILE:acuppa>)
    add_test(NAME lua_scene_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_scene_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME lua_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME falling_blocks_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks")
    add_test(NAME falling_blocks_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks")
    add_test(NAME falling_blocks_lit_assets COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_lit_assets_test.py"
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
    add_test(NAME falling_blocks_lit_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
    add_test(NAME falling_blocks_lit_ssao COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_lit_ssao_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
    add_test(NAME falling_blocks_lit_clear_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_lit_clear_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
    add_test(NAME falling_blocks_lit_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
    add_test(NAME falling_blocks_export_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_export_test.py"
             $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks"
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit"
             "${PROJECT_BINARY_DIR}/falling_blocks_export_capture" --graphical)
    add_test(NAME siege_sling_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/siege_sling_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/siege_sling" "${PROJECT_BINARY_DIR}/siege_sling_capture")
    add_test(NAME siege_sling_export_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/siege_sling_export_test.py"
             $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/siege_sling"
             $<TARGET_FILE:acuppa_siege_sling_game_test>
             "${PROJECT_BINARY_DIR}/siege_sling_export_capture" --graphical)
    add_test(NAME siege_sling_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/siege_sling")
    add_test(NAME siege_sling_editor_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/siege_sling" "editor")
    add_test(NAME garage_run_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/garage_run_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/garage_run" "${PROJECT_BINARY_DIR}/garage_run_capture")
    add_test(NAME garage_run_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/garage_run")
    add_test(NAME garage_run_export_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/garage_run_export_test.py"
             $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/games/garage_run"
             $<TARGET_FILE:acuppa_garage_run_game_test>
             "${PROJECT_BINARY_DIR}/garage_run_export_capture" --graphical)
    add_test(NAME input_scene_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/input_scene_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME gltf_scene_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/gltf_scene_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME pbr_material_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/pbr_material_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME hdr_exposure_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/hdr_exposure_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME hdr_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "hdr")
    add_test(NAME ibl_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/ibl_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME ibl_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "ibl")
    add_test(NAME shadow_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/shadow_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME ssao_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/ssao_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME ssao_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "ssao")
    add_test(NAME physics_scene_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/physics_scene_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME physics_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "physics")
    add_test(NAME physics_materials_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "physics_materials")
    add_test(NAME sky_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/sky_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME sky_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "sky")
    add_test(NAME views_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/views_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME views_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "views")
    add_test(NAME audio_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "audio")
    add_test(NAME ui_menu_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/ui_capture_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "${PROJECT_BINARY_DIR}")
    add_test(NAME ui_menu_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "menu")
    add_test(NAME editor_viewport_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/editor_viewport_capture_test.py"
             $<TARGET_FILE:acuppa_editor> "${PROJECT_SOURCE_DIR}/tests/feature_scene"
             "${PROJECT_BINARY_DIR}")
    add_test(NAME editor_viewport_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/editor_viewport_live_test.py"
             $<TARGET_FILE:acuppa_editor> "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME shadow_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "shadow")
    add_test(NAME point_shadow_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:acuppa>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "shadow_point")
  endif()
endif()

option(ACUPPA_BUILD_2D_PROTOTYPE "Build the older SDL3/Lua prototype" OFF)
if(ACUPPA_BUILD_2D_PROTOTYPE)
  find_package(SDL3 3.4 CONFIG REQUIRED)
  find_package(Lua 5.4 EXACT REQUIRED)
  add_executable(acuppa_2d_prototype src/legacy/main.cpp)
  set_target_properties(acuppa_2d_prototype PROPERTIES OUTPUT_NAME acuppa-2d-prototype)
  target_compile_features(acuppa_2d_prototype PRIVATE cxx_std_17)
  target_include_directories(acuppa_2d_prototype PRIVATE "${LUA_INCLUDE_DIR}")
  target_link_libraries(acuppa_2d_prototype PRIVATE SDL3::SDL3 ${LUA_LIBRARIES})
  if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME legacy_cli COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/cli_test.py" $<TARGET_FILE:acuppa_2d_prototype>)
    add_test(NAME legacy_graphics COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/graphics_test.py" $<TARGET_FILE:acuppa_2d_prototype>)
  endif()
endif()
