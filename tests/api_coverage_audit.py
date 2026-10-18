"""Every registered Lua API needs a live example and an invalid-call fixture."""

from pathlib import Path
import re
import sys


root = Path(sys.argv[1]).resolve()
registrations = (root / "src/core/lua_project.cpp").read_text()
api = set(re.findall(r'add_function\("([a-z_]+)"', registrations))
assert api, "no public Lua calls found"
scripts = root / "tests/feature_scene/scripts"
positive = "\n".join(path.read_text() for path in scripts.glob("*.lua")
                     if path.name != "api_contract.lua")
invalid = (scripts / "api_contract.lua").read_text()
assert 'api_contract = "scenes/api_contract.scene.lua"' in (
    root / "tests/feature_scene/game.lua").read_text()
assert 'tp.quit()' in invalid
examples = {name for name in api if re.search(
    r'\b(?:tp|threepoint)\.' + name + r'\s*\(', positive)} | {"quit"}
failures = {name for name in api if re.search(
    r'\b' + name + r'\s*=\s*function\s*\(\s*\)\s*tp\.' + name + r'\s*\(', invalid)}
assert examples == api, f"Lua calls without a feature-scene example: {sorted(api - examples)}"
assert failures == api, f"Lua calls without an invalid-input check: {sorted(api - failures)}"
print(f"Audited {len(api)} registered Lua calls against feature-scene examples and errors")
