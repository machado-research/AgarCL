os_name=$(uname -s)
echo "Operating System: $os_name"

current_dir=$(pwd)

# Check if the OS is MacOS
if [ "$os_name" == "Darwin" ]; then
    arch=$(uname -m)
    echo "Architecture: $arch"

    # Step 1: Install packages
    if ! command -v brew &> /dev/null; then
        echo "Homebrew is not installed. Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    else
        echo "Homebrew is already installed."
    fi

    packages=(cmake cxxopts glm glfw)
    for package in "${packages[@]}"; do
        if brew list "$package" &>/dev/null; then
            echo "$package is already installed."
        else
            echo "Installing $package..."
            brew install "$package"
        fi
    done

    #Step 2: Update Include Paths
    ZSHRC_PATH="$HOME/.zshrc"
    if [ ! -f "$ZSHRC_PATH" ]; then
        echo "$ZSHRC_PATH not found. Creating $ZSHRC_PATH"
        touch "$ZSHRC_PATH"
    fi
    
    if ! grep -q "CPLUS_INCLUDE_PATH" "$ZSHRC_PATH"; then
        echo "Updating include paths in $ZSHRC_PATH"

        echo 'export CPLUS_INCLUDE_PATH=/usr/local/opt/glfw/include:$CPLUS_INCLUDE_PATH' >> "$ZSHRC_PATH"
        echo 'export CPLUS_INCLUDE_PATH=/usr/local/opt/cxxopts/include:$CPLUS_INCLUDE_PATH' >> "$ZSHRC_PATH"
        echo 'export CPLUS_INCLUDE_PATH=/usr/local/opt/glm/include:$CPLUS_INCLUDE_PATH' >> "$ZSHRC_PATH"
    fi
    
    if grep -q "CPATH" "$ZSHRC_PATH"; then
        echo "CPATH already in $ZSHRC_PATH"
    else
        echo "Include CPATH in $ZSHRC_PATH"
        if [ "$arch" == "x86_64" ]; then
            echo "export CPATH=/usr/local/include" >> "$ZSHRC_PATH"
        else
            echo "export CPATH=/opt/homebrew/include" >> "$ZSHRC_PATH"
        fi
    fi

    if grep -q "LIBRARY_PATH" "$ZSHRC_PATH"; then
        echo "LIBRARY_PATH already in $ZSHRC_PATH"
    else
        echo "Include LIBRARY_PATH in $ZSHRC_PATH"
        if [ "$arch" == "x86_64" ]; then
            echo "export LIBRARY_PATH=/usr/local/lib" >> "$ZSHRC_PATH"
        else
            echo "export LIBRARY_PATH=/opt/homebrew/lib" >> "$ZSHRC_PATH"
        fi
    fi

    source "$ZSHRC_PATH"
    # Step 3: Install pybind11
    if ! command -v pip3 &> /dev/null; then
        echo "pip3 is not installed. Installing pip3..."
        brew install python3
    else
        echo "pip3 is already installed."
    fi

    pybind11_path="$current_dir/environment/pybind11"
    if [ ! -d "$pybind11_path" ]; then
        echo "No pybind11 directory found. Cloning pybind11..."
        git submodule update --init --recursive
    fi

    cd "$pybind11_path" || { echo "Error: Cannot change to directory $PROJECT_DIR"; exit 1; }

    pip install -e .
    echo "Pybind11 installed successfully."

    # Step 4: Running the code
    python3 setup.py install

# Check if the OS is Linux
elif [ "$os_name" == "Linux" ]; then
    # Step 1: Install CMake
    sudo apt-get install libglm-dev 
    sudo apt-get install libglobjects-dev
    sudo apt-get install cmake
    sudo apt-get install libgtest-dev
    
    # Step 2: Install GLM
    mkdir -p "$current_dir/build"
    cd "$current_dir/build"

    git clone https://github.com/g-truc/glm
    cmake \
        -DGLM_BUILD_TESTS=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -B build .
    cmake --build build -- all
    cmake --build build -- install

    # Step 3: Install Cxxopts
    git clone https://github.com/jarro2783/cxxopts.git
    cmake ${current_dir}/build/cxxopts
    make

    # Step 4: Install required packages
    sudo apt-get install libgl1-mesa-dev
    sudo apt-get install libglu1-mesa-dev
    sudo apt-get install libglfw3-dev
    sudo apt-get install freeglut3-dev
    sudo apt install libstdc++-12-dev
    
    # Step 5: USE CLANG Compiler
    if command -v gcc &> /dev/null; then
        sudo apt-get remove gcc
    fi

    if ! command -v clang &> /dev/null; then
        sudo apt-get install clang
    fi
    CXX=`which clang++`

    # Step 6: Exporting right paths to bashrc
    if ! grep -q "CPLUS_INCLUDE_PATH" "$HOME/.bashrc"; then
        echo "Updating include paths in $HOME/.bashrc"

        echo 'export CPLUS_INCLUDE_PATH=/usr/local/opt/glfw/include:$CPLUS_INCLUDE_PATH' >> "$HOME/.bashrc"
        echo 'export CPLUS_INCLUDE_PATH=/usr/local/opt/cxxopts/include:$CPLUS_INCLUDE_PATH' >> "$HOME/.bashrc"
        echo 'export CPLUS_INCLUDE_PATH=/usr/local/opt/glm/include:$CPLUS_INCLUDE_PATH' >> "$HOME/.bashrc"
    fi

    # Step 7: Benchmarking
    cd build
    git clone https://github.com/google/benchmark.git
    cd benchmark
    cmake -E make_directory "build"
    cmake -E chdir "build" cmake -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on -DCMAKE_BUILD_TYPE=Release ../
    cmake --build "build" --config Release
    sudo cmake --build "build" --config Release --target install
    
    # Step 8: Running the code
    python3 setup.py install
    
else
    echo "Unsupported Operating System: $os_name"
    exit 1
fi