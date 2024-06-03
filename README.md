<img width="380" alt="AgarLE" src="https://user-images.githubusercontent.com/15920014/60447827-9d1a1a00-9c24-11e9-8a8b-a8043e8e1302.png">

The Agar.io Learning Environment (AgarLE) is a performant implementation of the popular online multi-player game agar.io along with an [OpenAI Gym](https://gym.openai.com/) interface suitable for reinforcement learning in Python.

<p align="center">
<img width="460" alt="Screenshot" src="https://user-images.githubusercontent.com/15920014/57587859-dbb31400-74c0-11e9-8f47-3e39113b99b4.png">
</p>

# Prerequisites
Before you begin, ensure you have the following installed on your system:

- **CMake**: A cross-platform tool designed to build, test, and package software.
- **GLM**: A header-only C++ mathematics library for graphics software based on the OpenGL Shading Language (GLSL) specifications.
- **cxxopts**: Lightweight C++ command-line option parser.

Additionally, you will need to install some essential packages for OpenGL development.

## Linux 


# Installation
Clone this repository (with submodules)

    git clone --recursive https://github.com/jondeaton/AgarLE.git

run the included installation script

    python setup.py install

# Usage

Installation will have installed the python module `gym_agario`, which when imported
registers the AgarLE gym environments. You need only import `gym_agario` and then
make an environment in the standard way 

```python
import gym
import gym_agario
    
env = gym.make("agario-grid-v0")
    
game_state = env.reset()
print(game_state.shape) # (128, 128, 10) , (grid_size, grid_size, num_channels)

action = np.array([0, 0]), 0  # don't move, don't split
while True:
  game_state, reward, done, info = env.step(action)
  if done: break
```

The Agar.io game and observation space are highly configurable. You can change
the parameters of the game and observation properties like so (default configuration
shown).

```python
config = {
  'ticks_per_step':  4,     # Number of game ticks per step
  'num_frames':      2,     # Number of game ticks observed at each step
  'arena_size':      1000,  # Game arena size
  'num_pellets':     1000,
  'num_viruses':     25,
  'num_bots':        25,
  'pellet_regen':    True,  # Whether pellets regenerate randomly when eaten
  'grid_size':       128,   # Size of spatial dimensions of observations
  'observe_cells':   True,  # Include an observation channel with agent's cells
  'observe_others':  True,  # Include an observation channel with other players' cells
  'observe_viruses': True,  # Include an observation channel with viruses
  'observe_pellets': True   # Include an observation channel with pellets
}

env = gym.make("agario-grid-v0", **config)
```

# Multi-Agent Environments

This gym supports multiple agents in the same game. By default, there will
only be a single agent, and the gym will conform to the typical gym interface
(note that there may still be any number of "bots" in the environment).
However, if you pass `"multi_agent": True` to the environment configuration
then the environment will have multiple agents all interacting within the
same agar.io game simultaneously.

    env = gym.make("agario-grid-v0", **{
        "multi_agent": True,
        "num_agents": 5
    })

With this configuration, the environment will no longer conform to the 
typical OpenAI gym interface in the following ways.

1. `step()` will expect a list of actions of the same length
as the number of agents, which specifies the action for each agent.

2. The return value of `step()` will be a list of observations,
list of rewards, and list of dones each with length equal to
the number of agents. The `i`'th elements of each lists
correspond to the `i`th agent consistently throughout the episode.

3. `reset()` will return a list of observations of length equal
to the number of agents.

4. When an agent is "done", observations for that agent will be None.
The environment may still be stepped while there is at least one agent
that is not "done". Only when all agents are done must the environment
be reset.

Note that if you pass `num_agents` greater than 1, `multi_agent`
will be set True automatically.

# Caveats

Currently compilation/installation is only working with Clang, so if you're
on Linux then you'll need to set your C++ compiler to Clang in your environment
before installing.

    CXX=`which clang++`

The only environment which has been tested extensively is `agario-grid-v0`,
although the RAM environment `agario-ram-v0` and screen environment `agario-screen-v0`
should work with some coaxing. The `agario-screen-v0` requires a window manager to
work so will not work on headless Linux machines, for instance. Calling `render`
will only work if the executable has been built with rendering turned on as can be
done by following the advanced set up guide. Rendering will not work
with the "screen" environment, despite the fact that that environment uses
the screen image as the environment's observation.

# Advanced setup
In order to play the game yourself or enable rendering in the gym environment,
you will need to build the game client yourself on a system where OpenGL has
been installed. This is most likely to succeed on macOS, but will probably work
on Linux. Issue the following commands

    git submodule update --init --recursive
    mkdir build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j 2 client agarle

This will output an executable named `client` in the directory `agario`

    agario/client  # play the game

If you also build the `agarle` target, then a python-importable dynamic library
(i.e. `*.so` file) named `agarle` will have been produced. To use it, copy it
into the "site packages" for your Python interpreter like so:

    cp environment/agarle* `python -m site --user-site`

The underlying gym environments may be compiled such that calling `render()`
will render the game onto the screen. This feature is turned off by default
for performance and portability reasons, but can be turned on during
compilation by using the following cmake command instead of the one shown above. 

    cmake -DCMAKE_BUILD_TYPE=Release -DRENDERABLE=ON ..
# Debugging 
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
