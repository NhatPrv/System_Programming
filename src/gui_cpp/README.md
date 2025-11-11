This folder contains a minimal skeleton to build a Dear ImGui + GLFW + GLAD
application that reads the project's POSIX shared memory and provides simple
Start/Stop/Cleanup buttons.

Prerequisites (Linux):
- g++ or clang++ supporting C++17
- CMake >= 3.10
- libglfw3-dev (or system-provided glfw3)
- X11 development packages (libx11-dev), GL development (libgl1-mesa-dev)

Setup steps:
1) You can run the helper script to install system dependencies and fetch ImGui/GLAD:

   cd src/gui_cpp
   ./install_deps.sh

   The script will try to detect apt/dnf and install packages via sudo, then clone ImGui
   into `../external/imgui` and `glad` into `vendor/glad` (if not already present).

2) If you already have GLAD (you mentioned you downloaded it), copy the generated files into:
   - `vendor/glad/src/glad.c`
   - `vendor/glad/include/glad/glad.h` and related include files

3) Build with CMake:
  mkdir -p build && cd build
  cmake ..
  cmake --build .

4) Run the program from the `src` folder so it can find `./writer`, `./reader`, and `./cleanup`:
  ../build/shm_imgui

Notes:
- The example uses system `./writer` and `./reader` binaries via `system()` calls.
  For demo use, run the program in the project's `src` directory where those
  binaries exist.
- The code reads the shared memory in read-only mode to display the ring buffer
  contents. It assumes the `Shared` struct is defined in `../shared.h`.
- You may need to install additional dev packages: `libglfw3-dev libx11-dev libglu1-mesa-dev`

If you want, I can try to auto-generate the external/imgui folder and copy your
GLAD files into vendor/glad for you. Say yes and I'll attempt to fetch imgui
and wire everything, then run a test build.