#!/usr/bin/env python
"""
Performance Benchmarking for AgarLE Gym Environment.

Example usage:

python -m cProfile bench/agarle_bench.py \
    -n 1000 --ticks_per_step 2 \
    | grep Agar

"""

import argparse
import gym, gym_agario
import numpy as np
import cProfile


def mass(): 
    return 0

def diff():
    return 1


default_config = {
    'ticks_per_step':  4,
    'num_frames':      10,
    'arena_size':      500,
    'num_pellets':     1000,
    'num_viruses':     25,
    'num_bots':        100000,
    'pellet_regen':    True,
    'grid_size':       9,
    'observe_cells':   False,
    'observe_others':  True,
    'observe_viruses': True,
    'observe_pellets': True,
    'obs_type'       : "grid",   #Two options: screen, grid
    'reward_type'    : diff(), # Two options: "mass:reward=mass", "diff = reward=mass(t)-mass(t-1)"
    'c_death'        : -100,  # reward = [diff or mass] - c_death if player is eaten
}


def main():
    args = parse_args()
    env_config = {
        name: getattr(args, name)
        for name in default_config
        if hasattr(args, name)
    }

    env = gym.make(args.env, **env_config)
    env.reset() 
    states = []
    for _ in range(args.num_steps):
        max_val, min_val = 1, -1
        range_size = max_val - min_val
        random_values = [0.01, 0.1]
        null_action = ([(random_values[0], random_values[1]),0])
        state, reward, done, step_num = env.step(null_action) 
        env.render()
    env.close()
#

def parse_args():
    parser = argparse.ArgumentParser(description="Benchmark Agar.io Learning Environment")

    parser.add_argument("-n", "--num_steps", default=10000, type=int, help="Number of steps")

    env_options = parser.add_argument_group("Environment")
    env_options.add_argument("--env", default="agario-grid-v0")
    for param in default_config:
        env_options.add_argument("--" + param, default=default_config[param], type=type(default_config[param]))

    args = parser.parse_args()
    return args


if __name__ == "__main__":
    main()
