#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
tools_dir="$project_root/.tools"
debs_dir="$tools_dir/debs"
sysroot_dir="$tools_dir/sysroot"

for tool in uv apt-get dpkg-deb git curl sha256sum; do
  command -v "$tool" >/dev/null || { echo "Missing setup tool: $tool" >&2; exit 1; }
done

mkdir -p "$debs_dir" "$sysroot_dir" "$tools_dir/uv-cache" "$tools_dir/zig-cache" "$tools_dir/vendor"
if [ ! -d "$tools_dir/venv" ]; then
  UV_CACHE_DIR="$tools_dir/uv-cache" uv venv "$tools_dir/venv"
fi
UV_CACHE_DIR="$tools_dir/uv-cache" uv pip install --python "$tools_dir/venv/bin/python" \
  ziglang==0.17.0 cmake==4.4.4 ninja==1.13.2

download_and_extract() {
  local name="$1" version="$2" architecture="${3:-amd64}"
  local archive="$debs_dir/${name}_${version}_${architecture}.deb"
  if [ ! -f "$archive" ]; then
    (cd "$debs_dir" && apt-get download "$name=$version")
  fi
  dpkg-deb -x "$archive" "$sysroot_dir"
}

download_and_extract libsdl3-dev '3.4.2+ds-1ubuntu1'
download_and_extract libsdl3-0 '3.4.2+ds-1ubuntu1'
download_and_extract liblua5.4-dev '5.4.8-1build1'
download_and_extract liblua5.4-0 '5.4.8-1build1'
download_and_extract libglfw3-dev '3.4-4'
download_and_extract libglfw3 '3.4-4'
download_and_extract libvulkan-dev '1.4.341.0-1'
download_and_extract libvulkan1 '1.4.341.0-1'
download_and_extract vulkan-validationlayers '1.4.341.0-1'
download_and_extract glslang-tools '16.2.0-2'

require_source() {
  local name="$1" url="$2" revision="$3"
  local source="$tools_dir/vendor/$name"
  if [ ! -d "$source/.git" ]; then
    mkdir -p "$source"
    git -C "$source" init -q
    git -C "$source" remote add origin "$url"
    git -C "$source" fetch --depth 1 origin "$revision"
    git -C "$source" checkout --detach -q FETCH_HEAD
  fi
  if [ "$(git -C "$source" rev-parse HEAD)" != "$revision" ]; then
    echo "Pinned source mismatch in $source (expected $revision)" >&2
    exit 1
  fi
}

require_source vulkan-headers https://github.com/KhronosGroup/Vulkan-Headers.git \
  015e25c3c91b70eb1a754d36fb14c4ba6ad9b0b9
require_source nvrhi https://github.com/NVIDIA-RTX/NVRHI.git \
  6b96fb03e07539f08327aea76c56d55f1de9d906
require_source glm https://github.com/g-truc/glm.git \
  0af55ccecd98d4e5a8d1fad7de25ba429d60e863
require_source cgltf https://github.com/jkuhlmann/cgltf.git \
  360db1a95480fe102ae9c69b27c5d101167ff5ba
require_source stb https://github.com/nothings/stb.git \
  2c980bb59875b0d32144a71867fbdebb2f77cd20
require_source JoltPhysics https://github.com/jrouwe/JoltPhysics.git \
  e77f175595e64cb44218cc9d9d56fc365ad0e36a

lua_archive="$debs_dir/lua-5.4.8.tar.gz"
lua_hash=4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae
if [ ! -f "$lua_archive" ]; then
  curl -fL --retry 3 https://www.lua.org/ftp/lua-5.4.8.tar.gz -o "$lua_archive"
fi
if [ "$(sha256sum "$lua_archive" | cut -d ' ' -f 1)" != "$lua_hash" ]; then
  echo "Pinned Lua archive hash mismatch" >&2
  exit 1
fi
if [ ! -d "$tools_dir/vendor/lua-5.4.8/src" ]; then
  tar -xzf "$lua_archive" -C "$tools_dir/vendor"
fi

gpu_tests=OFF
if [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then gpu_tests=ON; fi

ZIG_GLOBAL_CACHE_DIR="$tools_dir/zig-cache" "$tools_dir/venv/bin/cmake" --fresh \
  -S "$project_root" -B "$project_root/build" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$tools_dir/venv/bin/ninja" \
  -DCMAKE_C_COMPILER="$tools_dir/venv/bin/python-zig" \
  -DCMAKE_C_COMPILER_ARG1=cc \
  -DCMAKE_CXX_COMPILER="$tools_dir/venv/bin/python-zig" \
  -DCMAKE_CXX_COMPILER_ARG1=c++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-I$sysroot_dir/usr/include/x86_64-linux-gnu" \
  -DSDL3_DIR="$sysroot_dir/usr/lib/x86_64-linux-gnu/cmake/SDL3" \
  -DLUA_INCLUDE_DIR="$sysroot_dir/usr/include/lua5.4" \
  -DLUA_LIBRARY="$sysroot_dir/usr/lib/x86_64-linux-gnu/liblua5.4.so" \
  -DLUA_MATH_LIBRARY=m \
  -Dglfw3_DIR="$sysroot_dir/usr/lib/x86_64-linux-gnu/cmake/glfw3" \
  -DTHREEPOINT_GLSLANG_VALIDATOR="$sysroot_dir/usr/bin/glslangValidator" \
  -DTHREEPOINT_VULKAN_LOADER="$sysroot_dir/usr/lib/x86_64-linux-gnu/libvulkan.so" \
  -DFETCHCONTENT_SOURCE_DIR_VULKAN_HEADERS="$tools_dir/vendor/vulkan-headers" \
  -DFETCHCONTENT_SOURCE_DIR_NVRHI="$tools_dir/vendor/nvrhi" \
  -DFETCHCONTENT_SOURCE_DIR_GLM="$tools_dir/vendor/glm" \
  -DFETCHCONTENT_SOURCE_DIR_CGLTF="$tools_dir/vendor/cgltf" \
  -DFETCHCONTENT_SOURCE_DIR_STB="$tools_dir/vendor/stb" \
  -DFETCHCONTENT_SOURCE_DIR_JOLT="$tools_dir/vendor/JoltPhysics" \
  -DFETCHCONTENT_SOURCE_DIR_LUA="$tools_dir/vendor/lua-5.4.8" \
  -DTHREEPOINT_BUILD_2D_PROTOTYPE=ON \
  -DTHREEPOINT_ENABLE_GPU_TESTS="$gpu_tests"
ZIG_GLOBAL_CACHE_DIR="$tools_dir/zig-cache" "$tools_dir/venv/bin/cmake" --build "$project_root/build"
"$project_root/build/threepoint" --version
VK_LAYER_PATH="$sysroot_dir/usr/share/vulkan/explicit_layer.d" \
LD_LIBRARY_PATH="$sysroot_dir/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}" \
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
ZIG_GLOBAL_CACHE_DIR="$tools_dir/zig-cache" "$tools_dir/venv/bin/ctest" \
  --test-dir "$project_root/build" --output-on-failure
