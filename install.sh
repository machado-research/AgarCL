os_name=$(uname -s)
echo "Operating System: $os_name"

# Check if the OS is MacOS
if [ "$os_name" == "Darwin" ]; then
    current_dir=$(pwd)
    # Step 1: Install packages
    if ! command -v brew &> /dev/null; then
        echo "Homebrew is not installed. Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    else
        echo "Homebrew is already installed."
    fi

    packages=(cmake cxxopts glm glfw mesa)
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
    
    echo "Updating include paths in $ZSHRC_PATH"
    echo "export CPLUS_INCLUDE_PATH=/usr/local/opt/glfw/include:$CPLUS_INCLUDE_PATH" #>> "$ZSHRC_PATH"
    echo "export CPLUS_INCLUDE_PATH=/usr/local/opt/cxxopts/include:$CPLUS_INCLUDE_PATH" #>> "$ZSHRC_PATH"
    echo "export CPLUS_INCLUDE_PATH=/usr/local/opt/glm/include:$CPLUS_INCLUDE_PATH" #>> "$ZSHRC_PATH"
    
    if grep -qxF "CPATH=/opt/homebrew/include" "$ZSHRC_PATH"; then
        echo "CPATH already in $ZSHRC_PATH"
    else
        echo "Include CPATH in $ZSHRC_PATH"
        echo "export CPATH=/opt/homebrew/include" # >> "$ZSHRC_PATH"
    fi

    if grep -qxF "LIBRARY_PATH=/opt/homebrew/lib" "$ZSHRC_PATH"; then
        echo "LIBRARY_PATH already in $ZSHRC_PATH"
    else
        echo "Include LIBRARY_PATH in $ZSHRC_PATH"
        echo "export LIBRARY_PATH=/opt/homebrew/lib" #>> "$ZSHRC_PATH"
    fi

    source "$ZSHRC_PATH"

    # Step 3: Install pybind11
    if ! command -v pip3 &> /dev/null; then
        echo "pip3 is not installed. Installing pip3..."
        brew install python3
    else
        echo "pip3 is already installed."
    fi

    python3 -m venv venv
    VENV_PATH="$current_dir/venv"

    # Check if the virtual environment exists
    if [ -d "$VENV_PATH" ]; then
        # Activate the virtual environment
        source "$VENV_PATH/bin/activate"
        echo "Virtual environment activated."
    else
        echo "Virtual environment not found at $VENV_PATH"
        exit 1
    fi

    pybind11_path="$current_dir/environment/pybind11"
    if [ ! -d "$pybind11_path" ]; then
        echo "No pybind11 directory found. Cloning pybind11..."
        git submodule update --init --recursive
    fi

    cd "$pybind11_path" || { echo "Error: Cannot change to directory $PROJECT_DIR"; exit 1; }

    # pip install -e .
    echo "Pybind11 installed successfully."

    # Step 4: Install GLAD
    cd ../..
    mkdir -p "$current_dir/build"
    glad_path="$current_dir/build/glad"
    if [ ! -d "$glad_path" ]; then
        echo "No glad directory found. Please install glad first in build directory."
    else
        echo "Glad directory found."

        cmake_file_path="$current_dir/agario/CMakeLists.txt"
        if [ ! -f "$cmake_file_path" ]; then
            echo "CMakeLists.txt not found."
            exit 1
        fi

        SRC_TEXT="set(EXT_SOURCE_DIR \"$current_dir/build/glad/src\")"
        INCLUDE_TEXT="set(EXT_INCLUDE_DIR \"$current_dir/build/glad/include\")"
        
        ESCAPED_INCLUDE_TEXT=$(printf '%s\n' "$INCLUDE_TEXT" | sed 's/[\/&]/\\&/g; s/\$/\\$/g')
        ESCAPED_SRC_TEXT=$(printf '%s\n' "$SRC_TEXT" | sed 's/[\/&]/\\&/g; s/\$/\\$/g')
        
        sed -i '' "111s/.*/${ESCAPED_SRC_TEXT}/" "$cmake_file_path"
        sed -i '' "112s/.*/${ESCAPED_INCLUDE_TEXT}/" "$cmake_file_path"
    fi

    # Step 5: Running the code

    python3 setup.py install
    # "$PYTHON" "$current_dir/bench/agarle_bench.py"

    # Step 6: Self-play setup
    # cd build
    # cmake -DCMAKE_BUILD_TYPE=Release ..
    # make -j 2 client agarle

# Check if the OS is Linux
elif [ "$os_name" == "Linux" ]; then
    current_dir=$(pwd)

    # Step 1: Install CMake
    sudo snap install cmake

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
    mkdir -p "$current_dir/build/cxxopts"


    # Step 4: Install required OPENGL Packages
    sudo apt-get install libgl1-mesa-dev
    sudo apt-get install libglu1-mesa-dev
    sudo apt-get install libglfw3-dev

    # Step 5: Install GLAD
    cd ..

    glad_path="$current_dir/environment/glad"
    if [ ! -d "$glad_path" ]; then
        echo "No glad directory found."
        exit 1
    else
        echo "Glad directory found."

        cmake_file_path="$current_dir/agario/CMakeLists.txt"
        if [ ! -f "$cmake_file_path" ]; then
            echo "CMakeLists.txt not found."
            exit 1
        fi

        # INCLUDE_TEXT="set(EXT_SOURCE_DIR "$current_dir/environment/glad/include")"
        SRC_TEXT="set(EXT_SOURCE_DIR \"$current_dir/environment/glad/src\")"
        INCLUDE_TEXT="set(EXT_INCLUDE_DIR \"$current_dir/environment/glad/include\")"
        
        ESCAPED_INCLUDE_TEXT=$(printf '%s\n' "$INCLUDE_TEXT" | sed 's/[\/&]/\\&/g; s/\$/\\$/g')
        ESCAPED_SRC_TEXT=$(printf '%s\n' "$SRC_TEXT" | sed 's/[\/&]/\\&/g; s/\$/\\$/g')
        
        sed -i '' "111s/.*/${ESCAPED_SRC_TEXT}/" "$cmake_file_path"
        sed -i '' "112s/.*/${ESCAPED_INCLUDE_TEXT}/" "$cmake_file_path"
    fi

    # Step 6: USE CLANG Compiler
    if command -v gcc &> /dev/null; then
        sudo apt-get remove gcc
    fi

    if ! command -v clang &> /dev/null; then
        sudo apt-get install clang
    fi
    CXX=`which clang++`

    # export CPLUS_INCLUDE_PATH=environment variables path :$CPLUS_INCLUDE_PATH

else
    echo "Unsupported Operating System: $os_name"
    exit 1
fi