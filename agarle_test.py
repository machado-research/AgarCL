import gymnasium as gym
import gym_agario
import argparse
import numpy as np
    
    
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
  'observe_pellets': True,   # Include an observation channel with pellets
  
  'obs_type': 'grid'
}


import agarle
print(agarle.__file__)

env = gym.make("agario-grid-v0", **config)
game_state = env.reset()
print(game_state.shape) # (128, 128, 10) , (grid_size, grid_size, num_channels)

# action = ((0, 0), 0) # don't move, don't split
action = np.array([0, 0]), 0
while True:
  game_state, reward, done, info = env.step(action)
  if bool(done): break