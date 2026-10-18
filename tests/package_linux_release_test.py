"""Release archives work after extraction, without the source checkout."""

from pathlib import Path
import os
import shutil
import hashlib
import subprocess
import sys
import tarfile


script = Path(sys.argv[1]).resolve()
cli = Path(sys.argv[2]).resolve()
work = Path(sys.argv[3]).resolve()
shutil.rmtree(work, ignore_errors=True)
work.mkdir(parents=True, exist_ok=True)
release = work / "release"
result = subprocess.run(["bash", str(script), str(release), str(cli.parent)],
                        capture_output=True, text=True, timeout=60)
assert result.returncode == 0, result.stdout + result.stderr
archives = sorted(release.glob("*.tar.gz"))
assert len(archives) == 4, archives
verified = subprocess.run(["sha256sum", "-c", "SHA256SUMS"], cwd=release,
                          capture_output=True, text=True, timeout=10)
assert verified.returncode == 0, verified.stdout + verified.stderr

unpacked = work / "unpacked"
unpacked.mkdir()
packages = {}
for archive in archives:
    with tarfile.open(archive, "r:gz") as content:
        names = content.getnames()
        assert all(name.startswith(archive.name.removesuffix(".tar.gz") + "/")
                   or name == archive.name.removesuffix(".tar.gz") for name in names)
        content.extractall(unpacked, filter="data")
    packages[archive.name.removesuffix(".tar.gz").split("-linux-")[1]] = \
        unpacked / archive.name.removesuffix(".tar.gz")

engine = next(path for name, path in packages.items() if name.endswith("-engine"))
player_games = [path for name, path in packages.items() if not name.endswith("-engine")]
assert len(player_games) == 3
assert (engine / "acuppa-editor").is_file()
assert (engine / "acuppa-player").is_file()
assert (engine / "RELEASE.md").is_file()
version = subprocess.run([str(engine / "acuppa"), "--version"],
                         capture_output=True, text=True, timeout=10)
assert version.returncode == 0 and "ACuppa Engine" in version.stdout \
    and "Brew games, one idea at a time." in version.stdout, version.stderr
environment = os.environ.copy()
environment["XDG_DATA_HOME"] = str(work / "user_data")
for game in player_games:
    assert not (game / "acuppa-editor").exists()
    assert not list(game.rglob("*.py")), game
    assert (game / "licenses/lua.html").is_file()
    checked = subprocess.run([str(game / "acuppa-game"), "--check"],
                             env=environment, capture_output=True, text=True, timeout=30)
    assert checked.returncode == 0, checked.stdout + checked.stderr

source = next(game for game in player_games if game.name.endswith("-falling_blocks"))
rebuilt = work / "rebuilt"
build = subprocess.run([str(engine / "acuppa"), "build", str(source / "game"),
                        str(rebuilt)], env=environment, capture_output=True, text=True,
                       timeout=30)
assert build.returncode == 0, build.stdout + build.stderr
checked = subprocess.run([str(rebuilt / "acuppa-game"), "--check"],
                         env=environment, capture_output=True, text=True, timeout=20)
assert checked.returncode == 0, checked.stdout + checked.stderr

again = subprocess.run(["bash", str(script), str(release), str(cli.parent)],
                       capture_output=True, text=True, timeout=10)
assert again.returncode != 0 and "already exists" in again.stderr
second = work / "release_again"
again = subprocess.run(["bash", str(script), str(second), str(cli.parent)],
                       capture_output=True, text=True, timeout=60)
assert again.returncode == 0, again.stdout + again.stderr
for archive in archives:
    duplicate = second / archive.name
    assert hashlib.sha256(archive.read_bytes()).digest() == \
           hashlib.sha256(duplicate.read_bytes()).digest(), archive.name
