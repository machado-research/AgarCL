# Throughput benchmark: master vs `perf/rendering-and-engine`

3 seeds x 500,000 environment steps per arm, run sequentially on the same
machine (macOS, Apple Silicon). Random actions.

**Configuration** (full game, as in `bench/screen_obs_example.py`):
mode 0, env_type 1 (continuing), arena 500x500, 1024 pellets, 4 bots,
10 viruses, 128x128 screen observations with `agent_view=True`,
`ticks_per_step=4` (so 500k steps = 2M engine frames per run).

Reproduce with:

```bash
python3 bench/screen_perf_run.py --seed 0 --steps 500000 --window 10000 \
    --num-pellets 1024 --num-bots 4 --num-viruses 10 \
    --out results/branch_500k_s0.csv --label branch_seed0
python3 bench/screen_perf_plot.py --csv results/*.csv --out results/compare.png
```

## Branch arm (`perf/rendering-and-engine`)

![branch 3 seeds](branch_500k_3seeds.png)

| seed | mean throughput | window min-max | RSS first -> last | wall time | resets |
|------|-----------------|----------------|-------------------|-----------|--------|
| 0    | 3,725 sps       | 3,021 - 4,661  | 124.8 -> 125.1 MB | 2.2 min   | 0      |
| 1    | 3,427 sps       | 3,128 - 4,148  | 124.0 -> 124.6 MB | 2.4 min   | 0      |
| 2    | 3,402 sps       | 3,047 - 4,185  | 124.1 -> 124.6 MB | 2.5 min   | 0      |

**Aggregate: 3,518 +/- 180 steps/s** (~284 us/step, ~14,100 engine
frames/s).

Notes:

- No throughput degradation within runs. Late-run windows are 11-26%
  *faster* than early ones, which tracks background load on the machine
  falling during the runs (machine noise, not env warm-up).
- Memory is flat: +0.3-0.6 MB over 500k steps (~125 MB steady). No leak.
- Zero resets across all 1.5M steps, as expected for mode 0 continuing.

## Master arm (baseline, `8580d4f`)

*Runs in progress; this section and the mean-of-seeds comparison will be
filled in when they complete.*
