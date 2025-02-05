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
    'ticks_per_step':  3,
    'num_frames':      1, # We should change it to make it always 1 : Skipping the num of frames
    'arena_size':      250,
    'num_pellets':     200,
    'num_viruses':     0,
    'num_bots':        0,
    'pellet_regen':    True,
    'grid_size':       84,
    'screen_len':      84,
    'observe_cells':   False,
    'observe_others':  False,
    'observe_viruses': False,
    'observe_pellets': False,
    'obs_type'       : "screen",   #Two options: screen, grid
    'reward_type'    : diff(), # Two options: "mass:reward=mass", "diff = reward=mass(t)-mass(t-1)"
    'render_mode'    : "rgb_array", # Two options: "human", "rgb_array"
    # 'multi_agent'    :  True,
    'num_agents'     :  1,
    'c_death'        : -100,  # reward = [diff or mass] - c_death if player is eaten
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
    env.reset()
    env.seed(0)
    states = []
    SPS_VALUES = []
    global_step = 0
    start_time = time.time()
    total_reward = 0
    num_episodes = 1

    import matplotlib.pyplot as plt

    episode_rewards = []

    for iter in tqdm.tqdm(range(num_episodes), desc="Benchmarking Progress"):
        episode_reward = 0
        episode_start_time = time.time()
        episode_steps = 0
        for _ in range(args.num_steps):
            agent_actions = []
            global_step += 1
            episode_steps += 1
            for i in range(num_agents):
                target_space = gym.spaces.Box(low=-1, high=1, shape=(2,))
                action = (target_space.sample(), 0)
                agent_actions.append(action)
            state, reward, done, truncations, step_num = env.step(agent_actions)
            print(f"reward: {reward}, step_num: {step_num}, Done: {done}")
            if(done == True):
                import pdb; pdb.set_trace()
            # Calculate SPS (Steps Per Second) for the episode
        episode_elapsed_time = time.time() - episode_start_time
        episode_SPS = episode_steps / episode_elapsed_time
        SPS_VALUES.append(episode_SPS)
        print(f"Episode {iter} finished in {episode_SPS:.2f} seconds")

    # Plotting SPS values
    plt.figure()
    plt.plot(SPS_VALUES)
    plt.xlabel('Step')
    plt.ylabel('SPS (Steps Per Second)')
    plt.title('Steps Per Second over Time')
    plt.savefig('/home/ayman/thesis/AgarLE/bench/sps_over_time.png')
    plt.close()


    env.close()

def parse_args():
    parser = argparse.ArgumentParser(description="Benchmark Agar.io Learning Environment")

    parser.add_argument("-n", "--num_steps", default=1000, type=int, help="Number of steps")
    parser.add_argument("--config_file", default='./tasks_configs/Exploration.json', type=str, help="Config file for the environment")
    env_options = parser.add_argument_group("Environment")
    env_options.add_argument("--env", default="agario-grid-v0")
    for param in default_config:
        env_options.add_argument("--" + param, default=default_config[param], type=type(default_config[param]))

    args = parser.parse_args()
    return args


if __name__ == "__main__":
    main()
