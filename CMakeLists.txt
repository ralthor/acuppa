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
file(GLOB THREEPOINT_LUA_SOURCES CONFIGURE_DEPENDS "${lua_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM THREEPOINT_LUA_SOURCES
  "${lua_SOURCE_DIR}/src/lua.c" "${lua_SOURCE_DIR}/src/luac.c")
add_library(threepoint_lua STATIC ${THREEPOINT_LUA_SOURCES})
target_include_directories(threepoint_lua PUBLIC "${lua_SOURCE_DIR}/src")
target_compile_features(threepoint_lua PUBLIC c_std_99)
target_compile_definitions(threepoint_lua PRIVATE LUA_USE_LINUX)
target_link_libraries(threepoint_lua PUBLIC m ${CMAKE_DL_LIBS})

find_package(glfw3 3.3 CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(THREEPOINT_GLSLANG_VALIDATOR glslangValidator REQUIRED)
find_library(THREEPOINT_VULKAN_LOADER NAMES vulkan REQUIRED)

set(THREEPOINT_SHADER_HEADERS)
foreach(shader IN ITEMS cube pbr tonemap shadow_cube shadow_pbr)
  foreach(stage IN ITEMS vert frag)
    set(shader_source "${PROJECT_SOURCE_DIR}/src/render/shaders/${shader}.${stage}")
    set(shader_binary "${PROJECT_BINARY_DIR}/shaders/${shader}.${stage}.spv")
    set(shader_header "${PROJECT_BINARY_DIR}/generated/${shader}_${stage}_spv.hpp")
    add_custom_command(OUTPUT "${shader_header}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/shaders" "${PROJECT_BINARY_DIR}/generated"
      COMMAND "${THREEPOINT_GLSLANG_VALIDATOR}" -V "${shader_source}" -o "${shader_binary}"
      COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/embed_spirv.py"
              "${shader_binary}" "${shader_header}" "${shader}_${stage}_spv"
      DEPENDS "${shader_source}" "${PROJECT_SOURCE_DIR}/scripts/embed_spirv.py"
      VERBATIM)
    list(APPEND THREEPOINT_SHADER_HEADERS "${shader_header}")
  endforeach()
endforeach()
set(shader_source "${PROJECT_SOURCE_DIR}/src/render/shaders/ssao.frag")
set(shader_binary "${PROJECT_BINARY_DIR}/shaders/ssao.frag.spv")
set(shader_header "${PROJECT_BINARY_DIR}/generated/ssao_frag_spv.hpp")
add_custom_command(OUTPUT "${shader_header}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/shaders" "${PROJECT_BINARY_DIR}/generated"
  COMMAND "${THREEPOINT_GLSLANG_VALIDATOR}" -V "${shader_source}" -o "${shader_binary}"
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/embed_spirv.py"
          "${shader_binary}" "${shader_header}" "ssao_frag_spv"
  DEPENDS "${shader_source}" "${PROJECT_SOURCE_DIR}/scripts/embed_spirv.py"
  VERBATIM)
list(APPEND THREEPOINT_SHADER_HEADERS "${shader_header}")

add_library(threepoint_core STATIC src/core/project.cpp src/core/scene_document.cpp
            src/core/source_document.cpp
            src/core/audio_world.cpp
            src/core/editor_gizmo.cpp src/core/editor_viewport.cpp
            src/core/camera.cpp src/core/input.cpp
            src/core/ui_canvas.cpp
            src/core/world.cpp src/core/physics_world.cpp src/core/save_data.cpp
            src/core/gltf_asset.cpp src/core/cgltf_impl.c src/core/stb_image_impl.c
            src/core/hdr_image.cpp src/core/lua_project.cpp)
target_compile_features(threepoint_core PUBLIC cxx_std_17)
target_include_directories(threepoint_core PUBLIC include PRIVATE "${cgltf_SOURCE_DIR}"
                           "${stb_SOURCE_DIR}")
target_link_libraries(threepoint_core PUBLIC glm::glm threepoint_lua PRIVATE Jolt miniaudio)
target_compile_definitions(threepoint_core PUBLIC THREEPOINT_VERSION="${PROJECT_VERSION}")

add_executable(threepoint src/runtime/main.cpp src/runtime/export_game.cpp)
add_library(threepoint_render STATIC src/render/vulkan_window.cpp src/render/cube_demo.cpp
            ${THREEPOINT_SHADER_HEADERS})
target_compile_features(threepoint_render PUBLIC cxx_std_17)
target_include_directories(threepoint_render PUBLIC include PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(threepoint_render PUBLIC threepoint_core glfw nvrhi_vk nvrhi Vulkan::Headers
                      "${THREEPOINT_VULKAN_LOADER}")
target_link_libraries(threepoint PRIVATE threepoint_core threepoint_render)
add_executable(threepoint_player src/runtime/player.cpp)
set_target_properties(threepoint_player PROPERTIES OUTPUT_NAME threepoint-player)
target_link_libraries(threepoint_player PRIVATE threepoint_core threepoint_render)
add_dependencies(threepoint threepoint_player)

add_executable(threepoint_editor src/editor/main.cpp)
set_target_properties(threepoint_editor PROPERTIES OUTPUT_NAME threepoint-editor)
target_link_libraries(threepoint_editor PRIVATE threepoint_core threepoint_render)

include(CTest)
option(THREEPOINT_ENABLE_GPU_TESTS "Run live Vulkan display checks" OFF)
if(BUILD_TESTING)
  add_executable(threepoint_audio_world_test tests/audio_world_test.cpp)
  target_link_libraries(threepoint_audio_world_test PRIVATE threepoint_core)
  add_test(NAME audio_decode_spatial_and_errors COMMAND threepoint_audio_world_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene/assets/contact.wav")
  add_executable(threepoint_audio_scene_test tests/audio_scene_test.cpp)
  target_link_libraries(threepoint_audio_scene_test PRIVATE threepoint_core)
  add_test(NAME lua_audio_contact COMMAND threepoint_audio_scene_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_executable(threepoint_api_contract_scene_test tests/api_contract_scene_test.cpp)
  target_link_libraries(threepoint_api_contract_scene_test PRIVATE threepoint_core)
  add_test(NAME lua_api_contract_scene COMMAND threepoint_api_contract_scene_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME lua_api_coverage_audit COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/api_coverage_audit.py" "${PROJECT_SOURCE_DIR}")
  add_executable(threepoint_editor_viewport_test tests/editor_viewport_test.cpp)
  target_link_libraries(threepoint_editor_viewport_test PRIVATE threepoint_core)
  add_test(NAME editor_viewport_drag COMMAND threepoint_editor_viewport_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/editor_viewport_fixture")
  add_executable(threepoint_editor_gizmo_test tests/editor_gizmo_test.cpp)
  target_link_libraries(threepoint_editor_gizmo_test PRIVATE threepoint_core)
  add_test(NAME editor_gizmo_math COMMAND threepoint_editor_gizmo_test)
  add_executable(threepoint_scene_document_test tests/scene_document_test.cpp)
  target_link_libraries(threepoint_scene_document_test PRIVATE threepoint_core)
  add_test(NAME scene_document_roundtrip COMMAND threepoint_scene_document_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/scene_document_fixture")
  add_test(NAME editor_source_commands COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/editor_cli_test.py" $<TARGET_FILE:threepoint_editor>
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/editor_cli_fixture")
  add_executable(threepoint_source_document_test tests/source_document_test.cpp)
  target_link_libraries(threepoint_source_document_test PRIVATE threepoint_core)
  add_test(NAME editor_raw_source_roundtrip COMMAND threepoint_source_document_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/source_document_fixture")
  add_test(NAME editor_raw_source_commands COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/editor_source_cli_test.py"
           $<TARGET_FILE:threepoint_editor>
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/editor_source_cli_fixture")
  add_executable(threepoint_ui_menu_test tests/ui_menu_test.cpp)
  target_link_libraries(threepoint_ui_menu_test PRIVATE threepoint_core)
  add_test(NAME source_ui_menu_and_save COMMAND threepoint_ui_menu_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/ui_menu_fixture")
  add_executable(threepoint_save_data_test tests/save_data_test.cpp)
  target_link_libraries(threepoint_save_data_test PRIVATE threepoint_core)
  add_test(NAME save_file_integrity COMMAND threepoint_save_data_test
           "${PROJECT_BINARY_DIR}/save_data_fixture")
  add_executable(threepoint_save_lua_test tests/save_lua_test.cpp)
  target_link_libraries(threepoint_save_lua_test PRIVATE threepoint_core)
  add_test(NAME lua_save_roundtrip COMMAND threepoint_save_lua_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/save_lua_fixture")
  add_executable(threepoint_physics_test tests/feature_scene/physics_core_test.cpp)
  target_link_libraries(threepoint_physics_test PRIVATE threepoint_core)
  add_test(NAME physics_core COMMAND threepoint_physics_test)
  add_executable(threepoint_vehicle_scene_test tests/vehicle_scene_test.cpp)
  target_link_libraries(threepoint_vehicle_scene_test PRIVATE threepoint_core)
  add_test(NAME lua_vehicle_scene COMMAND threepoint_vehicle_scene_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_executable(threepoint_core_test tests/core_test.cpp)
  target_link_libraries(threepoint_core_test PRIVATE threepoint_core)
  add_executable(threepoint_camera_test tests/camera_test.cpp)
  target_link_libraries(threepoint_camera_test PRIVATE threepoint_core)
  add_test(NAME camera_math COMMAND threepoint_camera_test)
  add_executable(threepoint_lua_test tests/lua_project_test.cpp)
  target_link_libraries(threepoint_lua_test PRIVATE threepoint_core)
  add_test(NAME lua_scene_logic COMMAND threepoint_lua_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME lua_component_errors COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/components_scene_test.py" $<TARGET_FILE:threepoint>
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_executable(threepoint_input_test tests/input_test.cpp)
  target_link_libraries(threepoint_input_test PRIVATE threepoint_core)
  add_test(NAME input_actions_and_ray COMMAND threepoint_input_test)
  add_executable(threepoint_world_test tests/world_test.cpp)
  target_link_libraries(threepoint_world_test PRIVATE threepoint_core)
  add_test(NAME world_components COMMAND threepoint_world_test)
  add_executable(threepoint_scene_flow_test tests/scene_flow_test.cpp)
  target_link_libraries(threepoint_scene_flow_test PRIVATE threepoint_core)
  add_test(NAME source_scene_and_prefabs COMMAND threepoint_scene_flow_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/scene_flow_fixture")
  add_executable(threepoint_gltf_test tests/gltf_asset_test.cpp)
  target_link_libraries(threepoint_gltf_test PRIVATE threepoint_core)
  add_test(NAME gltf_geometry_and_errors COMMAND threepoint_gltf_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/gltf_fixture")
  add_executable(threepoint_hdr_image_test tests/hdr_image_test.cpp)
  target_link_libraries(threepoint_hdr_image_test PRIVATE threepoint_core)
  add_test(NAME hdr_image_validation COMMAND threepoint_hdr_image_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene"
           "${PROJECT_BINARY_DIR}/hdr_image_fixture")
  add_executable(threepoint_falling_blocks_rules_test tests/falling_blocks_rules_test.cpp)
  target_link_libraries(threepoint_falling_blocks_rules_test PRIVATE threepoint_lua)
  add_test(NAME falling_blocks_rules COMMAND threepoint_falling_blocks_rules_test
           "${PROJECT_SOURCE_DIR}/games/falling_blocks/scripts/board.lua"
           "${PROJECT_SOURCE_DIR}/tests/falling_blocks_rules.lua")
  add_executable(threepoint_garage_run_rules_test tests/garage_run_rules_test.cpp)
  target_link_libraries(threepoint_garage_run_rules_test PRIVATE threepoint_lua)
  add_test(NAME garage_run_driving_rules COMMAND threepoint_garage_run_rules_test
           "${PROJECT_SOURCE_DIR}/games/garage_run/scripts/driving.lua"
           "${PROJECT_SOURCE_DIR}/tests/garage_run_rules.lua")
  add_executable(threepoint_garage_run_impact_test tests/garage_run_impact_test.cpp)
  target_link_libraries(threepoint_garage_run_impact_test PRIVATE threepoint_lua)
  add_test(NAME garage_run_impact_camera COMMAND threepoint_garage_run_impact_test
           "${PROJECT_SOURCE_DIR}/games/garage_run/scripts/impact.lua"
           "${PROJECT_SOURCE_DIR}/tests/garage_run_impact.lua")
  add_executable(threepoint_garage_run_game_test tests/garage_run_game_test.cpp)
  target_link_libraries(threepoint_garage_run_game_test PRIVATE threepoint_core)
  add_test(NAME garage_run_gameplay COMMAND threepoint_garage_run_game_test
           "${PROJECT_SOURCE_DIR}/games/garage_run"
           "${PROJECT_BINARY_DIR}/garage_run_game_fixture")
  add_test(NAME garage_run_assets COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/garage_run_assets_test.py"
           "${PROJECT_SOURCE_DIR}/games/garage_run"
           "${PROJECT_BINARY_DIR}/garage_run_assets_fixture")
  add_test(NAME garage_run_export COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/garage_run_export_test.py"
           $<TARGET_FILE:threepoint>
           "${PROJECT_SOURCE_DIR}/games/garage_run"
           $<TARGET_FILE:threepoint_garage_run_game_test>
           "${PROJECT_BINARY_DIR}/garage_run_export_fixture")
  add_executable(threepoint_falling_blocks_game_test tests/falling_blocks_game_test.cpp)
  target_link_libraries(threepoint_falling_blocks_game_test PRIVATE threepoint_core)
  add_test(NAME falling_blocks_game_logic COMMAND threepoint_falling_blocks_game_test
           "${PROJECT_SOURCE_DIR}/games/falling_blocks")
  add_executable(threepoint_falling_blocks_lit_game_test tests/falling_blocks_lit_game_test.cpp)
  target_link_libraries(threepoint_falling_blocks_lit_game_test PRIVATE threepoint_core)
  add_test(NAME falling_blocks_lit_game_logic COMMAND threepoint_falling_blocks_lit_game_test
           "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
  add_test(NAME falling_blocks_export COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/falling_blocks_export_test.py"
           $<TARGET_FILE:threepoint>
           "${PROJECT_SOURCE_DIR}/games/falling_blocks"
           "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit"
           "${PROJECT_BINARY_DIR}/falling_blocks_export_fixture")
  add_test(NAME core_project_check COMMAND threepoint_core_test
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME runtime_project_check COMMAND threepoint --check-project
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  add_test(NAME headless_project_test COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/project_test_cli.py" $<TARGET_FILE:threepoint>
           "${PROJECT_SOURCE_DIR}/tests/project_test_fixture"
           "${PROJECT_BINARY_DIR}/headless_project_fixture")
  add_test(NAME standalone_export_files COMMAND "${Python3_EXECUTABLE}"
           "${PROJECT_SOURCE_DIR}/tests/export_game_test.py" $<TARGET_FILE:threepoint>
           "${PROJECT_SOURCE_DIR}/tests/project_test_fixture"
           "${PROJECT_BINARY_DIR}/export_files_fixture")
  add_test(NAME editor_project_check COMMAND threepoint_editor --check-project
           "${PROJECT_SOURCE_DIR}/tests/feature_scene")
  if(THREEPOINT_ENABLE_GPU_TESTS)
    add_test(NAME standalone_export_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/export_player_capture_test.py"
             $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/project_test_fixture"
             "${PROJECT_BINARY_DIR}/export_capture_fixture")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME vulkan_smoke COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/vulkan_smoke_test.py" $<TARGET_FILE:threepoint>)
    add_test(NAME cube_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/cube_capture_test.py" $<TARGET_FILE:threepoint>)
    add_test(NAME lua_scene_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_scene_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME lua_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME falling_blocks_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks")
    add_test(NAME falling_blocks_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_capture_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks")
    add_test(NAME falling_blocks_lit_assets COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_lit_assets_test.py"
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
    add_test(NAME falling_blocks_lit_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_capture_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
    add_test(NAME falling_blocks_lit_ssao COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_lit_ssao_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
    add_test(NAME falling_blocks_lit_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit")
    add_test(NAME falling_blocks_export_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/falling_blocks_export_test.py"
             $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/games/falling_blocks"
             "${PROJECT_SOURCE_DIR}/games/falling_blocks_lit"
             "${PROJECT_BINARY_DIR}/falling_blocks_export_capture" --graphical)
    add_test(NAME garage_run_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/garage_run_capture_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/games/garage_run" "${PROJECT_BINARY_DIR}/garage_run_capture")
    add_test(NAME garage_run_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/games/garage_run")
    add_test(NAME garage_run_export_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/garage_run_export_test.py"
             $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/games/garage_run"
             $<TARGET_FILE:threepoint_garage_run_game_test>
             "${PROJECT_BINARY_DIR}/garage_run_export_capture" --graphical)
    add_test(NAME input_scene_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/input_scene_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME gltf_scene_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/gltf_scene_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME pbr_material_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/pbr_material_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME hdr_exposure_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/hdr_exposure_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME hdr_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "hdr")
    add_test(NAME ibl_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/ibl_capture_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME ibl_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "ibl")
    add_test(NAME shadow_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/shadow_capture_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME ssao_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/ssao_capture_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME ssao_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "ssao")
    add_test(NAME physics_scene_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/physics_scene_capture_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME physics_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "physics")
    add_test(NAME audio_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "audio")
    add_test(NAME ui_menu_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/ui_capture_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "${PROJECT_BINARY_DIR}")
    add_test(NAME ui_menu_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "menu")
    add_test(NAME editor_viewport_capture COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/editor_viewport_capture_test.py"
             $<TARGET_FILE:threepoint_editor> "${PROJECT_SOURCE_DIR}/tests/feature_scene"
             "${PROJECT_BINARY_DIR}")
    add_test(NAME editor_viewport_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/editor_viewport_live_test.py"
             $<TARGET_FILE:threepoint_editor> "${PROJECT_SOURCE_DIR}/tests/feature_scene")
    add_test(NAME shadow_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "shadow")
    add_test(NAME point_shadow_live_startup COMMAND "${Python3_EXECUTABLE}"
             "${PROJECT_SOURCE_DIR}/tests/lua_live_start_test.py" $<TARGET_FILE:threepoint>
             "${PROJECT_SOURCE_DIR}/tests/feature_scene" "shadow_point")
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
