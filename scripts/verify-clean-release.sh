#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
tools="$root/.tools"
sysroot="$tools/sysroot"
work="$root/build/clean-release-check"
if [[ ! -x "$tools/venv/bin/cmake" || ! -d "$tools/vendor/JoltPhysics" ]]; then
  echo "Run scripts/bootstrap-local.sh before the clean release check" >&2
  exit 1
fi
rm -rf "$work"
mkdir -p "$work/source"
git -C "$root" archive HEAD | tar -x -C "$work/source"
export ZIG_GLOBAL_CACHE_DIR="$tools/zig-cache"
export THREEPOINT_VENDOR_DIR="$tools/vendor"
export VK_LAYER_PATH="$sysroot/usr/share/vulkan/explicit_layer.d"
export LD_LIBRARY_PATH="$sysroot/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
"$tools/venv/bin/cmake" -S "$work/source" -B "$work/build" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$tools/venv/bin/ninja" \
  -DCMAKE_C_COMPILER="$tools/venv/bin/python-zig" -DCMAKE_C_COMPILER_ARG1=cc \
  -DCMAKE_CXX_COMPILER="$tools/venv/bin/python-zig" -DCMAKE_CXX_COMPILER_ARG1=c++ \
  -DCMAKE_CXX_FLAGS="-I$sysroot/usr/include/x86_64-linux-gnu" \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DTHREEPOINT_ENABLE_GPU_TESTS=ON \
  -Dglfw3_DIR="$sysroot/usr/lib/x86_64-linux-gnu/cmake/glfw3" \
  -DTHREEPOINT_GLSLANG_VALIDATOR="$sysroot/usr/bin/glslangValidator" \
  -DTHREEPOINT_VULKAN_LOADER="$sysroot/usr/lib/x86_64-linux-gnu/libvulkan.so" \
  -DFETCHCONTENT_SOURCE_DIR_VULKAN_HEADERS="$tools/vendor/vulkan-headers" \
  -DFETCHCONTENT_SOURCE_DIR_NVRHI="$tools/vendor/nvrhi" \
  -DFETCHCONTENT_SOURCE_DIR_GLM="$tools/vendor/glm" \
  -DFETCHCONTENT_SOURCE_DIR_CGLTF="$tools/vendor/cgltf" \
  -DFETCHCONTENT_SOURCE_DIR_STB="$tools/vendor/stb" \
  -DFETCHCONTENT_SOURCE_DIR_MINIAUDIO="$tools/vendor/miniaudio" \
  -DFETCHCONTENT_SOURCE_DIR_JOLT="$tools/vendor/JoltPhysics" \
  -DFETCHCONTENT_SOURCE_DIR_LUA="$tools/vendor/lua-5.4.8"
"$tools/venv/bin/cmake" --build "$work/build" --parallel 4
"$tools/venv/bin/ctest" --test-dir "$work/build" --output-on-failure --parallel 4
echo "Clean source release check passed at $(git -C "$root" rev-parse --short HEAD)"
