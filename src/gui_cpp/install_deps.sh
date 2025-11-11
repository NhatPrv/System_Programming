#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)/.."
echo "Root: $ROOT_DIR"

detect_pkg_manager() {
  if command -v apt >/dev/null 2>&1; then
    echo apt
  elif command -v dnf >/dev/null 2>&1; then
    echo dnf
  else
    echo "unknown"
  fi
}

PKG_MANAGER=$(detect_pkg_manager)
echo "Detected package manager: $PKG_MANAGER"

if [ "$PKG_MANAGER" = "apt" ]; then
  sudo apt update
  sudo apt install -y build-essential cmake git pkg-config python3-pip \
    libglfw3-dev libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libglu1-mesa-dev libglew-dev
elif [ "$PKG_MANAGER" = "dnf" ]; then
  sudo dnf install -y @development-tools cmake git python3-pip pkgconfig \
    glfw-devel mesa-libGL-devel libX11-devel libXrandr-devel libXinerama-devel \
    libXcursor-devel libXi-devel libGLU-devel glew-devel
else
  echo "Unsupported package manager. Please install these packages manually:"
  echo "cmake, build tools, libglfw3-dev, libgl1-mesa-dev, libx11-dev, libglew-dev, pkg-config"
  exit 1
fi

cd "$ROOT_DIR/gui_cpp"

if [ ! -d "../external/imgui" ]; then
  echo "Cloning Dear ImGui into ../external/imgui..."
  mkdir -p ../external
  git clone https://github.com/ocornut/imgui.git ../external/imgui
else
  echo "imgui already exists at ../external/imgui"
fi

if [ ! -d "vendor/glad" ]; then
  echo "Cloning glad into vendor/glad (this provides glad.c and include tree)..."
  mkdir -p vendor
  git clone https://github.com/Dav1dde/glad.git vendor/glad
  echo "Note: if you already downloaded GLAD manually, copy glad.c into vendor/glad/src and the include/ tree into vendor/glad/include"
else
  echo "glad already present in vendor/glad"
fi

echo "Installing python dependencies (if any)..."
python3 -m pip install --user --upgrade pip

echo "Done. To build the GUI project:" 
echo "  cd gui_cpp && mkdir -p build && cd build && cmake .. && cmake --build ."
echo "Run the binary from the src directory so it can find ./writer ./reader ./cleanup binaries:" 
echo "  cd .. && ./build/shm_imgui"
