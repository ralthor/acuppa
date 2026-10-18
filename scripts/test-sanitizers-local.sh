#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
tools_dir="$project_root/.tools"
sysroot_dir="$tools_dir/sysroot"
if [ ! -x "$tools_dir/venv/bin/cmake" ] || [ ! -d "$tools_dir/vendor/JoltPhysics" ]; then
  echo "Run scripts/bootstrap-local.sh before the sanitizer check" >&2
  exit 1
fi

export ZIG_GLOBAL_CACHE_DIR="$tools_dir/zig-cache"
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}"

"$tools_dir/venv/bin/cmake" -S "$project_root" -B "$project_root/build-sanitize" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$tools_dir/venv/bin/ninja" \
  -DCMAKE_C_COMPILER="$tools_dir/venv/bin/python-zig" -DCMAKE_C_COMPILER_ARG1=cc \
  -DCMAKE_CXX_COMPILER="$tools_dir/venv/bin/python-zig" -DCMAKE_CXX_COMPILER_ARG1=c++ \
  -DCMAKE_CXX_FLAGS="-I$sysroot_dir/usr/include/x86_64-linux-gnu" \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DACUPPA_ENABLE_SANITIZERS=ON -DACUPPA_ENABLE_GPU_TESTS=OFF \
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
  -DFETCHCONTENT_SOURCE_DIR_LUA="$tools_dir/vendor/lua-5.4.8"
"$tools_dir/venv/bin/cmake" --build "$project_root/build-sanitize" --parallel
"$tools_dir/venv/bin/ctest" --test-dir "$project_root/build-sanitize" --output-on-failure
