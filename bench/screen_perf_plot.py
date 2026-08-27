#!/usr/bin/env python3
"""
Plots throughput comparison from screen_perf_run.py CSVs.

usage:
  screen_perf_plot.py --csv results/perf_master.csv results/perf_branch.csv \
                      --out results/screen_perf.png
"""
import argparse
import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

COLORS = {"master": "#B0413E", "branch": "#2F6DB0"}
NICE = {"master": "master (baseline)", "branch": "perf branch"}

# Shade families so multi-seed runs stay readable: labels are matched by
# prefix ("master*" reds, "branch*" blues) and shaded in order of appearance.
FAMILIES = {
    "master": ["#B0413E", "#E07B60", "#7B241C"],
    "branch": ["#2F6DB0", "#5DADE2", "#1A5276"],
}


def family_of(label):
    for fam in FAMILIES:
        if label.startswith(fam):
            return fam
    return None


def assign_colors(runs):
    """exact COLORS for legacy labels, family shades for seed suffixes"""
    seen = {fam: 0 for fam in FAMILIES}
    out = []
    for r in runs:
        lab = r["label"]
        if lab in COLORS:
            out.append(COLORS[lab])
            continue
        fam = family_of(lab)
        if fam:
            shades = FAMILIES[fam]
            out.append(shades[seen[fam] % len(shades)])
            seen[fam] += 1
        else:
            out.append(None)
    return out


def load(path):
    rows = list(csv.DictReader(open(path)))
    if not rows:
        raise SystemExit(f"{path} is empty")
    return {
        "label": rows[0]["label"],
        "step": np.array([int(r["step"]) for r in rows]),
        "sps": np.array([float(r["window_sps"]) for r in rows]),
        "cum": np.array([float(r["cumulative_sps"]) for r in rows]),
        "rss": np.array([float(r["rss_mb"]) for r in rows]),
        "elapsed": np.array([float(r["elapsed_s"]) for r in rows]),
        "load": np.array([float(r.get("loadavg1", 0) or 0) for r in rows]),
        "reward": np.array([float(r["window_mean_reward"]) for r in rows]),
    }


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--csv", nargs="+", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--title", default=None)
    args = p.parse_args()

    runs = [load(c) for c in args.csv]
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)

    fig, axes = plt.subplots(2, 2, figsize=(14, 9))
    ax_sps, ax_cum, ax_rss, ax_time = axes.ravel()
    mstep = 1e-6  # x axis in millions of steps

    colors = assign_colors(runs)
    for r, c in zip(runs, colors):
        lab = NICE.get(r["label"], r["label"])
        x = r["step"] * mstep

        ax_sps.plot(x, r["sps"], color=c, lw=1.0, alpha=0.45)
        if len(r["sps"]) >= 9:  # rolling median to show the trend through noise
            k = 9
            smooth = np.array([np.median(r["sps"][max(0, i - k // 2):i + k // 2 + 1])
                               for i in range(len(r["sps"]))])
            ax_sps.plot(x, smooth, color=c, lw=2.2, label=f"{lab} (median filt.)")
        else:
            ax_sps.plot(x, r["sps"], color=c, lw=2.2, label=lab)

        ax_cum.plot(x, r["cum"], color=c, lw=2.0, label=lab)
        ax_rss.plot(x, r["rss"], color=c, lw=2.0, label=lab)
        ax_time.plot(x, r["elapsed"] / 60.0, color=c, lw=2.0, label=lab)

    ax_sps.set(xlabel="environment steps (millions)", ylabel="steps / second",
               title="Throughput vs steps (per window)")
    ax_sps.legend(); ax_sps.grid(alpha=0.3)
    ax_sps.set_ylim(bottom=0)

    ax_cum.set(xlabel="environment steps (millions)", ylabel="steps / second",
               title="Cumulative mean throughput")
    ax_cum.legend(); ax_cum.grid(alpha=0.3)
    ax_cum.set_ylim(bottom=0)

    ax_rss.set(xlabel="environment steps (millions)", ylabel="resident memory (MB)",
               title="Process memory (flat => no leak)")
    ax_rss.legend(); ax_rss.grid(alpha=0.3)
    ax_rss.set_ylim(bottom=0)

    ax_time.set(xlabel="environment steps (millions)", ylabel="wall-clock (minutes)",
                title="Time to reach step count")
    ax_time.legend(); ax_time.grid(alpha=0.3)

    # speedup annotation: mean of final cumulative throughput per family
    fams = {}
    for r in runs:
        fam = r["label"] if r["label"] in COLORS else family_of(r["label"])
        if fam:
            fams.setdefault(fam, []).append(r["cum"][-1])
    if "master" in fams and "branch" in fams:
        sp = np.mean(fams["branch"]) / np.mean(fams["master"])
        ax_cum.annotate(f"{sp:.2f}x", xy=(0.62, 0.5), xycoords="axes fraction",
                        fontsize=22, fontweight="bold", color="#2F6DB0")

    title = args.title or (
        "AgarCL screen env - mode 0 (continuing), random actions\n"
        "arena 500x500, 1024 pellets, 6 bots, 8 viruses, 128x128 obs, agent_view")
    fig.suptitle(title, fontsize=12)
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    fig.savefig(args.out, dpi=150)
    print("wrote", args.out)

    for r in runs:
        print(f"{r['label']:8s} steps={r['step'][-1]:>10,}  "
              f"mean={r['cum'][-1]:7.0f} sps  "
              f"first-window={r['sps'][0]:6.0f}  last-window={r['sps'][-1]:6.0f}  "
              f"rss {r['rss'][0]:.0f}->{r['rss'][-1]:.0f} MB  "
              f"wall={r['elapsed'][-1]/60:.1f} min  load~{r['load'].mean():.1f}")


if __name__ == "__main__":
    main()
