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
import os
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
    'ticks_per_step':  1,
    'num_frames':      1, # We should change it to make it always 1 : Skipping the num of frames
    'arena_size':      144,
    'num_pellets':     1000,
    'num_viruses':     10,
    'num_bots':        0,
    'pellet_regen':    True,
    'grid_size':       128,
    'screen_len':      128,
    'observe_cells':   False,
    'observe_others':  False,
    'observe_viruses': False,
    'observe_pellets': False,
    'obs_type'       : "gobigger",   #Two options: screen, grid
    'reward_type'    : diff(), # Two options: "mass:reward=mass", "diff = reward=mass(t)-mass(t-1)"
    'render_mode'    : "rgb_array", # Two options: "human", "rgb_array"
    # 'multi_agent'    :  True,
    'num_agents'     :  1,
    'c_death'        : 0,  # reward = [diff or mass] - c_death if player is eaten
    'agent_view'     : True,
    'add_noise'     : True,
    'mode'          : 0,
    'number_steps'  : 14400,
    'env_type'      : 0, #0 -> episodic or 1 - > continuing
}


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
    env.reset()
    env.seed(args.seed)
    states = []
    SPS_VALUES = []
    global_step = 0
    start_time = time.time()
    total_reward = 0
    total_steps = int(1e6)

    import matplotlib.pyplot as plt

    episode_rewards = []
    episode_reward = 0
    episode_start_time = time.time()
    sps_data = []  # To store Episode and SPS values
    output_dir = 'GoBigger_SPS_ours_CPUONLY'
    os.makedirs(output_dir, exist_ok=True)
    output_file = os.path.join(output_dir, f'episodic_rewards_sps_{args.seed}.csv')

    with open(output_file, 'a', newline='') as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow(['Timestep', 'SPS'])
    for iter in tqdm.tqdm(range(total_steps), desc="Benchmarking Progress"):

        agent_actions = []
        global_step += 1
        for i in range(num_agents):
            c_target_space = gym.spaces.Box(low=-1, high=1, shape=(2,))
            d_target_space = gym.spaces.Discrete(3)
            action = (c_target_space.sample(), d_target_space.sample())
            agent_actions.append(action)
        state, reward, done, truncations, step_num = env.step(agent_actions)
        episode_reward += reward

        if(global_step % 100 == 0):
            sps = 100 / (time.time() - episode_start_time)
            episode_start_time = time.time()
            # Write to CSV every timestep
            with open(output_file, 'a', newline='') as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow([global_step, sps])


        if done:
            env.reset()

    env.close()


def parse_args():
    parser = argparse.ArgumentParser(description="Benchmark Agar.io Learning Environment")

    parser.add_argument("-n", "--num_steps", default=3000, type=int, help="Number of steps")
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
