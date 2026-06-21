# Code Style

This document defines the mandatory C++ and OpenGL conventions for the project.
The repository's `.clang-format`, `.editorconfig`, and editor settings enforce
the mechanical parts of this style.

## Naming and Formatting

| Element | Convention | Example |
| --- | --- | --- |
| Indentation | 2 spaces, no tabs | `if (...) {` |
| Variables | `var_name` | `triangle_count` |
| Member variables | `m_var_name` | `m_shader_program` |
| Static variables | `s_static_var` | `s_house_singleton` |
| Types and classes | `TypeName` | `MainApp` |
| Functions | `getVarName()` | `loadShader()` |
| Parameters | `parName` | `filePath` |
| Constants | `CONSTANT_VARIABLE_NAME` | `VERTEX_COUNT` |

Files use UTF-8, Unix line endings, and a final newline. Header files use
`#pragma once`. Includes are ordered from the matching project header, through
other project headers and third-party headers, to standard-library headers.

## Design Rules

- Function names are clear, descriptive, and communicate side effects.
- A class has one clear responsibility. RAII types express ownership of OpenGL
  and system resources.
- OpenGL context and resource setup are separated from per-frame rendering.
- Magic numbers are replaced with named constants when they represent a domain
  value, resource limit, binding, or configuration choice.
- Per-frame code minimizes allocations, copies, state changes, and GPU
  synchronization while remaining readable.
- Code communicates intent directly. Comments explain constraints, platform
  behavior, or non-obvious decisions rather than restating the code.

Each completed feature is connected to the running application and verified as
part of the complete data flow. Unit tests provide additional focused coverage.

## C++ Guidelines

- Use C++23 standard-library facilities when they are supported by both the
  selected Apple Clang and Microsoft Visual C++ toolchains.
- Use fixed-width integer types when data size is part of a file format or GPU
  interface.
- Use `std::span` for non-owning contiguous views and `std::string_view` for
  non-owning text views when their lifetime is unambiguous.
- Mark values and functions `const` whenever mutation is not required.
- Use `[[nodiscard]]` for results whose loss can hide an error.
- Treat warnings as defects. New code must build without warnings on the
  supported macOS and Windows configurations.

## OpenGL Guidelines

The rendering target is OpenGL 4.1 Core Profile. Use bind-to-edit OpenGL calls
for resource creation and updates:

```cpp
glBindBuffer(...);
glBufferData(...);
glBindVertexArray(...);
glEnableVertexAttribArray(...);
glVertexAttribPointer(...);
```

Direct State Access calls are outside the compatibility contract:

```cpp
glNamedBufferData(...);
glVertexArrayVertexBuffer(...);
glProgramUniform1f(...);
```

They have caused runtime crashes in the target environment.

Additional rendering rules:

- Shaders use `#version 410 core`.
- GPU object creation and destruction remain in dedicated RAII classes.
- A render pass establishes the state it depends on instead of relying on an
  undocumented previous pass.
- Attribute locations, uniform-block bindings, and texture units use named
  constants shared with the relevant renderer.
- OpenGL errors and shader compile/link failures are reported with enough
  context to identify the resource and operation.
- Platform-critical or framework-dependent code receives a concise explanation
  of the constraint it handles.

## Formatting

The formatter parses C++23 and applies the repository configuration. Shared VS
Code settings format C and C++ files automatically when they are saved. Other
editors use the same `.clang-format` file and should enable format-on-save.
