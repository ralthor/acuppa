return {
  api = 1,
  title = "Garage Run",
  window = {width = 1280, height = 720},
  first_scene = "course",
  scenes = {course = "scenes/course.scene.lua"},
  input = {
    throttle = {keyboard = "w", gamepad_axis = "right_trigger"},
    reverse = {keyboard = "s", gamepad_axis = "left_trigger"},
    left = {keyboard = "a", gamepad_button = "dpad_left"},
    right = {keyboard = "d", gamepad_button = "dpad_right"},
    steer = {gamepad_axis = "left_x"},
    brake = {keyboard = "space", gamepad_button = "a"},
    restart = {keyboard = "r", gamepad_button = "back"},
    exit = {keyboard = "escape", gamepad_button = "start"},
  },
}
