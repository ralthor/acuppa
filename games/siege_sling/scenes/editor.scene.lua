-- Level editor: the same yard as the play scene (hill, sling, ground, sun),
-- so the designer sees the player's view, driven by scripts/editor.lua.
local file = assert(package.searchpath("scenes/play.scene", package.path, "", ""))
local scene = dofile(file)
scene.script = "scripts/editor.lua"
return scene
