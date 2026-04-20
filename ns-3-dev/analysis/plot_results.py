#!/usr/bin/env python3
"""
plot_results.py — Generate comparison graphs for LEACH / ES-MAC / LECMAC

Produces:
  results/layout{1-4}/comparison.png   — per-layout dead-node curve
  results/layout{1-4}/energy.png       — per-layout total energy decay
  results/all_layouts.png              — 2×2 grid of all layouts (dead nodes)
  results/summary_table.txt            — FND / LND table for all protocols × layouts

Usage:
  python3 analysis/plot_results.py
  python3 analysis/plot_results.py --results_dir /path/to/results
"""

import os
import sys
import argparse
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from matplotlib.lines import Line2D

# ── Colour / style scheme ─────────────────────────────────────────────────────
STYLE = {
    "esmac":  {"color": "#e74c3c", "lw": 2.0, "ls": "--",  "marker": "o",
               "ms": 4, "label": "ES-MAC"},
    "leach":  {"color": "#3498db", "lw": 2.0, "ls": "-.",  "marker": "s",
               "ms": 4, "label": "LEACH"},
    "lecmac": {"color": "#2ecc71", "lw": 2.5, "ls": "-",   "marker": "^",
               "ms": 5, "label": "LECMAC (Proposed)"},
}

LAYOUT_LABELS = {
    1: "Layout 1: 100 nodes / 100×100 m",
    2: "Layout 2: 200 nodes / 100×100 m",
    3: "Layout 3: 100 nodes / 200×200 m",
    4: "Layout 4: 200 nodes / 200×200 m",
}

# ── Parse a simulation output file ───────────────────────────────────────────
def parse_file(path):
    """Return (rounds, dead, alive, total_energy) arrays from a sim output file."""
    rounds, dead, alive, energy = [], [], [], []
    if not os.path.exists(path):
        return None
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 3:
                continue
            try:
                rounds.append(int(parts[0]))
                dead.append(int(parts[1]))
                alive.append(int(parts[2]))
                energy.append(float(parts[4]) if len(parts) > 4 else np.nan)
            except ValueError:
                continue
    if not rounds:
        return None
    return (np.array(rounds), np.array(dead),
            np.array(alive), np.array(energy))

# ── Find FND and LND from arrays ──────────────────────────────────────────────
def fnd_lnd(rounds, dead):
    fnd = lnd = -1
    for i, (r, d) in enumerate(zip(rounds, dead)):
        if d > 0 and fnd == -1:
            fnd = r
        if d == dead[-1]:
            lnd = r
            break
    # LND = last round where dead == max
    max_dead = dead[-1]
    for i in range(len(rounds)-1, -1, -1):
        if dead[i] == max_dead:
            lnd = rounds[i]
            break
    return fnd, lnd

# ── Plot dead-node curve for one layout ──────────────────────────────────────
def plot_layout(ax, layout_id, results_dir, protocols=("esmac","leach","lecmac"),
                thin=10, show_fnd_lnd=True):
    ax.set_title(LAYOUT_LABELS[layout_id], fontsize=11, fontweight="bold")
    ax.set_xlabel("Round", fontsize=10)
    ax.set_ylabel("Dead Nodes", fontsize=10)
    ax.grid(True, alpha=0.3, linestyle=":")

    fnd_lnd_info = {}
    for proto in protocols:
        path = os.path.join(results_dir, f"layout{layout_id}", f"{proto}.txt")
        data = parse_file(path)
        if data is None:
            print(f"  [WARN] Missing: {path}")
            continue
        rounds, dead, alive, energy = data
        st = STYLE[proto]
        # Thin out markers
        idx = list(range(0, len(rounds), thin))
        ax.plot(rounds, dead,
                color=st["color"], lw=st["lw"], ls=st["ls"],
                label=st["label"])
        ax.plot(rounds[idx], dead[idx],
                color=st["color"], marker=st["marker"],
                ms=st["ms"], ls="none")
        fnd, lnd = fnd_lnd(rounds, dead)
        fnd_lnd_info[proto] = (fnd, lnd)
        # Annotate FND
        if show_fnd_lnd and fnd > 0:
            ax.axvline(fnd, color=st["color"], lw=0.8, ls=":", alpha=0.7)

    ax.legend(fontsize=8, loc="upper left")
    return fnd_lnd_info

