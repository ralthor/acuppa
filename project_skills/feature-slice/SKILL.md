---
name: acuppa-feature-slice
description: Implement one small, fully tested ACuppa 3D engine milestone while preserving the source-only game workflow.
---

# Feature slice

1. Read README.md and the next unmet PLAN.md phase; list the public behavior and its testable check.
2. Inspect only this repository. Identify the runtime, editor, Lua, asset, and export paths the change touches.
3. Implement the smallest complete version that passes the phase check. Keep shared engine behavior in the core library and game data in source files.
4. Add unit tests for core logic, Lua integration tests for the public API, invalid-input tests, and a feature-scene case. A new component or API without a feature-scene case is incomplete.
5. Build and run the available automated checks. State which platform checks were actually run; keep missing platform checks open.
6. Update the plan's status and API documentation, then commit the finished step. Preserve unrelated user files.
