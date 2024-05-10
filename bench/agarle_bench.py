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

null_action = (np.random.rand(1, 2)[0].astype(np.float32), np.random.randint(0, 3))
default_config = {
    'ticks_per_step':  4,
    'num_frames':      1,
    'arena_size':      1000,
    'num_pellets':     1000,
    'num_viruses':     25,
    'num_bots':        25,
    'pellet_regen':    True,
    'grid_size':       128,
    'observe_cells':   True,
    'observe_others':  True,
    'observe_viruses': True,
    'observe_pellets': True
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
        state, reward, done, info = env.step(null_action)
        states.append(state)
    import pdb; pdb.set_trace()
    env.render()


def parse_args():
    parser = argparse.ArgumentParser(description="Benchmark Agar.io Learning Environment")

    parser.add_argument("-n", "--num_steps", default=1000, type=int, help="Number of steps")

    env_options = parser.add_argument_group("Environment")
    env_options.add_argument("--env", default="agario-grid-v0")
    for param in default_config:
        env_options.add_argument("--" + param, default=default_config[param], type=type(default_config[param]))

    args = parser.parse_args()
    return args


if __name__ == "__main__":
    main()