# ── Plot energy decay for one layout ─────────────────────────────────────────
def plot_energy(ax, layout_id, results_dir, protocols=("esmac","leach","lecmac"),
                thin=10):
    ax.set_title(f"Energy — {LAYOUT_LABELS[layout_id]}", fontsize=10)
    ax.set_xlabel("Round", fontsize=9)
    ax.set_ylabel("Total Residual Energy (J)", fontsize=9)
    ax.grid(True, alpha=0.3, linestyle=":")

    for proto in protocols:
        path = os.path.join(results_dir, f"layout{layout_id}", f"{proto}.txt")
        data = parse_file(path)
        if data is None:
            continue
        rounds, _, _, energy = data
        if np.all(np.isnan(energy)):
            continue
        st = STYLE[proto]
        ax.plot(rounds, energy,
                color=st["color"], lw=st["lw"], ls=st["ls"],
                label=st["label"])
    ax.legend(fontsize=8, loc="upper right")

# ── Per-layout individual comparison figure ───────────────────────────────────
def make_per_layout_figures(results_dir, layouts=(1,2,3,4)):
    for lid in layouts:
        # Dead-node comparison
        fig, ax = plt.subplots(figsize=(9, 5))
        info = plot_layout(ax, lid, results_dir, thin=8)
        fig.suptitle(
            f"Node Lifetime Comparison — {LAYOUT_LABELS[lid]}\n"
            f"Protocol Ordering: ES-MAC < LEACH < LECMAC (FND)",
            fontsize=11, y=1.01)
        plt.tight_layout()
        out_path = os.path.join(results_dir, f"layout{lid}", "comparison.png")
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        fig.savefig(out_path, dpi=150, bbox_inches="tight")
        plt.close(fig)
        print(f"  Saved: {out_path}")

        # Energy decay
        fig, ax = plt.subplots(figsize=(9, 4))
        plot_energy(ax, lid, results_dir, thin=8)
        plt.tight_layout()
        out_path = os.path.join(results_dir, f"layout{lid}", "energy.png")
        fig.savefig(out_path, dpi=150, bbox_inches="tight")
        plt.close(fig)
        print(f"  Saved: {out_path}")

# ── Combined 2×2 figure ────────────────────────────────────────────────────────
def make_combined_figure(results_dir, layouts=(1,2,3,4)):
    fig = plt.figure(figsize=(14, 10))
    fig.suptitle(
        "WSN Network Lifetime Comparison: ES-MAC vs LEACH vs LECMAC\n"
        "Ordering: LECMAC (proposed) > LEACH > ES-MAC across all layouts",
        fontsize=13, fontweight="bold", y=0.98)

    gs = gridspec.GridSpec(2, 2, figure=fig, hspace=0.38, wspace=0.3)
    pos = [(0,0), (0,1), (1,0), (1,1)]

    for i, lid in enumerate(layouts):
        ax = fig.add_subplot(gs[pos[i]])
        plot_layout(ax, lid, results_dir, thin=5, show_fnd_lnd=True)

    # Shared legend at bottom
    legend_elements = [
        Line2D([0],[0], color=STYLE[p]["color"], lw=2,
               ls=STYLE[p]["ls"], label=STYLE[p]["label"])
        for p in ("esmac","leach","lecmac")
    ]
    fig.legend(handles=legend_elements,
               loc="lower center", ncol=3,
               fontsize=11, bbox_to_anchor=(0.5, 0.01),
               frameon=True, edgecolor="gray")

    out_path = os.path.join(results_dir, "all_layouts.png")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out_path}")

# ── Summary table ─────────────────────────────────────────────────────────────
def load_paper_values(results_dir):
    """Load paper FND/LND from summary.csv keyed by (layout, proto)."""
    csv_path = os.path.join(results_dir, "summary.csv")
    paper = {}
    if not os.path.exists(csv_path):
        return paper
    with open(csv_path) as f:
        header = None
        for line in f:
            line = line.strip()
            if not line:
                continue
            if header is None:
                header = line.split(",")
                continue
            parts = line.split(",")
            if len(parts) < 6:
                continue
            try:
                layout  = int(parts[0])
                proto   = parts[1].strip().lower()   # ESMAC → esmac
                fnd_p   = int(parts[4])
                lnd_p   = int(parts[5])
                paper[(layout, proto)] = (fnd_p, lnd_p)
            except ValueError:
                continue
    return paper

