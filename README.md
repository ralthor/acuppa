# ThreePoint game engine

**First release:** a 2D desktop engine with a code-first project format. Games use Lua for scenes, objects, behavior, UI, and settings. A small C++ runtime uses SDL3 for the window, input, graphics, and audio; CMake builds it. We will pin dependency versions when implementation starts. Lua is an established full programming language, so the work is to make the *engine API* complete enough to build whole games, not to invent a new language. ([Lua](https://www.lua.org/manual/), [SDL3](https://wiki.libsdl.org/SDL3/FrontPage), [CMake](https://cmake.org/cmake/help/latest/guide/tutorial/))

**Rule for every milestone:** a game must be runnable from source with `threepoint run path/to/game`. Structured scenes live in readable Lua tables, with handwritten behavior in separate Lua files. The editor can change those tables, but must never hold game data that scripts cannot express.

| Step | Build | Check before moving on |
| --- | --- | --- |
| 1 | Define the project files, Lua entry point, engine commands, and an API checklist covering every planned game capability. | Sketch both sample games using only those APIs; record any missing capability. |
| 2 | Install and document the compiler, CMake, and required libraries. | A fresh setup can build a minimal executable. |
| 3 | Add `threepoint init`, `threepoint run`, and clear project/error messages. | A new project starts and exits from the command line. |
| 4 | Embed Lua with `start`, `fixed_update`, `update`, and `draw` callbacks. | A script runs each callback; syntax and runtime errors identify the file and line. |
| 5 | Add a window, timing, and script-controlled shapes, built-in text, and camera. | A Lua-only scene draws and moves a labeled rectangle. |
| 6 | Expose keyboard, mouse, and controller input. | A small script responds to injected events from each device in a repeatable test. |
| 7 | Add script-defined scenes and object creation, removal, and ordering. | A script spawns objects and switches scenes without editor data. |
| 8 | Add fixed-step 2D movement and collision events. | A scripted ball bounces; automated cases check contact and separation. |
| 9 | Add images, fonts, sound, and simple generated visuals/audio. | Lua loads file assets and produces a visible shape and audible tone without external assets. |
| 10 | Add script-defined UI, timers, and animation. | A menu button starts play; an animated object and timer work from Lua. |
| 11 | Add save/load and a headless script test runner. | A test saves state, reloads it, and checks the same result without opening a window. |
| 12 | Build sample game 1: Pong, entirely from Lua and engine built-ins. | A player can finish a match using `threepoint run`; its rules have automated tests. |
| 13 | Build sample game 2: a small top-down adventure, entirely from Lua and engine built-ins. | It exercises scenes, UI, collision, audio, animation, and save/load from the command line. |
| 14 | Add an interactive editor that can open a project and visually create or change scenes and objects. | An edit is saved to Lua scene tables and appears in `threepoint run`. |
| 15 | Add editor preview and source round-trip checks. | Editing a scene table by hand, reopening it in the editor, and saving preserves game behavior; no feature requires clicking. |
| 16 | Add `threepoint test` and `threepoint build` for standalone games, then document the workflow. | From a clean setup, both samples pass tests and produce playable builds without opening the editor. |

**Release gate:** document every public Lua API with a runnable example, and check each supported 2D capability from Lua. Both complete sample games must use source files alone, and every game-changing editor action must have an equivalent code path. Expand the API whenever either sample exposes a gap.

## Phase 1: project and scripting contract

This section is the design for the first implementation phase. The commands and Lua functions below are **specified, not implemented yet**. The sketches are small design checks, not runnable games today.

### Project files

```text
my-game/
  game.lua                 # required entry point: title, window, scenes, input actions
  scenes/*.scene.lua       # scene tables: entities, UI, background, script path
  scripts/*.lua            # handwritten game behavior and reusable modules
  assets/                  # optional images, fonts, and audio
  tests/*.test.lua         # optional automated Lua tests
```

`game.lua` returns one table with `api = 1`, `title`, `window = {width, height}`, `first_scene`, `scenes = {name = relative_path}`, and `input = {action = {bindings}}`. Every path is relative to the project. The engine loads the first scene, loads its `script`, then calls any `start()`, `fixed_update(dt)`, `update(dt)`, and `draw()` functions that script returns. `fixed_update` uses a fixed 1/60-second step; `dt` is in seconds. The engine draws scene entities first, calls `draw()`, then draws UI. Invalid paths, IDs, bindings, or Lua errors report a project-relative file and line.

Each `.scene.lua` returns a **literal data table**: `background`, `script`, `entities`, and `ui`. An entity has a unique `id`, position (`x`, `y`), optional `scale`, `depth`, `visible`, `visual`, and `collider`. Coordinates use a top-left origin and pixels; `visual` describes a rectangle, circle, or sprite. A collider is an axis-aligned rectangle with `w`, `h`, and `solid` (false means trigger). The editor changes only these literal tables and leaves handwritten scripts alone. Lua scripts may also create or change entities at runtime.

For example, these are ordinary source files in a Pong project:

```lua
-- game.lua
return {
  api = 1, title = "Pong", window = {width = 800, height = 450},
  first_scene = "play", scenes = {play = "scenes/play.scene.lua"},
  input = {
    up = {"key:w", "key:up", "pad:0:dpad_up"},
    down = {"key:s", "key:down", "pad:0:dpad_down"},
    restart = {"key:r", "pad:0:a"}
  }
}
```

```lua
-- scenes/play.scene.lua
return {
  background = "#101820", script = "scripts/play.lua",
  entities = {
    {id = "top", x = 0, y = 0, visual = {kind = "rect", w = 800, h = 12, color = "#ffffff"}, collider = {w = 800, h = 12, solid = true}},
    {id = "bottom", x = 0, y = 438, visual = {kind = "rect", w = 800, h = 12, color = "#ffffff"}, collider = {w = 800, h = 12, solid = true}},
    {id = "player", x = 25, y = 185, visual = {kind = "rect", w = 12, h = 80, color = "#ffffff"}, collider = {w = 12, h = 80, solid = true}},
    {id = "opponent", x = 763, y = 185, visual = {kind = "rect", w = 12, h = 80, color = "#ffffff"}, collider = {w = 12, h = 80, solid = true}},
    {id = "ball", x = 394, y = 219, visual = {kind = "rect", w = 12, h = 12, color = "#ffffff"}, collider = {w = 12, h = 12, solid = true}}
  },
  ui = {
    {id = "score", kind = "label", x = 370, y = 22, text = "0 : 0"},
    {id = "message", kind = "label", x = 290, y = 210, text = ""}
  }
}
```

Ordinary Lua control flow, functions, tables, math, and project-local modules are available. `require("threepoint")` supplies the engine API. Binding names use `key:`, `mouse:`, and `pad:<number>:` prefixes. Games can use engine built-in shapes, text, and tones, so sample games need no manually prepared media. Optional files in `assets/` travel with a built game.

### ThreePoint commands

| Command | Result |
| --- | --- |
| `threepoint init <directory>` | Create a minimal code-only project with `game.lua`, one scene, one script, and an example test. |
| `threepoint run <directory>` | Validate the project, then play it directly from source. |
| `threepoint test <directory>` | Run `tests/*.test.lua` headlessly with controllable input and time. |
| `threepoint build <directory> --out <directory>` | Produce a standalone playable game containing the runtime, Lua source, and referenced assets. |
| `threepoint edit <directory>` | Open the optional visual editor on those same project files. |

No command requires an editor project database or a click to finish a game.

The editor will also offer a source view for behavior scripts. Its visual controls edit only literal scene tables; handwritten Lua remains editable as text.

### Lua API checklist for the first 2D release

Each row is a planned capability; none is implemented yet. An API is complete only when it has an example and a test. Functions use the `tp` value returned by `require("threepoint")`.

| Capability | Planned Lua surface |
| --- | --- |
| App and lifecycle | Scene script callbacks above; `tp.app.quit()`; project settings in `game.lua`. |
| Scenes and entities | `tp.scene.get(id)`, `spawn(definition)`, `remove(id)`, `all(tag)`, `switch(name)`; mutable position, scale, depth, visibility, and visual fields. |
| Drawing and camera | `tp.draw.rect/circle/line/text/image(...)`, `tp.camera.set(x, y, zoom)`, `tp.camera.to_world(x, y)`; automatic drawing of scene visuals. |
| Input | Named actions in `game.lua`; `tp.input.down/pressed/released(action)`, `axis(binding)`, `pointer()`; keyboard, mouse, and controller bindings. |
| Collision and movement | `tp.physics.move(id, dx, dy)` resolves solid rectangles and returns contacts with `entity`, `nx`, and `ny`; `overlaps(id, other_id)` and `query_rect(x, y, w, h)` include triggers. |
| Assets | `tp.asset.image/font/sound(relative_path)`; missing assets produce clear errors; sprite visuals may name image frames. |
| Audio | `tp.audio.play(sound, options)`, `stop(handle)`, `tone(hz, seconds, volume)`; music uses a looping sound. |
| Animation | Sprite clips in scene data; `tp.anim.play(id, clip)`, `stop(id)`, `tween(id, numeric_field, target, seconds, options)`. |
| UI | Scene-defined labels, buttons, and text inputs; `tp.ui.clicked(id)`, `value(id)`, `set_text(id, text)`, `set_visible(id, bool)`, `focus(id)`; mouse, keyboard, and controller focus. |
| Timers | `tp.timer.after(seconds, fn)`, `every(seconds, fn)`, `cancel(handle)`; scene timers end when that scene closes. |
| Save data | `tp.save.read(slot)`, `write(slot, plain_lua_table)`, `delete(slot)`; safe, atomic writes and clear errors. |
| Testing | In `*.test.lua`: `tp.test.start(scene)`, `press(action)`, `release(action)`, `step(frames)`, `reset_save()`; tests use Lua `assert` to inspect entities, UI, and saved state. |

API rules: IDs are unique within a scene; missing IDs are errors, not silent no-ops. `tp.scene.get` returns the live mutable entity. `tp.physics.move` prevents crossing solid colliders and reports the collision normal; triggers are detected with `overlaps`. Drawing functions are used from `draw()`. UI and scene visuals render automatically. Every editor operation must map to scene data or one of these runtime calls.

### Sample game sketches using this contract

**Pong:** `game.lua` names a `play` scene and binds `up`, `down`, and `restart` to keys or controller buttons. Its scene table contains a player paddle, opponent paddle, ball, top and bottom walls, and two UI labels. The scene script can be written entirely with the proposed API:

```lua
local tp = require("threepoint")
local vx, vy, player_score, opponent_score = 220, 130, 0, 0
local finished = false

local function reset_ball()
  local ball = tp.scene.get("ball")
  ball.x, ball.y = 394, 219
  vx, vy = -vx, 130
end

return {
  fixed_update = function(dt)
    if finished then
      if tp.input.pressed("restart") then
        player_score, opponent_score, finished = 0, 0, false
        tp.ui.set_text("message", "")
        reset_ball()
      end
      return
    end

    local player, opponent, ball = tp.scene.get("player"), tp.scene.get("opponent"), tp.scene.get("ball")
    local direction = (tp.input.down("down") and 1 or 0) - (tp.input.down("up") and 1 or 0)
    player.y = math.max(12, math.min(358, player.y + direction * 300 * dt))
    local chase = math.max(-1, math.min(1, (ball.y - opponent.y - 40) / 50))
    opponent.y = math.max(12, math.min(358, opponent.y + chase * 200 * dt))

    for _, hit in ipairs(tp.physics.move("ball", vx * dt, vy * dt)) do
      if hit.nx ~= 0 then vx = -vx end
      if hit.ny ~= 0 then vy = -vy end
      tp.audio.tone(440, 0.05, 0.2)
    end
    if ball.x < 0 or ball.x > 800 then
      if ball.x < 0 then opponent_score = opponent_score + 1
      else player_score = player_score + 1 end
      tp.ui.set_text("score", string.format("%d : %d", player_score, opponent_score))
      finished = player_score == 7 or opponent_score == 7
      if finished then tp.ui.set_text("message", player_score == 7 and "You win! Press restart" or "Opponent wins! Press restart") end
      reset_ball()
    end
  end
}
```

**Top-down adventure:** `game.lua` names `room` and `ending` scenes and binds `up`, `down`, `left`, and `right`. The `room` scene table contains `hero`, solid walls, a trigger `gem`, a trigger `exit`, a `status` label, and a `save` button. The `ending` scene contains a victory label. No scene needs editor-generated state outside Lua.

```lua
local tp = require("threepoint")
local has_gem = false
local function save()
  local hero = tp.scene.get("hero")
  tp.save.write("slot1", {x = hero.x, y = hero.y, gem = has_gem})
end

return {
  start = function()
    local saved = tp.save.read("slot1") or {}
    local hero = tp.scene.get("hero")
    hero.x, hero.y, has_gem = saved.x or hero.x, saved.y or hero.y, saved.gem or false
    if has_gem then tp.scene.remove("gem")
    else tp.anim.tween("gem", "scale", 1.2, 0.4, {loop = true, yoyo = true}) end
  end,
  fixed_update = function(dt)
    local dx = (tp.input.down("right") and 1 or 0) - (tp.input.down("left") and 1 or 0)
    local dy = (tp.input.down("down") and 1 or 0) - (tp.input.down("up") and 1 or 0)
    tp.physics.move("hero", dx * 150 * dt, dy * 150 * dt)
    if not has_gem and tp.physics.overlaps("hero", "gem") then
      has_gem = true
      tp.scene.remove("gem")
      tp.scene.spawn({id = "spark", x = 200, y = 120, visual = {kind = "circle", radius = 8, color = "#ffee55"}})
      tp.audio.tone(660, 0.15, 0.3)
      tp.ui.set_text("status", "Gem found")
      tp.timer.after(1, function() tp.scene.remove("spark"); tp.ui.set_text("status", "") end)
    end
    if has_gem and tp.physics.overlaps("hero", "exit") then
      save()
      tp.scene.switch("ending")
    end
  end,
  update = function()
    if tp.ui.clicked("save") then save(); tp.ui.set_text("status", "Saved") end
  end
}
```

**Gap record:** Both sketches use only names in the checklist, so they reveal no missing Lua capability for these two games. They do expose required implementation details: collision normals for Pong; trigger overlaps, scene-scoped timers, and serializable save data for the adventure. The samples do **not** exercise file assets, text input, analog controller axes, camera conversion, or sprite clips; those need their own API examples and tests before release. If a later game needs another capability, add it to this checklist and supply a code-only example before implementing an editor control for it.

**Phase 1 check:** project files, command names, callbacks, and API names are defined here; both sample sketches use only those names; the unproven API areas are recorded above. No runtime behavior is claimed until later phases implement and test it.

## Phase 2: build bootstrap

The repository now has a minimal C++ executable that opens a Lua state and reads the linked SDL3 version. It does not open a game window yet. This verifies the compiler, CMake build, headers, and both libraries before runtime work begins.

On this Ubuntu 26.04 x86-64 machine, administrator access is unavailable to unattended commands. The tested setup keeps tools under ignored `.tools/` and build output under ignored `build/`:

```sh
bash scripts/bootstrap-local.sh
```

The script needs `uv`, `apt-get`, and `dpkg-deb` on the machine. It pins Zig 0.17.0 as the local C++ compiler, CMake 4.4.4, Ninja 1.13.2, Ubuntu SDL3 3.4.2, and Lua 5.4.8. It downloads the SDL3 and Lua development packages without installing them system-wide, then configures, builds, and runs the executable. Internet access is needed the first time. On a machine with administrator access, the conventional alternative is to install `build-essential`, `cmake`, `ninja-build`, `libsdl3-dev`, and `liblua5.4-dev` with the system package manager, then run `cmake -S . -B build -G Ninja` and `cmake --build build`.

**Step 2 check passed here:** `bash scripts/bootstrap-local.sh` completed and printed `ThreePoint bootstrap: SDL 3.4.2, Lua 5.4`. Window creation, input, and game commands remain for later steps.
