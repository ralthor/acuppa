# ThreePoint Linux release

The engine and each sample game ship as separate `linux-<architecture>.tar.gz` archives. These are portable folders, not system installers. On Ubuntu 24+, install the system GLFW library (`libglfw3`), Vulkan loader (`libvulkan1`), and a working Vulkan GPU driver. The Lua, physics, audio, renderer, and shader code is included in the executables. No editor or build tools are needed to play a sample game.

Extract the engine archive and run `./threepoint --version`. To run a source project, use `./threepoint run /path/to/project`; `./threepoint-editor` handles source scene editing. The sibling `threepoint-player` is needed when `threepoint build` exports a game. Extract a game archive and run `./threepoint-game` inside its folder. Its `--check` option validates every scene without a window. Game saves live under XDG user data, so a game folder may be read-only.

After building and testing the repository, run `bash scripts/package-linux-release.sh`. It exports and checks all three sample games, writes four deterministic archives under `build/release/`, and records SHA-256 hashes in `SHA256SUMS`. The archives include the notices for statically linked dependencies. Verify an archive with `sha256sum -c SHA256SUMS` from that folder.

This release target is Linux only. Hosted Ubuntu 24+ CI and speaker playback on user hardware remain separate validation gates until they are run on those machines.
