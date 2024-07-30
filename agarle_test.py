import gym
import time
from tqdm import tqdm

import numpy as np
import gym_agario
from gym.wrappers import RecordVideo

import imageio

def gen_random_actions():
  max_val, min_val = 1, -1
  range_size = max_val - min_val
  random_values = np.random.rand(2) * range_size + min_val
  return [tuple(random_values), 0] # ([(x,y), action])


def dump_next_states(filename, next_state):
  with open(filename, 'wb') as f:
      next_state.tofile(f)


def main(config):
  env = gym.make("agario-screen-v0",  render_mode="human", **config)
  # video_writer = imageio.get_writer('video/screen_env.mp4', fps=100)
  state = env.reset()

  start_time = time.time()
    
  num_steps = 750
  for i in tqdm(range(num_steps)):
    # null_action = gen_random_actions()
    null_action = [(1,1), 0]
    next_state, reward, done, truncation, info = env.step(null_action)  
    # rendered = env.render()
    env.render()
    # for j in range(env.num_frames):
    #   video_writer.append_data(rendered[j])   

  # video_writer.close()
  env.close()
    
  end_time = time.time()
  total_time = end_time - start_time
  fps = num_steps / total_time
    
  print(f"Frames per second (FPS): {fps:.2f}")


if __name__ == "__main__":
  config = {
    'num_frames': 1,
    'arena_size': 1000,
    'pellet_regen': True,
    'num_pellets': 1000,
    'num_viruses': 25,
    'num_bots': 25,
    'screen_len': 84, # for screen world
    # 'grid_size': 128, # for grid world
  } 
  
  main(config)
  