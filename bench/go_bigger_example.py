#!/usr/bin/env python
"""
Performance Benchmarking for AgarLE Gym Environment.

Example usage:

python -m cProfile bench/agarle_bench.py \
    -n 1000 --ticks_per_step 2 \
    | grep Agar

"""

import argparse
import gymnasium as gym
import gym_agario
import numpy as np
import cProfile
from abc import ABC, abstractmethod
import time
# import tqdm
def mass():
    return 0

def diff():
    return 1

import random
from typing import Tuple, List
import csv
import tqdm

# Default configuration for the environment
default_config = {
    'ticks_per_step':  4,
    'num_frames':      1, # We should change it to make it always 1 : Skipping the num of frames
    'arena_size':      350,
    'num_pellets':     500,
    'num_viruses':     0,
    'num_bots':        0,
    'pellet_regen':    True,
    'grid_size':       128,
    'screen_len':      128,
    'observe_cells':   False,
    'observe_others':  False,
    'observe_viruses': False,
    'observe_pellets': False,
    'obs_type'       : "gobigger",   #Three options: screen, grid, gobigger
    'reward_type'    : diff(), # Two options: "mass:reward=mass", "diff = reward=mass(t)-mass(t-1)"
    'render_mode'    : "human", # Two options: "human", "rgb_array"
    # 'multi_agent'    :  True,
    'num_agents'     :  1,
    'c_death'        : 0,  # reward = [diff or mass] - c_death if player is eaten
    'agent_view'     : True,
    'add_noise'     : True,
    'mode'          : 1,
    'number_steps'  : 500,
    'env_type'      : 0, #0 -> episodic or 1 - > continuing
}

# config_file = 'bench/tasks_configs/Exploration.json'
# with open(config_file, 'r') as file:
#     default_config = eval(file.read())
#     default_config = {k: (v.lower() == 'true' if isinstance(v, str) and v.lower() in ['true', 'false'] else v) for k, v in default_config.items()}

def main():

    args = parse_args()

    env_config = {
        name: getattr(args, name)
        for name in default_config
        if hasattr(args, name)
    }
    print(env_config)
    num_agents =  default_config['num_agents']
    env = gym.make(args.env, **env_config)

    print("environment created")
    env.reset()
    env.seed(args.seed)
    states = []
    SPS_VALUES = []
    global_step = 0
    start_time = time.time()
    total_reward = 0
    num_steps = args.num_steps

    import matplotlib.pyplot as plt

    episode_rewards = []
    env.enable_video_recorder()
    for iter in tqdm.tqdm(range(num_steps), desc="Benchmarking Progress"):
        episode_reward = 0
        episode_start_time = time.time()
        episode_steps = 0
        agent_actions = []
        global_step += 1
        episode_steps += 1
        for i in range(num_agents):
            c_target_space = gym.spaces.Box(low=-1, high=1, shape=(2,))
            d_target_space = gym.spaces.Discrete(3)
            action = (c_target_space.sample(), d_target_space.sample())
            agent_actions.append(action)
        state, reward, done, truncations, step_num = env.step(agent_actions)
        total_reward += reward
        with open('step_rewards.csv', 'a', newline='') as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow([global_step, reward, total_reward])
        # env.render()
        if(done):
            env.reset()

    episode_elapsed_time = time.time() - episode_start_time
    episode_SPS = episode_steps / episode_elapsed_time
    SPS_VALUES.append(episode_SPS)
    print(f"Episode {iter} finished in {episode_SPS:.2f} seconds")


    env.close()

def parse_args():
    parser = argparse.ArgumentParser(description="Benchmark Agar.io Learning Environment")

    parser.add_argument("-n", "--num_steps", default=500, type=int, help="Number of steps")
    parser.add_argument("--config_file", default='./tasks_configs/Exploration.json', type=str, help="Config file for the environment")
    parser.add_argument("--seed", default=0, type=int , help="Seed for running the environment")
    env_options = parser.add_argument_group("Environment")
    env_options.add_argument("--env", default="agario-grid-v0")
    for param in default_config:
        env_options.add_argument("--" + param, default=default_config[param], type=type(default_config[param]))

    args = parser.parse_args()
    return args


if __name__ == "__main__":
    main()
