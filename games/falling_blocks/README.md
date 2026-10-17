# Falling Blocks

This is the source-only 3D falling-block game under construction. Its 10×20 board rules live in `scripts/board.lua`, independent of rendering and the editor. The seven shapes can spawn, move, rotate, collide, lock, and clear lines. A separate game scene and controls arrive in Phase 11.

Run the deterministic rule test with `ctest --test-dir build -R falling_blocks_rules --output-on-failure` after building the project.
