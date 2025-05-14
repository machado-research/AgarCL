#!/usr/bin/env bash
set -euo pipefail

#----------------------------------------
# Detect OS and current dir
#----------------------------------------
os_name="$(uname -s)"
echo "Operating System: $os_name"
project_dir="$(pwd)"

#----------------------------------------
# Core environment exports
#----------------------------------------
# Your requested EGL_PLATFORM
export EGL_PLATFORM=surfaceless
# Prepare include/lib paths (may get prepended below)
export CPLUS_INCLUDE_PATH="${CPLUS_INCLUDE_PATH:-}"
export LIBRARY_PATH="${LIBRARY_PATH:-}"

#----------------------------------------
# macOS (Darwin)
#----------------------------------------
if [[ "$os_name" == "Darwin" ]]; then
  echo "==> macOS detected: using Homebrew"

  # 1) Ensure Homebrew is installed
  if ! command -v brew &>/dev/null; then
    echo "Homebrew not found → installing…"
    /bin/bash -c \
      "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  fi

  # 2) Core packages
  brew update
  pkgs=(cmake glm glfw cxxopts pybind11)
  for p in "${pkgs[@]}"; do
    if brew list "$p" &>/dev/null; then
      echo "$p already installed"
    else
      echo "Installing $p…"
      brew install "$p"
    fi
  done

  # 3) Shell init updates (~/.zshrc by default)
  rc="$HOME/.zshrc"
  touch "$rc"
  # Export include/lib paths and CXX
  grep -q "^export CPLUS_INCLUDE_PATH=" "$rc" \
    || echo "export CPLUS_INCLUDE_PATH=\$(brew --prefix)/include:\$CPLUS_INCLUDE_PATH" >> "$rc"
  grep -q "^export LIBRARY_PATH=" "$rc" \
    || echo "export LIBRARY_PATH=\$(brew --prefix)/lib:\$LIBRARY_PATH" >> "$rc"
  # Also set CXX to clang++
  grep -q "^export CXX=" "$rc" \
    || echo "export CXX=$(which clang++)" >> "$rc"

  echo "Sourcing $rc…"
  # shellcheck disable=SC1090
  source "$rc"

#----------------------------------------
# Linux (Debian/Ubuntu)
#----------------------------------------
elif [[ "$os_name" == "Linux" ]]; then
  echo "==> Linux detected: using apt"

  sudo apt-get update
  sudo apt-get install -y \
    build-essential cmake git clang \
    libglm-dev libglfw3-dev libgtest-dev \
    libglobjects-dev libgl1-mesa-dev libglu1-mesa-dev

  # Export CXX here and in bashrc
  export CXX=clang++
  rc="$HOME/.bashrc"
  touch "$rc"
  # Add include/lib paths
  line1="export CPLUS_INCLUDE_PATH=/usr/local/include:\$CPLUS_INCLUDE_PATH"
  grep -qF "$line1" "$rc" || echo "$line1" >> "$rc"
  line2="export LIBRARY_PATH=/usr/local/lib:\$LIBRARY_PATH"
  grep -qF "$line2" "$rc" || echo "$line2" >> "$rc"
  # Add CXX export
  line3="export CXX=clang++"
  grep -qF "$line3" "$rc" || echo "$line3" >> "$rc"

  # Build & install GLM from source (if needed)
  if [ ! -d build/glm ]; then
    git clone https://github.com/g-truc/glm build/glm
    mkdir -p build/glm/build
    cmake -DGLM_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF \
      -B build/glm/build build/glm
    cmake --build build/glm/build --target install
  fi

#   # Build & install Cxxopts from source
#   if [ ! -d build/cxxopts ]; then
#     git clone https://github.com/jarro2783/cxxopts.git build/cxxopts
#     mkdir -p build/cxxopts/build
#     cmake -B build/cxxopts/build build/cxxopts
#     cmake --build build/cxxopts/build --target install
#   fi

  echo "Sourcing $rc…"
  # shellcheck disable=SC1090
  source "$rc"

else
  echo "Unsupported OS: $os_name"
  exit 1
fi

echo "✅ All done! You can now build your project in $project_dir"
