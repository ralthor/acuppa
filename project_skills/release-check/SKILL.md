---
name: acuppa-release-check
description: Verify an ACuppa build and its source-only sample games before claiming a production-ready Linux release.
---

# Release check

1. Read README.md, PLAN.md, and the feature-scene coverage manifest.
2. From a clean checkout, build pinned dependencies, the static core library, runtime, editor, and exported-game target on Ubuntu 24+.
3. Run unit, Lua integration, feature-scene, renderer capture, physics, audio-state, sanitizer, asset-error, and packaged-game tests. Record exact commands and outcomes.
4. Verify all three sample projects start, play, and exit from source and from export without editor interaction. Verify the exported runtime has no editing tools or hidden editor data.
5. Report gaps or untested platforms plainly. Release only after every required gate passes, then commit the release documentation.
