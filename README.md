# AgarCL

<div align="center">
    <img src="assets/agarcl_logo.png" alt="AgarCL logo" width="200"/>
</div>


A research platform for continual RL that allows for a progression of increasingly sophisticated behaviour.

<div align="center">
    <img src="assets/game_description.png" alt="AgarCL description" width="600"/>
</div>

**AgarCL is based on the game Agar.io.** It's a non-episodic, high-dimensional problem featuring stochastic, ever-evolving dynamics, continuous actions, and partial observability.

## Installation instructions

The recommended way to use AgarCL is within a Docker container running a Linux OS. This ensures there are no conflicts with other installed packages or platforms. This installation script will allow you to interact with AgarCL in a headless mode.

### Setting up the container
Follow these steps to set up the container:

1. **Download the Dockerfile**
   - Download the [Dockerfile.txt](https://github.com/AgarCL/AgarCL/blob/master/Dockerfile.txt).

2. **Navigate to the Directory Containing the Dockerfile**
   - Open your terminal and navigate to the folder where the `Dockerfile.txt` is located:
     ```bash
     cd /path/to/Dockerfile/directory
     ```

3. **Build the Docker Image**
   - Build the Docker image by specifying the custom Dockerfile using the `-f` flag:
     ```bash
     docker build -f Dockerfile.txt -t agarclimage .
     ```

4. **Run the Docker Container**
   - Once the image has been built, run the container:
     ```bash
     docker run --gpus all -it --name agarclcontainer agarclimage
     ```
   - This command will start the container with the name `agarclcontainer`. The `--gpus all` flag tells Docker to use all available GPUs on your host system for the container.

### Installing the AgarCL Platform

Now, let's install the platform on your system (`agarclcontainer` container):

1. **Clone the AgarCL Repository**
   - Clone the repository with the `--recursive` flag to ensure all submodules are included:
     ```bash
     git clone --recursive git@github.com:AgarCL/AgarCL.git
     ```

2. **Install the Platform**
   - Change into the `AgarCL` directory:
     ```bash
     cd AgarCL
     ```

   - Run the installation command to set up the platform:
     ```bash
     python3 setup.py install --user
     ```

   - This will install the platform in your local user environment.

#### Done!

### Installing the AgarCL Platform and benchmarking tools

1. **Clone the AgarCL-benchmark Repository**
   - Clone the repository:
     ```bash
     git clone git@github.com/AgarCL/AgarCL-benchmark.git
     ```

2. **Navigate to the AgarLE-Benchmark Directory**
   - Change into the `AgarCL-benchmark` directory:
     ```bash
     cd AgarCL-benchmark
     ```

4. **Clone the AgarCLgit  Repository**
   - Clone the `AgarCL` repository with the `--recursive` flag to ensure all submodules are included:
     ```bash
     git clone --recursive git@github.com:AgarCL/AgarCL.git
     ```

5. **Navigate to the AgarCL Directory**
   - Change into the `AgarCL` directory:
     ```bash
     cd AgarCL
     ```

6. **Install the Platform**
   - Run the installation command to set up the platform:
     ```bash
     python setup.py install --user
     ```

#### Done!

### macOS Installation Guide

Follow the instructions [here](https://brew.sh/) and make sure you have homebrew correctly installed and updated.

Then follow these steps to set up the AgarCL environment on macOS:

1. **Clone the repository:**
   ```bash
   git clone --recursive git@github.com:AgarCL/AgarCL.git
   ```
2. **Change into the project directory:**
   ```bash
   cd AgarCL
   ```
3. **Create a Python virtual environment:**
   ```bash
   python3 -m venv agarclenv
   ```
4. **Activate the virtual environment:**
   ```bash
   source agarclenv/bin/activate
   ```
5. **Run the installer script:**
   ```bash
   ./install.sh
   ```
6. **Install Python dependencies:**
   ```bash
   pip install -r requirements.txt
   ```
7. **Build & install the Python package:**
   ```bash
   python3 setup.py install
    ```

#### Done!

### Linux Installation Guide

1. **Clone the repository:**
   ```bash
   git clone --recursive git@github.com:AgarCL/AgarCL.git
   ```
2. **Change into the project directory:**
   ```bash
   cd AgarCL
   ```
3. **Create a Python virtual environment:**
   ```bash
   python3 -m venv agarclenv
   ```
4. **Activate the virtual environment:**
   ```bash
   source agarclenv/bin/activate
   ```
5. **Run the installer script:**
   ```bash
   ./install.sh
   ```
6. **Install Python dependencies:**
   ```bash
   pip install -r requirements.txt
   ```
7. **Build & install the Python package:**
   ```bash
   python setup.py install
    ```

## Running the code
To run the Go Bigger example, execute the following line:

```python
python project_path/bench/go_bigger_example.py
```

To run the Screen Observations example, execute the following line:

```python
python project_path/bench/screen_obs_example.py
```

## Using the environment


```python
import gymnasium as gym

# Initialise the environment
env = gym.make("agario-screen-v0", render_mode="human")

# Reset the environment to generate the first observation
observation = env.reset()
for _ in range(1000):
    # this is where you would insert your policy
    c_target_space = gym.spaces.Box(low=-1, high=1, shape=(2,))
    d_target_space = gym.spaces.Discrete(3)
    action = [(c_target_space.sample(), d_target_space.sample())]

    # step (transition) through the environment with the action
    # receiving the next observation, reward and if the episode has terminated or truncated
    observation, reward, terminated, truncated, info = env.step(action)

    # If the episode has ended then we can reset to start a new episode
    if terminated or truncated:
        observation = env.reset()

env.close()
```

### Self-Play setup

In order to play the game yourself or enable rendering in the gym environment, you will need to build the game
client yourself on a system where OpenGL has been installed. Issue the following commands:


```shell
git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j 2 client agario
```

This will output an executable named client in the directory agario

```shell
agario/client
```

Use your cursor to control the agent.



### Loading and Saving Environment Snapshots

AgarCL allows you to save and load snapshots of the environment's state. This feature is useful for debugging, benchmarking, or resuming training from a specific point.

#### Saving a Snapshot

To save the current state of the environment, use the `save_env_state` method:

```python
env.save_env_state('path_to_save_snapshot.json')
```

This will save the environment's state to a JSON file at the specified path.

#### Loading a Snapshot

To load a previously saved snapshot, use the `load_env_state` method:

```python
env.load_env_state('path_to_snapshot.json')
```
Before loading a snapshot, ensure that the `load_env_state` option is enabled in the environment configuration. This will allow the environment to restore its state from the specified JSON file.

#### Example Usage

Here is an example of how to use these methods in a script:

```python
import gymnasium as gym

# Initialize the environment
env = gym.make("agario-screen-v0", render_mode="human")

# Load a snapshot if available
env.load_env_state('snapshot.json')

# Reset the environment
env.reset()

# Perform some steps
for _ in range(100):
   action = [(env.action_space.sample(), env.action_space.sample())]
   observation, reward, terminated, truncated, info = env.step(action)
   if terminated or truncated:
      break

# Save the environment's state
env.save_env_state('snapshot.json')

env.close()
```

This functionality ensures reproducibility and allows for efficient experimentation with different configurations.

### Recording and Saving Videos

AgarCL provides functionality to record and save videos of the environment's execution. This is useful for visualizing agent behavior or debugging.

#### Enabling Video Recording

To enable video recording, set the `record_video` parameter to `True` in the environment configuration. You can also enable video recording programmatically:

```python
env.enable_video_recorder()
```

#### Saving the Video

To save the recorded video, use the `generate_video` method:

```python
env.generate_video('path_to_save_video', 'video_name.avi')
```

This will save the video to the specified path with the given file name.

#### Disabling Video Recording

To stop recording, use the `disable_video_recorder` method:

```python
env.disable_video_recorder()
```

#### Example Usage

Here is an example of how to record and save a video:

```python
import gymnasium as gym

# Initialize the environment
env = gym.make("agario-screen-v0", render_mode="rgb_array")

# Enable video recording
env.enable_video_recorder()

# Reset the environment
env.reset()

# Perform some steps
for _ in range(100):
   action = [(env.action_space.sample(), env.action_space.sample())]
   observation, reward, terminated, truncated, info = env.step(action)
   if terminated or truncated:
      break

# Save the video
env.generate_video('videos', 'example_run.avi')

# Disable video recording
env.disable_video_recorder()

env.close()
```

### Real-Time Render View

Display the environment in a live GUI window for debugging, demos, and visually tracking your agent’s decisions as they happen.

An example of how to invoke the window:

```python
import gymnasium as gym

# Initialize the environment
env = gym.make("agario-screen-v0", render_mode="human")

# Reset the environment
env.reset()

# Perform some steps
for _ in range(100):
   action = [(env.action_space.sample(), env.action_space.sample())]
   observation, reward, terminated, truncated, info = env.step(action)

   # Update the on-screen display
   env.render()
   if terminated or truncated:
      break

env.close()
```


This functionality allows you to capture and analyze the agent's performance visually.


## Acknowledgment
This implementation is built upon the [AgarLE repository](https://github.com/jondeaton/AgarLE).
