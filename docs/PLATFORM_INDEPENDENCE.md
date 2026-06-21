# Platform Independence

The application supports macOS and Windows from one shared C++ codebase. CMake
owns build-system differences, while platform-specific runtime behavior is kept
behind narrow interfaces. Rendering targets the common OpenGL 4.1 Core Profile
feature set.

## Supported Environment

| Area | macOS | Windows | Shared contract |
| --- | --- | --- | --- |
| Compiler | Apple Clang | Microsoft Visual C++ | C++23 without vendor extensions |
| Build | CMake with Xcode, Make, or Ninja | CMake with Visual Studio or Ninja | Target-based CMake |
| Graphics | OpenGL 4.1 Core Profile | OpenGL 4.1 Core Profile | GLSL `410 core` |
| Assets | Blender `.glb` exports | Blender `.glb` exports | Identical binary data |
| Paths | POSIX filesystem | Drive-based filesystem | `std::filesystem::path` |

Both platforms execute the same scene, simulation, animation, and rendering
code. Platform branches are reserved for unavoidable operating-system or
framework integration.

## Portable Programming Rules

- Build paths with `std::filesystem::path` and preserve the path type until an
  external API requires another representation.
- Locate runtime assets relative to the packaged executable layout.
- Use `std::chrono::steady_clock` for frame and simulation timing.
- Define binary, GPU, and serialization data with fixed-width types, explicit
  layout, and explicit byte order.
- Open files in binary mode when reading GLB files, textures, audio, or rendered
  output.
- Repository paths and asset references use lowercase snake case and remain
  unique on case-insensitive filesystems.
- Asset and configuration numbers use locale-independent parsing and formatting.

## OpenGL Compatibility Contract

Only OpenGL functionality available in 4.1 Core Profile on both platforms is
allowed. In particular:

- Use classic bind-to-edit resource functions rather than Direct State Access.
- Request the same core context version and profile on both platforms.
- Load function pointers through the university framework.
- Keep shader sources compatible with GLSL `410 core`.
- Query implementation limits where the specification permits different values.
- Use extensions only after checking availability and providing a compatible
  fallback.

Apple marks OpenGL as deprecated at the operating-system level. The
`GL_SILENCE_DEPRECATION` definition only suppresses that platform warning; it
does not change the selected API or compatibility target.

## Platform Boundaries

When shared standard-library or framework functionality is insufficient, expose
a small platform-neutral interface and place implementations in clearly named
platform source files. Preprocessor checks belong in those boundary files, not
throughout animation, simulation, asset, or renderer code.

```text
src/platform/PlatformService.h
src/platform/PlatformService_macos.cpp
src/platform/PlatformService_windows.cpp
```

Platform code must explain why the shared implementation is insufficient. Every
new branch requires verification on both targets.

## CMake Rules

- Attach include paths, compile features, definitions, libraries, and
  platform-specific options to targets.
- Keep dependency versions pinned.
- Use generator expressions or compiler checks for target-specific warnings.
- Copy runtime assets with cross-platform `cmake -E` commands.
- List application source files explicitly so that both generators build the
  same program.
- Keep compiler extensions disabled.

## Verification Matrix

Every weekly milestone is tested as an integrated executable.

| Check | macOS | Windows |
| --- | ---: | ---: |
| Configure from a clean build directory | Required | Required |
| Release build without warnings | Required | Required |
| Start from the build output directory | Required | Required |
| Start from the packaged submission directory | Required | Required |
| Load the same GLB test asset | Required | Required |
| Render the same animation clip | Required | Required |

For each milestone, record the compiler, configuration, tested commit, visible
result, and any platform-specific finding in the
[Development Log](DEVELOPMENT_LOG.md). A feature is complete only after it works
inside the application on both target platforms or has an explicitly recorded
pending platform verification.
