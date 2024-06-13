import gym
from gym import error, spaces, utils
import gym_agario
import argparse
import numpy as np
    

def gen_random_actions():
  max_val, min_val = 1, -1
  range_size = max_val - min_val
  random_values = np.random.rand(2) * range_size + min_val
  return ([(random_values[0], random_values[1]), 0]) # ([(x,y), action])


def main(config):
  env = gym.make("agario-screen-v0", **config)
  state = env.reset()
  print(f'State shape: {state.shape} ')
  # print(f'State: {state} ')
  env.render() 

  for _ in range(10):
    null_action = gen_random_actions()
    next_state, reward, done, info = env.step(null_action)
    print(f'Action: {null_action}')
    print(f'Next state shape: {next_state.shape} ')
    print(f'Next state max & min values: {np.max(next_state), np.min(next_state)} ')
    print(f'Done: {done}')
    print(f'Info: {info}')
  
    
if __name__ == "__main__":
  config = {
  'ticks_per_step':  4,     # Number of game ticks per step
  'num_frames':      1,     # Number of game ticks observed at each step
  'arena_size':      1000,  # Game arena size
  'num_pellets':     1000,
  'num_viruses':     25,
  'num_bots':        25,
  'pellet_regen':    True,  # Whether pellets regenerate randomly when eaten
  'grid_size':       1024,   # Size of spatial dimensions of observations
  'observe_cells':   True,  # Include an observation channel with agent's cells
  'observe_others':  True,  # Include an observation channel with other players' cells
  'observe_viruses': True,  # Include an observation channel with viruses
  'observe_pellets': True,   # Include an observation channel with pellets
  'render_mode': 'human',
  }
  
  main(config)
  