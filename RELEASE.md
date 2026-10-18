# ACuppa Linux release

The engine and each sample game ship as separate `linux-<architecture>.tar.gz` archives. These are portable folders, not system installers. On Ubuntu 24+, install the system GLFW library (`libglfw3`), Vulkan loader (`libvulkan1`), and a working Vulkan GPU driver. The Lua, physics, audio, renderer, and shader code is included in the executables. No editor or build tools are needed to play a sample game.

Extract the engine archive and run `./acuppa --version`. To run a source project, use `./acuppa run /path/to/project`; `./acuppa-editor` handles source scene editing. The sibling `acuppa-player` is needed when `acuppa build` exports a game. Extract a game archive and run `./acuppa-game` inside its folder. Its `--check` option validates every scene without a window. Game saves live under XDG user data, so a game folder may be read-only.

Exported games from before the engine rename stored saves under `threepoint/<game.id>/saves/` in XDG user data. Copy those save files to `acuppa/<game.id>/saves/` if you want a newly exported ACuppa game to continue an earlier save. Source-project saves remain in their project folders.

After building and testing the repository, run `bash scripts/package-linux-release.sh`. It exports and checks all three sample games, writes four deterministic archives under `build/release/`, and records SHA-256 hashes in `SHA256SUMS`. The archives include the notices for statically linked dependencies. Verify an archive with `sha256sum -c SHA256SUMS` from that folder.

Run `bash scripts/verify-clean-release.sh` to build only committed source in an isolated directory and run the complete unit, integration, Vulkan capture, live-startup, and archive suite. It uses the locally bootstrapped pinned toolchain and dependencies. The separate sanitizer gate is `bash scripts/test-sanitizers-local.sh`.

This release target is Linux only. Hosted Ubuntu 24+ CI and speaker playback on user hardware remain separate validation gates until they are run on those machines.

## Local validation record — 2026-10-18

- Normal local ACuppa build: 78/78 automated tests passed, including the engine name check, Vulkan captures, interactive startup, and standalone archives on an NVIDIA GeForce GTX 1660.
- Debug ASan/UBSan build: 40/40 device-free tests passed.
- Clean committed-source ACuppa build: 76/76 tests passed, including all three packaged games, archive hashes, reproducibility, and re-export from the packaged engine.
- Hosted Ubuntu 24+ CI has not run because this repository has no Git remote. Speaker playback quality has not been checked on the user's output hardware. Treat these as open release qualification checks before public distribution.
