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


def dump_next_states(filename, next_state):
  with open(filename, 'wb') as f:
      next_state.tofile(f)
  

def main(config):
  env = gym.make("agario-screen-v0", **config)
  state = env.reset()
  # env.render() 
  print(f'State shape: {state.shape} ')
  # print(f'State: {state} ')
  env.render() 

  for i in range(10):
    null_action = gen_random_actions()
    next_state, reward, done, info = env.step([(1, 1), 0])
    # print(f'Action: {null_action}')
    print(f'Next state shape: {next_state.shape} ')
    print(f'Next state max & min values: {np.max(next_state), np.min(next_state)} ')
    # print(f'Done: {done}')
    # print(f'Info: {info}')
    env.render() 
    
    if np.max(next_state) > 0:
      dump_next_states(f'plotted_observations/next_state_{i+1}.bin', next_state)
      
if __name__ == "__main__":
  config = {
    'ticks_per_step': 1,
    'arena_size': 1000,
    'pellet_regen': True,
    'num_pellets': 1000,
    'num_viruses': 25,
    'num_bots': 25,
    'screen_len': 1024, # for screen world
    # 'grid_size': 9, # for grid world
  } 
  
  main(config)
  