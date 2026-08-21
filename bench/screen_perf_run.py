#!/usr/bin/env python3
"""
Throughput benchmark for AgarCL screen observations.

Runs a random-action agent and logs windowed throughput, memory footprint and
reward statistics to CSV, so performance can be plotted against step count.

Defaults describe the full game (mode 0) as configured in
bench/screen_obs_example.py: continuing (non-episodic), 128x128 agent-view
observations, action repeat of 4. Mode 0 with env_type=1 is the paper's primary
continual setting - the environment never resets on its own, players respawn in
place, and neither the C++ env nor the gym wrapper reports done.

examples:
  # full game, reproducible
  screen_perf_run.py --seed 0 --steps 20000

  # match a task config
  screen_perf_run.py --mode 7 --arena-size 350 --num-pellets 500 \
                     --num-viruses 0 --num-bots 1 --env-type 0
"""
import argparse
import csv
import os
import resource
import sys
import time

import numpy as np
import gymnasium as gym
import gym_agario  # noqa: F401  (registers the agario-* environments)

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


def parse_args():
    p = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        description=__doc__)

    run = p.add_argument_group("run")
    run.add_argument("--steps", type=int, default=20_000,
                     help="environment steps to run")
    run.add_argument("--frames", type=int, default=None,
                     help="engine frames to run; overrides --steps. One env "
                          "step advances ticks_per_step frames (frame skip), "
                          "so 1M frames = 1M / ticks_per_step steps")
    run.add_argument("--window", type=int, default=2_000,
                     help="steps between logged samples")
    run.add_argument("--seed", type=int, default=0)
    run.add_argument("--out", type=str, default=None, help="CSV output path")
    run.add_argument("--label", type=str, default="run")

    env = p.add_argument_group("environment (full game / mode 0 by default)")
    env.add_argument("--mode", type=int, default=0, help="task selector, 0 = full game")
    env.add_argument("--env-type", type=int, default=1,
                     help="0 episodic (truncates at --number-steps), 1 continuing")
    env.add_argument("--arena-size", type=int, default=500)
    env.add_argument("--num-pellets", type=int, default=1024)
    env.add_argument("--num-bots", type=int, default=4)
    env.add_argument("--num-viruses", type=int, default=10,
                     help="10 matches the full-game config in screen_obs_example.py; "
                          "the mode_*.json task configs all use 0")
    env.add_argument("--pellet-regen", type=int, default=1)
    env.add_argument("--ticks-per-step", type=int, default=4,
                     help="action repeat: engine ticks per env step")
    env.add_argument("--screen-len", type=int, default=128)
    env.add_argument("--agent-view", type=int, default=1,
                     help="1 = 4 encoded channels, 0 = plain RGB")
    env.add_argument("--reward-type", type=int, default=1,
                     help="0 = current mass, 1 = change in mass")
    env.add_argument("--c-death", type=int, default=0,
                 help="death penalty; the binding takes an int, so "
                      "fractional penalties are not expressible")
    env.add_argument("--add-noise", type=int, default=1)
    env.add_argument("--number-steps", type=int, default=100_000,
                     help="truncation limit when --env-type 0")
    return p.parse_args()


def main() -> None:
    args = parse_args()
    if args.frames is not None:
        args.steps = max(1, args.frames // args.ticks_per_step)

    config = {
        "obs_type": "screen",
        "render_mode": "rgb_array",
        "mode": args.mode,
        "env_type": args.env_type,
        "arena_size": args.arena_size,
        "num_pellets": args.num_pellets,
        "num_bots": args.num_bots,
        "num_viruses": args.num_viruses,
        "pellet_regen": bool(args.pellet_regen),
        "ticks_per_step": args.ticks_per_step,
        "screen_len": args.screen_len,
        "agent_view": bool(args.agent_view),
        "reward_type": args.reward_type,
        "c_death": args.c_death,
        "add_noise": bool(args.add_noise),
        "num_agents": 1,
        "number_steps": args.number_steps,
    }

    env = gym.make("agario-screen-v0", **config)
    # Seed before reset: world generation (pellet and virus placement, spawn
    # positions) draws from the engine's generator during reset().
    env.unwrapped.seed(args.seed)
    obs, _ = env.reset()

    print("AgarCL screen throughput benchmark")
    print(f"  mode {args.mode} "
          f"({'continuing' if args.env_type == 1 else 'episodic'}), seed {args.seed}")
    print(f"  arena {args.arena_size}x{args.arena_size}, pellets {args.num_pellets}, "
          f"bots {args.num_bots}, viruses {args.num_viruses}")
    print(f"  {args.screen_len}x{args.screen_len} observations, "
          f"agent_view={bool(args.agent_view)}, ticks_per_step {args.ticks_per_step}")
    print(f"  observation shape {np.asarray(obs).shape}, "
          f"target {args.steps:,} steps "
          f"({args.steps * args.ticks_per_step:,} engine frames)\n", flush=True)

    writer = None
    handle = None
    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        handle = open(args.out, "w", newline="")
        writer = csv.writer(handle)
        writer.writerow(["label", "step", "elapsed_s", "window_sps", "cumulative_sps",
                         "rss_mb", "window_mean_reward", "window_max_reward",
                         "loadavg1", "resets"])

    rng = np.random.default_rng(args.seed)
    t_start = time.perf_counter()
    step = 0
    resets = 0

    while step < args.steps:
        n = min(args.window, args.steps - step)
        # actions drawn in blocks so the agent's own cost stays off the timed path
        targets = rng.uniform(-1, 1, size=(n, 2)).astype(np.float32)
        discretes = rng.integers(0, 3, size=n)
        rewards = np.empty(n, dtype=np.float64)

        t0 = time.perf_counter()
        for i in range(n):
            obs, reward, terminated, truncated, _ = env.step((targets[i], int(discretes[i])))
            rewards[i] = reward
            if terminated or truncated:   # not expected in mode 0 / continuing
                env.reset()
                resets += 1
        dt = time.perf_counter() - t0

        step += n
        elapsed = time.perf_counter() - t_start
        window_sps = n / dt
        cumulative_sps = step / elapsed
        load1 = os.getloadavg()[0]

        if writer:
            writer.writerow([args.label, step, f"{elapsed:.3f}", f"{window_sps:.1f}",
                             f"{cumulative_sps:.1f}", f"{rss_mb():.1f}",
                             f"{rewards.mean():.4f}", f"{rewards.max():.4f}",
                             f"{load1:.2f}", resets])
            handle.flush()

        pct = 100.0 * step / args.steps
        eta = (args.steps - step) / cumulative_sps if cumulative_sps else 0.0
        print(f"  {step:>9,} ({pct:5.1f}%)  {window_sps:7.0f} sps   "
              f"cum {cumulative_sps:7.0f} sps   {1e6 / window_sps:6.1f} us/step   "
              f"rss {rss_mb():6.1f} MB   load {load1:4.1f}   eta {eta:5.1f}s", flush=True)

    total = time.perf_counter() - t_start
    print(f"\n  done: {step:,} steps in {total:.1f}s")
    print(f"  mean throughput {step / total:.0f} steps/s "
          f"({1e6 * total / step:.1f} us/step), {resets} resets")
    if args.out:
        handle.close()
        print(f"  wrote {args.out}")
    env.close()


if __name__ == "__main__":
    main()
