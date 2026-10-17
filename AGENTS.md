# ThreePoint development guidelines

- [README.md](README.md) is the user-provided source of truth. [PLAN.md](PLAN.md) is an implementation sequence. If they disagree, follow README and correct PLAN.
- The engine is **3D** and currently targets Linux (Ubuntu 24+). The user's later platform update removes Windows and macOS gates. The existing SDL3/2D executable is prototype work, not a completed 3D milestone.
- The project name is already **ThreePoint**. Work autonomously and ask questions only when a necessary decision cannot be made from the brief or repository.
- Do not inspect files outside this working directory or use other code on this computer as a reference. Keep downloaded tools and pinned dependencies inside ignored project-local directories.
- Use C++/CMake for a static core library, GLFW and nvrhi with Vulkan as the primary renderer, glm for math, miniaudio for audio, Lua for behavior, and a pinned 3D physics library. Record versions and platform requirements.
- Keep complete games authorable from source and CLI alone. Editor changes must write the same scene/project files, and the exported runtime must not require editor state or clicks.
- Implement one small, independently testable phase at a time. Add unit tests, Lua integration tests, and an entry in the all-feature test scene for every new component or public API. Test failure paths as well as the normal path.
- Run relevant automated tests before calling a phase complete. Do not claim Linux or graphical behavior passed unless actually tested with a real render capture or live window check.
- For a windowed source scene, also run its interactive launch without `--frames`; fixed-frame captures can miss first-frame callback timing.
- Update README/PLAN status honestly and commit each completed step. Leave user-created untracked projects untouched.
- Use [feature-slice](project_skills/feature-slice/SKILL.md) for implementation phases and [release-check](project_skills/release-check/SKILL.md) for final validation.
