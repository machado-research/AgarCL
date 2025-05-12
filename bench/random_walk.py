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
    'arena_size':      350,
    'num_pellets':     1024,
    'num_viruses':     0,
    'num_bots':        0,
    'pellet_regen':    True,
    'grid_size':       84,
    'screen_len':      128,
    'observe_cells':   False,
    'observe_others':  False,
    'observe_viruses': False,
    'observe_pellets': False,
    'obs_type'       : "screen",   #Two options: screen, grid
    'reward_type'    : diff(), # Two options: "mass:reward=mass", "diff = reward=mass(t)-mass(t-1)"
    'render_mode'    : "rgb_array", # Two options: "human", "rgb_array"
    # 'multi_agent'    :  True,
    'num_agents'     :  1,
    'c_death'        : 0,  # reward = [diff or mass] - c_death if player is eaten
    'agent_view'     : True,
    'add_noise'     : True,
    'mode'          : 7,
    'number_steps'  : 3000,
    'env_type'      : 1, #0 -> episodic or 1 - > continuing
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
    output_dir = 'random_walk_mode_CPU_4_Skipping'
    os.makedirs(output_dir, exist_ok=True)
    bot_total_reward = 0
    output_file = os.path.join(output_dir, f'episodic_rewards_sps_{args.seed}.csv')
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
        bot_total_reward += step_num['bot_reward']
        if global_step % 100 == 0:
            print(f"Step: {global_step}, Episode Reward: {episode_reward}, Bot Reward: {bot_total_reward}, Time: {time.time() - episode_start_time}, SPS: {global_step / (time.time() - start_time)}")
            import pdb; pdb.set_trace()
        if(done):
            env.reset()
            episode_rewards.append(episode_reward)
            episode_reward = 0
            import pdb; pdb.set_trace()
            global_step = 0

        # if(global_step % 100 == 0):
        #     print(f"Episode: {len(episode_rewards)}, Episode Reward: {episode_reward}, Time: {time.time() - episode_start_time}, SPS: {1000 / (time.time() - episode_start_time)}")
        #     episode_start_time = time.time()
        #     # Write to CSV every timestep
        #     with open(output_file, 'a', newline='') as csvfile:
        #         writer = csv.writer(csvfile)
        #         if global_step == 100:  # Write header only once
        #             writer.writerow(['Timestep', 'SPS'])
        #         writer.writerow([global_step, sps])




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
