#!/usr/bin/env python
"""
Performance Benchmarking for AgarCL Gym Environment.

Example usage:

python -m cProfile bench/example.py \
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
    'ticks_per_step'    :   4,          #Frame Skipping
    'arena_size'        :   500,
    'num_pellets'       :   350,
    'num_viruses'       :   10,
    'num_bots'          :   8,
    'pellet_regen'      :   True,
    'screen_len'        :   128,
    'obs_type'          :   "screen",    #Two options: screen, gobigger
    'reward_type'       :   diff(),      #Two options: "mass:reward=mass", "diff = reward=mass(t)-mass(t-1)"
    'render_mode'       :   "rgb_array", #Two options: "human", "rgb_array"
    'num_agents'        :   1,
    'c_death'           :   0,           #reward = [diff or mass] - c_death if player is eaten [It is zero in the original paper]
    'agent_view'        :   True,        #Do you want to have the observation as 4 channels or RGB?
    'add_noise'         :   True,
    'mode'              :   0,
    'env_type'          :   1,           #0 -> episodic or 1 - > continuing
    'number_steps'      :   3000,        #Number of steps to run the environment in case it is episodic
    'load_env_snapshot' :   0,           #Do you want to load a snapshot of the environment?
    'record_video'      :   True,       #Do you want to record a video of the environment?
    'save_env_snapshot' :   True,       #Do you want to save a snapshot of the environment?
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
    if args.load_env_snapshot:
        env.load_env_state('THE_PATH_ENVIRONMENT_SNAPSHOT.json')

    env.reset()
    env.seed(args.seed)

    if args.record_video:
        env.enable_video_recorder()

    states = []
    SPS_VALUES = []
    global_step = 0
    start_time = time.time()
    total_reward = 0
    num_steps = args.num_steps

    import matplotlib.pyplot as plt

    episode_rewards = []
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

        #env.render()
        if(done):
            env.reset()


    if args.record_video:
        env.generate_video('YOUR_PATH_TO_SAVE_THE_VIDEO', 'VIDEO_NAME.avi')
        env.disable_video_recorder() #In case you want to disable the video recorder after some time steps

    if args.save_env_snapshot:
        env.save_env_state('YOUR_PATH_TO_SAVE_THE_SNAPSHOT.json')

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
