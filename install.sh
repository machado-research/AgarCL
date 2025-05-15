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
      echo "${p} already installed"
    else
      echo "Installing ${p}…"
      brew install "$p"
    fi
  done

  # 3) Shell init updates (~/.zshrc by default)
  env_file="$HOME/.agarcl_env"
  touch "$env_file"
  # Export include/lib paths and CXX
  grep -q "^export CPLUS_INCLUDE_PATH=" "$env_file" \
    || echo "export CPLUS_INCLUDE_PATH=\$(brew --prefix)/include:\$CPLUS_INCLUDE_PATH" >> "$env_file"
  grep -q "^export LIBRARY_PATH=" "$env_file" \
    || echo "export LIBRARY_PATH=\$(brew --prefix)/lib:\$LIBRARY_PATH" >> "$env_file"
  # Also set CXX to clang++
  grep -q "^export CXX=" "$env_file" \
    || echo "export CXX=$(which clang++)" >> "$env_file"

  echo "Sourcing ${env_file}…"
  # shellcheck disable=SC1090
  source "$env_file"

  echo "Your project env has been loaded."
  echo ""
  echo "👉 To auto-load in every new shell, add this line to your ~/.zshrc:"
  echo ""
  echo "     source ~/.agarcl_env"
  echo ""
  echo "👉 Or, for per-project activation, install direnv and in your project root:"
  echo ""
  echo "     echo 'source ~/.agarcl_env' > .envrc"
  echo "     direnv allow"
  echo ""
  echo "  Then your env is loaded whenever you cd into the project."
  echo ""
  echo "👉 If you only want to load your project env one time (i.e. in the current shell) and not have it re-appear in every new terminal, just run:"
  echo ""
  echo "    source ~/.agarcl_env"

#----------------------------------------
# Linux (Debian/Ubuntu)
#----------------------------------------
elif [[ "$os_name" == "Linux" ]]; then
  echo "==> Linux detected: using apt"

  sudo apt-get update
  sudo apt upgrade -y

  sudo apt-get install -y \
    build-essential cmake git clang \
    libglm-dev libglfw3-dev libgtest-dev \
    libglobjects-dev libgl1-mesa-dev libglu1-mesa-dev \
    libegl1-mesa-dev

      # Build & install GLM from source (if needed)
  if [ ! -d build/glm ]; then
    git clone https://github.com/g-truc/glm build/glm
    mkdir -p build/glm/build
    cmake -DGLM_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF \
      -B build/glm/build build/glm
    cmake --build build/glm/build --target install
  fi

  # Install clang++ if it does not exist
  if ! command -v clang++ &>/dev/null; then
    echo "clang++ not found → installing…"
    sudo apt-get install -y clang
  fi
    # Export CXX here and in bashrc
    export CXX=clang++
    rc="$HOME/.bashrc"
    touch "$rc"

    # Ensure PS1 is set to avoid unbound variable errors
    if [[ -z "${PS1+x}" ]]; then
      echo "export PS1=''" >> "$rc"
      export PS1=''
    fi
    echo "export CXX=$CXX" >> "$rc"
    echo "[[ -z \"\${PS1+x}\" ]] && export PS1=''" >> "$rc"
    # Add include/lib paths
    line1="export CPLUS_INCLUDE_PATH=/usr/local/include:\$CPLUS_INCLUDE_PATH"
    grep -qF "$line1" "$rc" || echo "$line1" >> "$rc"
    line2="export LIBRARY_PATH=/usr/local/lib:\$LIBRARY_PATH"
    grep -qF "$line2" "$rc" || echo "$line2" >> "$rc"
    # Add CXX export
    line3="export CXX=clang++"
    grep -qF "$line3" "$rc" || echo "$line3" >> "$rc"
    # Add EGL path
    line4="export EGL_PLATFORM=surfaceless"
    grep -qF "$line4" "$rc" || echo "$line4" >> "$rc"
    # Add PATH export for EGL
    line5="export PATH=\$PATH:/usr/include/EGL"
    grep -qF "$line5" "$rc" || echo "$line5" >> "$rc"

    # Install Python 3.11 if it does not exist
    if ! python3.11 --version &>/dev/null; then
      echo "Python 3.11 not found → installing…"
      sudo apt-get update
      sudo apt-get install -y software-properties-common
      sudo add-apt-repository -y ppa:deadsnakes/ppa
      sudo apt-get update
      sudo apt-get install -y python3.11 python3.11-distutils
    fi

    # Ensure pip is installed for Python 3.11
    if ! python3.11 -m pip --version &>/dev/null; then
      echo "pip for Python 3.11 not found → installing…"
      curl -sS https://bootstrap.pypa.io/get-pip.py | python3.11
    fi

    # Install Python dependencies
    echo "Installing Python dependencies…"
    python3.11 -m pip install --upgrade pip setuptools wheel
    python3.11 -m pip install -r "$project_dir/requirements.txt"

  echo "Sourcing $rc…"
  # shellcheck disable=SC1090
  source "$rc"

  echo "✅ All done! You can now build your project in $project_dir"

else
  echo "Unsupported OS: $os_name"
  exit 1
fi
