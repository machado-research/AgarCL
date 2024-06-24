import gym
import time

import numpy as np
import gym_agario

def gen_random_actions():
  max_val, min_val = 1, -1
  range_size = max_val - min_val
  random_values = np.random.rand(2) * range_size + min_val
  return [tuple(random_values), 0] # ([(x,y), action])


def dump_next_states(filename, next_state):
  with open(filename, 'wb') as f:
      next_state.tofile(f)
  

def main(config):
  env = gym.make("agario-screen-v0", **config)
  
  # state = env.reset()
  # start_time = time.time()
  # env.render()
   
  num_steps = 10  
  state = env.reset()
  
  for i in range(num_steps):
    null_action = gen_random_actions()
    # next_state, reward, done, info = env.step([(1, 1), 0])
    next_state, reward, done, info = env.step(null_action)
     # env.render() 
    
    if np.max(next_state) > 0:
      dump_next_states(f'plotted_observations/next_state_{i+1}.bin', next_state)
    
  # end_time = time.time()
  # total_time = end_time - start_time
  # fps = num_steps / total_time
    
  # print(f"Frames per second (FPS): {fps:.2f}")


if __name__ == "__main__":
  config = {
    'ticks_per_step': 1,
    'arena_size': 1000,
    'pellet_regen': True,
    'num_pellets': 1000,
    'num_viruses': 25,
    'num_bots': 25,
    'screen_len': 84, # for screen world
    # 'grid_size': 9, # for grid world
  } 
  
  main(config)
  