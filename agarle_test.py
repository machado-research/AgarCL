import gymnasium as gym
import gym_agario
import argparse
import time
from tqdm import tqdm


import imageio
import imageio_ffmpeg
import numpy as np

def gen_random_actions():
  max_val, min_val = 1, -1
  range_size = max_val - min_val
  random_values = np.random.rand(2) * range_size + min_val
  return [(tuple(random_values), 0)] # ([(x,y), action])


def dump_next_states(filename, next_state):
  with open(filename, 'wb') as f:
      next_state.tofile(f)


def main(config):
  env = gym.make("agario-grid-v0",  render_mode="rgb_array", **config)
  video_writer = imageio.get_writer('video/grid_env.mp4', fps=50)
  state = env.reset()

  start_time = time.time()

  num_steps = 500
  for i in tqdm(range(num_steps)):
    # null_action = gen_random_actions()
    null_action = gen_random_actions()
    next_state, reward, done, truncation, info = env.step(null_action)
    rendered = env.render()
    for j in range(env.num_frames):
      video_writer.append_data(rendered[j])

  video_writer.close()
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
    'screen_len': 256, # for screen world
    'grid_size': 128, # for grid world
  }

  main(config)
