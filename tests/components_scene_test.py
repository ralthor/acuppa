"""Invalid Lua component operations must fail before a GPU window is needed."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()
fixture = Path(sys.argv[2]).resolve()


with tempfile.TemporaryDirectory() as directory:
    project = Path(directory) / "project"
    shutil.copytree(fixture, project)
    script = project / "scripts/main.lua"
    cases = [
        ('tp.component_set(id, "unknown", {})', "unknown component kind"),
        ('tp.component_set(id, "transform", {scale={0,1,1}})', "scale"),
        ('tp.component_set(id, "transform", {parent=id})', "parent"),
        ('tp.component_set(id, "camera", {fov=0})', "camera"),
        ('tp.component_set(id, "light", {intensity=-1})', "light"),
        ('tp.component_set(id, "light", {shadows="yes"})', "boolean"),
        ('tp.component_set(id, "light", {shadow_range=0})', "shadow range"),
        ('tp.component_set(id, "mesh", {mesh="missing"})', "mesh path must end"),
        ('tp.component_set(id, "mesh", {visible="yes"})', "boolean"),
        ('tp.component_set(id, "script", {path="../outside.lua"})', "outside project"),
        ('tp.entity_destroy(999)', "unknown entity ID"),
    ]
    for expression, expected in cases:
        script.write_text('local tp=require("acuppa")\n'
                          'return {start=function() local id=tp.entity_create() '
                          + expression + ' end, fixed_update=function() end, '
                          'update=function() end}\n')
        result = subprocess.run([str(binary), "run", str(project), "--frames", "1"],
                                capture_output=True, text=True, timeout=10)
        assert result.returncode != 0 and expected in result.stderr, \
            f"{expression}: {result.stdout}{result.stderr}"

print("Invalid Lua component inputs and entity IDs were rejected")
