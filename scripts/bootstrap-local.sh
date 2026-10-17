#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
tools_dir="$project_root/.tools"
debs_dir="$tools_dir/debs"
sysroot_dir="$tools_dir/sysroot"

for tool in uv apt-get dpkg-deb; do
  command -v "$tool" >/dev/null || { echo "Missing setup tool: $tool" >&2; exit 1; }
done

mkdir -p "$debs_dir" "$sysroot_dir" "$tools_dir/uv-cache" "$tools_dir/zig-cache"
if [ ! -d "$tools_dir/venv" ]; then
  UV_CACHE_DIR="$tools_dir/uv-cache" uv venv "$tools_dir/venv"
fi
UV_CACHE_DIR="$tools_dir/uv-cache" uv pip install --python "$tools_dir/venv/bin/python" \
  ziglang==0.17.0 cmake==4.4.4 ninja==1.13.2

download_and_extract() {
  local name="$1" version="$2"
  local archive="$debs_dir/${name}_${version}_amd64.deb"
  if [ ! -f "$archive" ]; then
    (cd "$debs_dir" && apt-get download "$name=$version")
  fi
  dpkg-deb -x "$archive" "$sysroot_dir"
}

download_and_extract libsdl3-dev '3.4.2+ds-1ubuntu1'
download_and_extract libsdl3-0 '3.4.2+ds-1ubuntu1'
download_and_extract liblua5.4-dev '5.4.8-1build1'
download_and_extract liblua5.4-0 '5.4.8-1build1'

ZIG_GLOBAL_CACHE_DIR="$tools_dir/zig-cache" "$tools_dir/venv/bin/cmake" --fresh \
  -S "$project_root" -B "$project_root/build" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$tools_dir/venv/bin/ninja" \
  -DCMAKE_CXX_COMPILER="$tools_dir/venv/bin/python-zig" \
  -DCMAKE_CXX_COMPILER_ARG1=c++ \
  -DCMAKE_CXX_FLAGS="-I$sysroot_dir/usr/include/x86_64-linux-gnu" \
  -DSDL3_DIR="$sysroot_dir/usr/lib/x86_64-linux-gnu/cmake/SDL3" \
  -DLUA_INCLUDE_DIR="$sysroot_dir/usr/include/lua5.4" \
  -DLUA_LIBRARY="$sysroot_dir/usr/lib/x86_64-linux-gnu/liblua5.4.so" \
  -DLUA_MATH_LIBRARY=m
ZIG_GLOBAL_CACHE_DIR="$tools_dir/zig-cache" "$tools_dir/venv/bin/cmake" --build "$project_root/build"
"$project_root/build/threepoint"
