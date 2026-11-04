return {
  api = 1,
  id = "acuppa-neat-lander-viewer",
  title = "NEAT Lander",
  window = {width = 1280, height = 720},
  first_scene = "main",
  scenes = {main = "scenes/main.scene.lua"},
  input = {
    toggle_net = {keyboard = "n", gamepad_button = "y"},
    previous = {keyboard = "left", gamepad_button = "dpad_left"},
    next = {keyboard = "right", gamepad_button = "dpad_right"},
    live = {keyboard = "l", gamepad_button = "x"},
    replay = {keyboard = "r", gamepad_button = "back"},
    speed = {keyboard = "f", gamepad_button = "b"},
    exit = {keyboard = "escape", gamepad_button = "start"},
  },
}
