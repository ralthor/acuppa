-- Source-only 3D project. No editor state or manual scene setup is required.
return {
  api = 1,
  title = "ThreePoint Feature Scene",
  window = {width = 1280, height = 720},
  first_scene = "main",
  scenes = {main = "scenes/main.scene.lua", alternate = "scenes/alternate.scene.lua",
            imported = "scenes/imported.scene.lua",
            textured = "scenes/textured.scene.lua", spheres = "scenes/spheres.scene.lua",
            point = "scenes/point.scene.lua", hdr = "scenes/hdr.scene.lua",
            hdr_dim = "scenes/hdr.scene.lua", ibl = "scenes/ibl.scene.lua",
            ibl_off = "scenes/ibl.scene.lua", ibl_missing = "scenes/ibl.scene.lua"},
  prefabs = {probe = "prefabs/probe.prefab.lua"},
  input = {
    turn = {keyboard = "space", gamepad_button = "a"},
    camera_right = {keyboard = "d", gamepad_axis = "right_x"},
    select = {mouse_button = "left"},
    jump = {gamepad_button = "b"},
    pan = {gamepad_axis = "left_x"},
  },
}
