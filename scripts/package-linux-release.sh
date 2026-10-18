#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${2:-$root/build}"
output="${1:-$build_dir/release}"
version="$(sed -nE 's/^project\(ThreePoint VERSION ([0-9]+\.[0-9]+\.[0-9]+).*$/\1/p' "$root/CMakeLists.txt")"
arch="$(uname -m)"
if [[ -z "$version" || ! -x "$build_dir/threepoint" ||
      ! -x "$build_dir/threepoint-editor" || ! -x "$build_dir/threepoint-player" ]]; then
  echo "Build all ThreePoint targets before packaging" >&2
  exit 1
fi
if [[ -e "$output" ]]; then
  echo "Release destination already exists: $output" >&2
  exit 1
fi
mkdir -p "$(dirname "$output")"
stage="$output.tmp.$$"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/packages" "$stage/user_data"
base="threepoint-$version-linux-$arch"

notices() {
  local target="$1"
  mkdir -p "$target/licenses"
  cp "$root/.tools/vendor/nvrhi/LICENSE.txt" "$target/licenses/nvrhi.txt"
  cp "$root/.tools/vendor/vulkan-headers/LICENSE.md" "$target/licenses/vulkan-headers.md"
  cp "$root/.tools/vendor/glm/copying.txt" "$target/licenses/glm.txt"
  cp "$root/.tools/vendor/cgltf/LICENSE" "$target/licenses/cgltf.txt"
  cp "$root/.tools/vendor/stb/LICENSE" "$target/licenses/stb.txt"
  cp "$root/.tools/vendor/JoltPhysics/LICENSE" "$target/licenses/jolt.txt"
  cp "$root/.tools/vendor/miniaudio/LICENSE" "$target/licenses/miniaudio.txt"
  cp "$root/.tools/vendor/lua-5.4.8/doc/readme.html" "$target/licenses/lua.html"
}

engine="$base-engine"
mkdir -p "$stage/packages/$engine"
cp "$build_dir/threepoint" "$build_dir/threepoint-editor" \
   "$build_dir/threepoint-player" "$stage/packages/$engine/"
cp "$root/README.md" "$root/DEPENDENCIES.md" "$root/RELEASE.md" \
   "$stage/packages/$engine/"
notices "$stage/packages/$engine"

for game in falling_blocks falling_blocks_lit garage_run; do
  package="$base-$game"
  "$build_dir/threepoint" build "$root/games/$game" "$stage/packages/$package"
  notices "$stage/packages/$package"
  XDG_DATA_HOME="$stage/user_data" "$stage/packages/$package/threepoint-game" --check
done

for package_dir in "$stage/packages"/*; do
  name="$(basename "$package_dir")"
  tar --sort=name --mtime='@0' --owner=0 --group=0 --numeric-owner \
      -C "$stage/packages" -cf - "$name" | gzip -n > "$stage/$name.tar.gz"
done
(cd "$stage" && sha256sum ./*.tar.gz > SHA256SUMS)
rm -rf "$stage/packages" "$stage/user_data"
mv "$stage" "$output"
trap - EXIT
echo "Linux release archives: $output"
