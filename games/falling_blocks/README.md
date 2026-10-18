# Falling Blocks

This source-only 3D game runs directly in ThreePoint. Its 10×20 board rules live in `scripts/board.lua`, independent of rendering and the editor. The project manifest, scene, and gameplay script define the whole game; no editor clicks are needed.

From the repository root, run `build/threepoint run games/falling_blocks`. Use Left/Right to move, Up to rotate, Down to fall faster, Space to hard drop, C to hold, P to pause, and R to restart. A controller can use the directional pad, A to rotate, B to hard drop, X to hold, Start to pause, and Back to restart. The in-game guide and this file list the controls.

Pieces come from deterministic, shuffled seven-piece bags. They fall with fixed-step gravity, lock, clear complete rows, and stop at top-out. The window shows the next and held pieces, a ghost landing outline, score, lines, level, pause, and game over. Every ten cleared lines raises the level and fall speed. Hard drops score two points per row and soft drops score one; line clears score 100, 300, 500, or 800 times the current level. Wall kicks try nearby positions when a rotation is blocked. Restart begins the same reproducible sequence.

After building, run `ctest --test-dir build -R falling_blocks --output-on-failure` for the deterministic rules, full-match gameplay logic, live startup, and Vulkan frame tests.

To export a standalone Linux game from the repository root, run `build/threepoint build games/falling_blocks build/falling_blocks_release`, then launch `build/falling_blocks_release/threepoint-game`. The bundle contains the source gameplay files and an executable player; no editor, Python generator, or CMake is needed to play it. `build/falling_blocks_release/threepoint-game --check` validates the bundle without opening a window.
