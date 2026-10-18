---
name: acuppa-release-check
description: Verify a ACuppa build and its code-only sample games before claiming a production-ready release.
---

# Release check

1. Read README.md, PLAN.md, and the feature-scene coverage manifest.
2. From a clean checkout, build pinned dependencies, the static core library, runtime, editor, and exported-game target on Windows, macOS, and Ubuntu 24+.
3. Run unit, Lua integration, feature-scene, renderer capture, physics, audio-state, sanitizer, asset-error, and packaged-game tests. Record exact commands and outcomes.
4. Verify both complete sample games start, play, and exit from source and from export without editor interaction. Verify the exported runtime has no editing tools or hidden editor data.
5. Report gaps or untested platforms plainly. Release only after every required gate passes, then commit the release documentation.
