Shattered Realities

## Building

Requires CMake 3.16+, Ninja, and a C++20 compiler (tested with MinGW-w64 g++).

**VS Code:** install the C/C++ and CMake Tools extensions, open the folder, and pick the
`MinGW Debug` preset. Press F7 to build, F5 to debug, Shift+F5 to run.

**Command line:**

```sh
cmake --preset mingw-debug
cmake --build --preset mingw-debug
./build/debug/shattered_realities
```

Rendered images are written to `output/` (or to a directory passed as the first argument).
