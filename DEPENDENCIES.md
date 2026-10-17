# Dependency record

The core and editor targets use C++17 and its standard library. The runtime also uses GLFW, Vulkan, nvrhi, glm, and Lua. CMake 3.25 or newer is required; the local bootstrap installs CMake **4.4.4**, Zig **0.17.0**, and Ninja **1.13.2** in ignored `.tools/`.

| Product dependency | Pinned source or local package | Role |
| --- | --- | --- |
| nvrhi (MIT) | `NVIDIA-RTX/NVRHI` commit `6b96fb03e07539f08327aea76c56d55f1de9d906` | Static Vulkan rendering backend. |
| Vulkan-Headers (Apache 2.0) | `KhronosGroup/Vulkan-Headers` v1.4.352, commit `015e25c3c91b70eb1a754d36fb14c4ba6ad9b0b9` | Vulkan C/C++ API headers matching nvrhi. |
| GLFW (zlib/libpng) | Local Ubuntu package `libglfw3` and `libglfw3-dev` **3.4-4** | Linux window, input events, and Vulkan surface. |
| Vulkan loader | Local Ubuntu packages `libvulkan1` and `libvulkan-dev` **1.4.341.0-1** | Loader library; an installed Vulkan driver is also needed at runtime. |
| Vulkan validation layer | Local Ubuntu package `vulkan-validationlayers` **1.4.341.0-1** | Checks Vulkan API use during GPU tests. |
| glm (MIT) | `g-truc/glm` 1.0.1, commit `0af55ccecd98d4e5a8d1fad7de25ba429d60e863` | Perspective camera and 3D model transforms. |
| cgltf (MIT) | `jkuhlmann/cgltf` v1.15, commit `360db1a95480fe102ae9c69b27c5d101167ff5ba` | glTF 2.0 JSON/GLB parsing; ThreePoint limits file sizes and validates supported accessors before reading them. |
| stb_image (public domain/MIT) | `nothings/stb` commit `2c980bb59875b0d32144a71867fbdebb2f77cd20` | Project-local PNG/JPEG decoding for glTF textures; source and decoded dimensions are bounded. |
| glslang (BSD-3-Clause) | Local Ubuntu package `glslang-tools` **16.2.0-2** | Build-time GLSL to SPIR-V compiler; generated shaders are embedded in the runtime. |
| Lua (MIT) | Official `lua-5.4.8.tar.gz`, SHA-256 `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae` | Statically linked 3D game scripting runtime. |
| Jolt Physics (MIT) | `jrouwe/JoltPhysics` v5.6.0, commit `e77f175595e64cb44218cc9d9d56fc365ad0e36a` | Static 3D rigid-body simulation; box/sphere colliders and sensors. |

CMake fetches the pinned nvrhi, Vulkan-Headers, glm, cgltf, stb, and Jolt commits and checks the Lua archive hash for a clean checkout. `scripts/bootstrap-local.sh` downloads those sources and exact Ubuntu packages into ignored `.tools/` and supplies them to CMake. Jolt is built with its CMake `Build` subdirectory as a static library, cross-platform deterministic arithmetic, baseline SSE2 on x86-64, and without Jolt's separate graphics compute backends, installer, or object stream. ThreePoint currently drives Jolt on one thread at 60 Hz. The Ubuntu 24.04 CI runner uses its distribution GLFW/Vulkan/glslang packages and runs the GPU test through Xvfb and Mesa; that hosted job has not run yet.

The older optional 2D prototype is built only with `THREEPOINT_BUILD_2D_PROTOTYPE=ON`. The local bootstrap pins its Ubuntu packages to SDL3 **3.4.2+ds-1ubuntu1** and Lua **5.4.8-1build1**. It is retained for regression checks, not used as the 3D renderer.

Pin the exact source revision or release and record its license, retrieval method, and platform build options when miniaudio enters the product in Phase 25. Keep downloads and build tools within this repository's ignored directories for local development.
