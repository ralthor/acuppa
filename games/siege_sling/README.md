# Siege Sling

*In development.* Only the deterministic rules exist so far
([scripts/rules.lua](scripts/rules.lua)); there is no scene, game.lua, or
assets yet, so the project cannot be run.

Siege Sling is a 3D slingshot game. A sling on a grassy hill launches a ball
at forts built from wood, stone, and glass blocks that guard round creatures
called grumps. The sling turns left and right as well as up and down: pull
back from the sling with the mouse, where the drag direction sets the aim and
its length sets the power, or aim with the keyboard or a gamepad. Each level
gives a limited number of shots. Defeat every grump to win, scoring points
for broken blocks, defeated grumps, and unused shots, and earn one to three
stars.

Run the rule tests from the repository root with
`ctest --test-dir build -R siege_sling --output-on-failure`.
