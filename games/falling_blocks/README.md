# Falling Blocks

This source-only 3D game runs directly in ThreePoint. Its 10×20 board rules live in `scripts/board.lua`, independent of rendering and the editor. The project manifest, scene, and gameplay script define the whole game; no editor clicks are needed.

From the repository root, run `build/threepoint run games/falling_blocks`. Use Left/Right to move, Up to rotate, Down to fall faster, Space to hard drop, and R to restart. A controller can use the directional pad, A to rotate, B to hard drop, and Start to restart. Pieces fall with fixed-step gravity, lock, clear complete rows, and stop at top-out. Score is tracked internally; Phase 12 adds visible score, game-over feedback, hold, next, ghost, levels, and pause.

After building, run `ctest --test-dir build -R falling_blocks --output-on-failure` for the deterministic rules, gameplay logic, live startup, and Vulkan frame tests.
