#!/usr/bin/env python3
"""
Throughput benchmark: screen observations, mode 0 (full game), continuing.

Runs a random-action agent for N steps and logs windowed throughput,
memory footprint and reward statistics to CSV so performance can be
plotted against step count.

Mode 0 / env_type 1 is the paper's primary continual setting: the
environment is non-episodic, players respawn in place, and neither the
C++ env nor the gym wrapper ever reports done.

usage:
  screen_perf_run.py --steps 10000000 --out results/perf_new.csv --label new
"""
import argparse
import csv
import os
import resource
import sys
import time

import numpy as np
import gymnasium as gym
import gym_agario

# mode 0, continuing, screen observations.
CONFIG = {
    "obs_type": "screen",
    "render_mode": "rgb_array",
    "ticks_per_step": 4,
    "num_frames": 1,
    "arena_size": 500,
    "num_pellets": 1024,
    "num_viruses": 8,
    "num_bots": 6,
    "pellet_regen": True,
    "screen_len": 128,
    "reward_type": 1,      # reward = mass(t) - mass(t-1)
    "c_death": 0,
    "agent_view": True,
    "add_noise": True,
    "num_agents": 1,
    "mode": 0,             # full game
    "env_type": 1,         # continuing (non-episodic)
    "load_env_snapshot": 0,
}


try:
    import psutil
    _PROC = psutil.Process()

    def rss_mb() -> float:
        """current resident set size, so the curve can fall as well as rise"""
        return _PROC.memory_info().rss / (1024 ** 2)
except ImportError:
    def rss_mb() -> float:
        """peak RSS fallback; macOS reports bytes, Linux kilobytes"""
        rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        return rss / (1024 ** 2) if sys.platform == "darwin" else rss / 1024


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--steps", type=int, default=10_000_000)
    p.add_argument("--window", type=int, default=10_000, help="steps per logged sample")
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--out", type=str, required=True)
    p.add_argument("--label", type=str, default="run")
    args = p.parse_args()

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)

    env = gym.make("agario-screen-v0", **CONFIG)
    env.unwrapped.seed(args.seed)   # seed before reset: engine rng drives world gen
    obs, _ = env.reset()

    rng = np.random.default_rng(args.seed)
    # pre-draw actions in blocks: keeps the agent cost off the measured path
    # as much as possible without changing the env's own workload
    block = args.window
    dtype = np.float32

    f = open(args.out, "w", newline="")
    writer = csv.writer(f)
    writer.writerow(["label", "step", "elapsed_s", "window_sps", "cumulative_sps",
                     "rss_mb", "window_mean_reward", "window_max_reward",
                     "loadavg1", "resets"])

    print(f"[{args.label}] mode 0 continuing | arena {CONFIG['arena_size']} "
          f"pellets {CONFIG['num_pellets']} bots {CONFIG['num_bots']} "
          f"viruses {CONFIG['num_viruses']} screen {CONFIG['screen_len']}", flush=True)
    print(f"[{args.label}] target {args.steps:,} steps, sample every {args.window:,}", flush=True)

    t_start = time.perf_counter()
    step = 0
    resets = 0
    while step < args.steps:
        n = min(block, args.steps - step)
        targets = rng.uniform(-1, 1, size=(n, 2)).astype(dtype)
        discretes = rng.integers(0, 3, size=n)
        rewards = np.empty(n, dtype=np.float64)

        t0 = time.perf_counter()
        for i in range(n):
            obs, r, done, trunc, info = env.step((targets[i], int(discretes[i])))
            rewards[i] = r
            if done or trunc:      # not expected in mode 0 / continuing
                env.reset()
                resets += 1
        dt = time.perf_counter() - t0

        step += n
        elapsed = time.perf_counter() - t_start
        # record machine load so windows contaminated by other processes are
        # visible when comparing runs measured at different times
        load1 = os.getloadavg()[0]
        writer.writerow([args.label, step, f"{elapsed:.3f}", f"{n/dt:.1f}",
                         f"{step/elapsed:.1f}", f"{rss_mb():.1f}",
                         f"{rewards.mean():.4f}", f"{rewards.max():.4f}",
                         f"{load1:.2f}", resets])
        f.flush()
        pct = 100.0 * step / args.steps
        eta = (args.steps - step) / (step / elapsed) if step else 0
        print(f"[{args.label}] {step:>10,} ({pct:5.1f}%)  "
              f"{n/dt:7.0f} sps  cum {step/elapsed:7.0f} sps  "
              f"rss {rss_mb():6.1f} MB  eta {eta/60:6.1f} min", flush=True)

    total = time.perf_counter() - t_start
    print(f"[{args.label}] DONE {step:,} steps in {total/60:.1f} min "
          f"({step/total:.0f} steps/s mean, {resets} resets)", flush=True)
    f.close()
    env.close()


if __name__ == "__main__":
    main()
