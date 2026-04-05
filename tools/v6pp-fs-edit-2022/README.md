# v6pp-fs-edit-2022

Linux-only CMake build for Unix V6++ filesystem tools.

## Targets

- filescanner
- fsedit

## Build

```bash
cmake -S . -B ../../.build-cache/v6pp-fs-edit-2022-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build ../../.build-cache/v6pp-fs-edit-2022-cmake --target filescanner fsedit --parallel
```

Binaries are generated in the parent build directory when used by the top-level project,
or in this subproject's build directory when built standalone.

Static filesystem content is stored in:

- fs/

## Note

Windows build compatibility is intentionally removed.
