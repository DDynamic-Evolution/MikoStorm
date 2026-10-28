# Reproducible MikoStorm Windows profiles

Run these commands in **cmd.exe**. `build-windows.cmd` finds Visual Studio 2022 and loads its x64 C++ toolchain. Python, CMake, Git, autobuild and NSIS must be installed as described in building_windows.md. Set AUTOBUILD_VARIABLES_FILE if fs-build-variables is not next to this checkout.

```
build-windows.cmd avx2 build
build-windows.cmd legacy build
build-windows.cmd all package
build-windows.cmd avx2 test
```

Both profiles include FMOD, OpenSim, 3D Stream and MCP. `legacy` disables both AVX and AVX2 for viewer compilation; compatibility of third-party DLLs also depends on their own build requirements. Dependencies are pinned by autobuild.xml. The legacy build checks generated compiler projects for accidental AVX flags.

Profiles live in `build-vc170-64-avx2` and `build-vc170-64-legacy`. They are independent of copied build-vc170-64/build-vc170-64-noavx caches, which can still refer to the old checkout. Only third-party packages are reused from those directories; generated projects and CMake caches are never reused. Configure an existing profile with `build-windows.cmd avx2 configure`.

After configuration and dependency setup, the build runs the regression suite before compiling the viewer. It uses one MSBuild project worker and six C++ compiler workers, with embedded /Z7 debug information. Override _CL_ to change the compiler worker count. CMake presets supply a fixed baseline rather than inheriting LL_BUILD from another profile. Release has LL_TESTS disabled for the upstream viewer suite; the focused MikoStorm suite is a separate required step.

`package` runs the normal viewer manifest and NSIS compiler. Compiler failures are propagated. Installers, SHA-256 checksums and build metadata are collected in `dist/`. To collect an installer that was compiled separately:

```
build-windows.cmd avx2 collect
```

Some restricted shells cannot launch 32-bit NSIS (Windows error 623 / exit 9020). In that environment:

```
build-windows.cmd avx2 prepare-package
```

This prepares resources and the normal NSIS script but **does not claim an installer was built**. Open the `script` path recorded in the profile's `nsis-prepared.json` in the installed NSIS GUI and compile it. Then run `collect`. Do not install the viewer just to validate packaging.

## Focused tests and runtime checks

The core C++ regression executable tests production HTTP framing/origin helpers, task completion/cancellation/timeouts/exceptions, the grid export policy matrix, numeric audio parameters and synthetic bounded round-robin work. Additional executables compile the actual camera selector and avatar logger against small viewer-service doubles; they test late/changed skeletons, synchronized selections, popup highlights, delayed names, path changes and write failures using real temporary files. Python checks production export-write guards, removed exporter/menu bindings, shared camera wiring, XML and required build features. These checks do not replace an in-world test.

Before publishing, verify camera selection after login/avatar changes, exports of permitted and denied inventory on the intended grids, MCP shutdown during a request, avatar logging with a changed/locked path, and 3D audio on a dense region. Profile `3D Stream Discovery` in the viewer's performance tools; enable `--logdebug Stream3D` for examined objects, request counts and elapsed scan time. Discovery has cooperative caps (512 candidates/2ms, structures 128/1ms); a single operation is completed before yielding, so these are not hard real-time guarantees.
