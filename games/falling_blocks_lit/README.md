# Falling Blocks Lit

This is a separate visual version of [Falling Blocks](../falling_blocks/README.md). Its rules and controls are the same. Seven source-generated glTF tile materials give each piece a distinct color and rounded, PBR-lit edges. A dark playfield, directional light, project-local studio HDRI, HDR exposure, and SSAO add depth to the board and pieces. The HUD and ghost outline still use the engine's built-in cubes.

From the repository root, run `build/threepoint run games/falling_blocks_lit`. Use Left/Right to move, Up to rotate, Down to fall faster, Space to hard drop, C to hold, P to pause, and R to restart. A controller can use the directional pad, A to rotate, B to hard drop, X to hold, Start to pause, and Back to restart. No editor clicks or asset download are needed.

The main scene enables medium-quality SSAO from Lua with a 1.15-unit radius and 0.6 strength. To compare the same playable game without the effect, run `build/threepoint run games/falling_blocks_lit --scene ssao_off`. Both scenes use the same board, pieces, controls, and rules.

Pieces come from deterministic, shuffled seven-piece bags. They fall with fixed-step gravity, lock, clear complete rows, and stop at top-out. The window shows the next and held pieces, a ghost landing outline, score, lines, level, pause, and game over. Every ten cleared lines raises the level and fall speed. Hard drops score two points per row and soft drops score one; line clears score 100, 300, 500, or 800 times the current level. Wall kicks try nearby positions when a rotation is blocked. Restart begins the same reproducible sequence.

`make_assets.py` uses only Python's standard library to generate the checked-in `assets/` files. To regenerate them, run `python3 games/falling_blocks_lit/make_assets.py` from the repository root. The game itself only needs ThreePoint; Python is not required to play. To test the copied game after building, run `ctest --test-dir build -R falling_blocks_lit --output-on-failure`.

The HDRI lights the PBR tiles and panel; it is not drawn as a sky. Shadow casting, sound, and animation remain future game work.