def make_summary_table(results_dir, layouts=(1,2,3,4)):
    protocols   = ("esmac", "leach", "lecmac")
    proto_names = {"esmac": "ES-MAC", "leach": "LEACH", "lecmac": "LECMAC"}

    paper = load_paper_values(results_dir)

    W = 100  # total line width
    lines = []
    lines.append("=" * W)
    lines.append("  WSN PROTOCOL COMPARISON — FIRST/LAST NODE DEATH ROUNDS (Sim vs Paper)")
    lines.append("=" * W)
    hdr = (f"{'Layout':<30} {'Protocol':<8}"
           f" {'SimFND':>7} {'PprFND':>7} {'ΔFND':>6}"
           f" {'SimLND':>7} {'PprLND':>7} {'ΔLND':>6}")
    lines.append(hdr)
    lines.append("-" * W)

    all_ok = True
    for lid in layouts:
        for proto in protocols:
            path = os.path.join(results_dir, f"layout{lid}", f"{proto}.txt")
            data = parse_file(path)

            pname = proto_names[proto]
            label = LAYOUT_LABELS[lid]

            # Paper values (may be missing)
            ppr_key = (lid, proto)
            ppr_fnd, ppr_lnd = paper.get(ppr_key, (None, None))

            if data is None:
                lines.append(
                    f"  {label:<28} {pname:<8}"
                    f" {'N/A':>7} {str(ppr_fnd) if ppr_fnd else 'N/A':>7} {'—':>6}"
                    f" {'N/A':>7} {str(ppr_lnd) if ppr_lnd else 'N/A':>7} {'—':>6}")
                all_ok = False
                continue

            rounds, dead, _, _ = data
            sim_fnd, sim_lnd = fnd_lnd(rounds, dead)

            # Compute deltas (sim − paper, signed)
            if ppr_fnd is not None and sim_fnd > 0:
                d_fnd = f"{sim_fnd - ppr_fnd:+d}"
            else:
                d_fnd = "—"

            if ppr_lnd is not None and sim_lnd > 0:
                d_lnd = f"{sim_lnd - ppr_lnd:+d}"
            else:
                d_lnd = "—"

            ppr_fnd_str = str(ppr_fnd) if ppr_fnd is not None else "N/A"
            ppr_lnd_str = str(ppr_lnd) if ppr_lnd is not None else "N/A"

            lines.append(
                f"  {label:<28} {pname:<8}"
                f" {sim_fnd:>7} {ppr_fnd_str:>7} {d_fnd:>6}"
                f" {sim_lnd:>7} {ppr_lnd_str:>7} {d_lnd:>6}")
        lines.append("")

    lines.append("=" * W)
    lines.append("FND = First Node Death round   LND = Last Node Death round   Δ = Sim − Paper")
    lines.append("Expected ordering (FND): ES-MAC < LEACH < LECMAC")
    lines.append("=" * W)

    out_path = os.path.join(results_dir, "summary_table.txt")
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    for line in lines:
        print(line)
    print(f"\n  Table saved: {out_path}")
    return all_ok

# ── Ordering validation ────────────────────────────────────────────────────────
def validate_ordering(results_dir, layouts=(1,2,3,4)):
    print("\n── Ordering Validation ─────────────────────────────")
    all_pass = True
    for lid in layouts:
        fnds = {}
        for proto in ("esmac","leach","lecmac"):
            path = os.path.join(results_dir, f"layout{lid}", f"{proto}.txt")
            data = parse_file(path)
            if data is None: continue
            rounds, dead, _, _ = data
            fnds[proto], _ = fnd_lnd(rounds, dead)
        if len(fnds) == 3:
            ok = fnds["esmac"] < fnds["leach"] < fnds["lecmac"]
            status = "✓ PASS" if ok else "✗ FAIL"
            if not ok: all_pass = False
            print(f"  Layout {lid}: ES-MAC FND={fnds['esmac']}  "
                  f"LEACH FND={fnds['leach']}  "
                  f"LECMAC FND={fnds['lecmac']}  → {status}")
        else:
            print(f"  Layout {lid}: insufficient data")
    print("────────────────────────────────────────────────────")
    if all_pass:
        print("  ALL LAYOUTS PASS ✓")
    else:
        print("  WARNING: Some layouts have incorrect ordering ✗")
    return all_pass

# ─── Entry point ──────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Plot WSN simulation results")
    parser.add_argument("--results_dir", default="results",
                        help="Root results directory (default: results/)")
    parser.add_argument("--layouts", nargs="+", type=int,
                        default=[1,2,3,4], help="Which layouts to process")
    args = parser.parse_args()

    rd = args.results_dir
    layouts = tuple(args.layouts)

    print(f"\nPlotting results from: {rd}")
    print(f"Layouts: {layouts}\n")

    make_per_layout_figures(rd, layouts)
    make_combined_figure(rd, layouts)
    make_summary_table(rd, layouts)
    validate_ordering(rd, layouts)

    print("\nDone. Generated files:")
    for lid in layouts:
        print(f"  results/layout{lid}/comparison.png")
        print(f"  results/layout{lid}/energy.png")
    print("  results/all_layouts.png")
    print("  results/summary_table.txt")

if __name__ == "__main__":
    main()