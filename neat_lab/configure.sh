#!/usr/bin/env bash
# Configures and builds the NEAT lab against the unchanged ACuppa engine sources.
# Run scripts/bootstrap-local.sh once first; this reuses its pinned tools in .tools/.
set -euo pipefail

lab_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(dirname "$lab_dir")"
tools_dir="$project_root/.tools"
sysroot_dir="$tools_dir/sysroot"
build_dir="${1:-$project_root/build-neat}"

if [ ! -x "$tools_dir/venv/bin/cmake" ]; then
  echo "Missing pinned tools; run scripts/bootstrap-local.sh first" >&2
  exit 1
fi

export ZIG_GLOBAL_CACHE_DIR="$tools_dir/zig-cache"
"$tools_dir/venv/bin/cmake" -S "$lab_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$tools_dir/venv/bin/ninja" \
  -DCMAKE_C_COMPILER="$tools_dir/venv/bin/python-zig" \
  -DCMAKE_C_COMPILER_ARG1=cc \
  -DCMAKE_CXX_COMPILER="$tools_dir/venv/bin/python-zig" \
  -DCMAKE_CXX_COMPILER_ARG1=c++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-I$sysroot_dir/usr/include/x86_64-linux-gnu" \
  -Dglfw3_DIR="$sysroot_dir/usr/lib/x86_64-linux-gnu/cmake/glfw3" \
  -DACUPPA_GLSLANG_VALIDATOR="$sysroot_dir/usr/bin/glslangValidator" \
  -DACUPPA_VULKAN_LOADER="$sysroot_dir/usr/lib/x86_64-linux-gnu/libvulkan.so" \
  -DFETCHCONTENT_SOURCE_DIR_VULKAN_HEADERS="$tools_dir/vendor/vulkan-headers" \
  -DFETCHCONTENT_SOURCE_DIR_NVRHI="$tools_dir/vendor/nvrhi" \
  -DFETCHCONTENT_SOURCE_DIR_GLM="$tools_dir/vendor/glm" \
  -DFETCHCONTENT_SOURCE_DIR_CGLTF="$tools_dir/vendor/cgltf" \
  -DFETCHCONTENT_SOURCE_DIR_STB="$tools_dir/vendor/stb" \
  -DFETCHCONTENT_SOURCE_DIR_MINIAUDIO="$tools_dir/vendor/miniaudio" \
  -DFETCHCONTENT_SOURCE_DIR_JOLT="$tools_dir/vendor/JoltPhysics" \
  -DFETCHCONTENT_SOURCE_DIR_LUA="$tools_dir/vendor/lua-5.4.8" \
  -DNEAT_LAB_PYTHON="$tools_dir/neat-venv/bin/python"
"$tools_dir/venv/bin/cmake" --build "$build_dir"
