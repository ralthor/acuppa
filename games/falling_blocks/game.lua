return {
  api = 1,
  title = "Falling Blocks",
  window = {width = 1280, height = 720},
  first_scene = "main",
  scenes = {main = "scenes/main.scene.lua"},
  input = {
    left = {keyboard = "left", gamepad_button = "dpad_left"},
    right = {keyboard = "right", gamepad_button = "dpad_right"},
    rotate = {keyboard = "up", gamepad_button = "a"},
    soft_drop = {keyboard = "down", gamepad_button = "dpad_down"},
    hard_drop = {keyboard = "space", gamepad_button = "b"},
    restart = {keyboard = "r", gamepad_button = "start"},
  },
}
