# Dependency record

The Phase 1 product targets (`threepoint_core`, `threepoint`, `threepoint_editor`) use C++17 and its standard library only. CMake 3.25 or newer is required by the project configuration; the local bootstrap installs CMake **4.4.4**, Zig **0.17.0**, and Ninja **1.13.2** in ignored `.tools/`.

The older optional 2D prototype is built only with `THREEPOINT_BUILD_2D_PROTOTYPE=ON`. The local bootstrap pins its Ubuntu packages to SDL3 **3.4.2+ds-1ubuntu1** and Lua **5.4.8-1build1**. It is retained for regression checks, not used as the 3D renderer.

Pin the exact source revision or release and record its license, retrieval method, and platform build options when each 3D dependency first enters the product: GLFW and nvrhi in Phase 2, glm in Phase 3, Jolt Physics in Phase 14, and miniaudio in Phase 16. The eventual Lua binding must also use one pinned Lua version. Keep downloads and build tools within this repository's ignored directories for local development.
