#!/usr/bin/env python3
"""
plot_results.py — WSN Protocol Comparison Plotter
===================================================
Reads simulation result files and generates:
  • Per-layout dead-node comparison graphs  (results/layoutN/comparison.png)
  • Per-layout energy decay graphs          (results/layoutN/energy.png)
  • Combined 2×2 figure                     (results/all_layouts.png)
  • Summary table                           (stdout + results/summary_table.txt)
  • Ordering validation PASS/FAIL           (stdout)
  • summary.csv                             (results/summary.csv)

Output file format (one line per round):
  # round dead_nodes alive_nodes ch_count total_energy_J
  1 0 100 6 499.9712

Usage
-----
    python3 analysis/plot_results.py                      # all layouts
    python3 analysis/plot_results.py --layouts 1 2        # specific layouts
    python3 analysis/plot_results.py --results_dir /path  # custom results dir
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import NamedTuple

import matplotlib
matplotlib.use("Agg")          # non-interactive; works without a display
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from matplotlib.lines import Line2D
import numpy as np

# ── Layout metadata ───────────────────────────────────────────────────────────
LAYOUT_LABELS = {
    1: "Layout 1 — 100 nodes, 100×100 m²",
    2: "Layout 2 — 200 nodes, 100×100 m²",
    3: "Layout 3 — 100 nodes, 200×200 m²",
    4: "Layout 4 — 200 nodes, 200×200 m²",
}

# Paper reference values (Pedditi & Debasis, IEEE AISP 2022)
PAPER_FND = {
    1: {"esmac": 821,  "leach": 2272, "lecmac": 3269},
    2: {"esmac": 718,  "leach": 1890, "lecmac": 2673},
    3: {"esmac": 681,  "leach": 2064, "lecmac": 2527},
    4: {"esmac": 790,  "leach": 1975, "lecmac": 2732},
}
PAPER_LND = {
    1: {"esmac": 970,  "leach": 4080, "lecmac": 4890},
    2: {"esmac": 1579, "leach": 3200, "lecmac": 4685},
    3: {"esmac": 1360, "leach": 4090, "lecmac": 4250},
    4: {"esmac": 1618, "leach": 3329, "lecmac": 4750},
}

# Protocol display styles
STYLE = {
    "esmac":  {"label": "ES-MAC",  "color": "#2196F3", "lw": 2.0, "ls": "--"},
    "leach":  {"label": "LEACH",   "color": "#FF9800", "lw": 2.0, "ls": "-."},
    "lecmac": {"label": "LECMAC",  "color": "#4CAF50", "lw": 2.5, "ls": "-"},
}
PROTOCOLS = ("esmac", "leach", "lecmac")


# ── Data loading ──────────────────────────────────────────────────────────────
class SimData(NamedTuple):
    rounds:     np.ndarray   # shape (N,)
    dead:       np.ndarray   # shape (N,)
    alive:      np.ndarray   # shape (N,)  — may be all NaN if col missing
    energy:     np.ndarray   # shape (N,)  — may be all NaN if col missing


def parse_file(path: Path) -> SimData | None:
    """Parse a simulation output file. Returns None if file missing/empty."""
    if not path.exists():
        return None
    rounds, dead, alive, energy = [], [], [], []
    with path.open() as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            try:
                rounds.append(int(parts[0]))
                dead.append(int(parts[1]))
                alive.append(int(parts[2]) if len(parts) > 2 else 0)
                energy.append(float(parts[4]) if len(parts) > 4 else float("nan"))
            except (ValueError, IndexError):
                continue
    if not rounds:
        return None
    return SimData(
        rounds=np.array(rounds, dtype=float),
        dead=np.array(dead, dtype=float),
        alive=np.array(alive, dtype=float),
        energy=np.array(energy, dtype=float),
    )


def fnd_lnd(data: SimData) -> tuple[int, int]:
    """Return (First Node Death round, Last Node Death round)."""
    fnd, lnd = 0, 0
    for r, d in zip(data.rounds, data.dead):
        if fnd == 0 and d > 0:
            fnd = int(r)
        if d > 0:
            lnd = int(r)
    return fnd, lnd


# ── Per-layout dead-node comparison plot ──────────────────────────────────────
def plot_dead_node_comparison(ax: plt.Axes,
                               layout_id: int,
                               results_dir: Path,
                               thin: int = 5,
                               show_fnd_lnd: bool = False) -> dict[str, tuple[int, int]]:
    """Plot dead-node curves for all 3 protocols on ax. Returns {proto: (fnd, lnd)}."""
    ax.set_title(LAYOUT_LABELS[layout_id], fontsize=10)
    ax.set_xlabel("Number of Rounds", fontsize=9)
    ax.set_ylabel("Number of Dead Nodes", fontsize=9)
    ax.grid(True, alpha=0.3, linestyle=":")

    info = {}
    for proto in PROTOCOLS:
        path = results_dir / f"layout{layout_id}" / f"{proto}.txt"
        data = parse_file(path)
        if data is None:
            continue
        st = STYLE[proto]
        # Thin the data for cleaner rendering
        idx = np.arange(0, len(data.rounds), thin)
        ax.plot(data.rounds[idx], data.dead[idx],
                color=st["color"], lw=st["lw"], ls=st["ls"],
                label=st["label"])
        fnd, lnd = fnd_lnd(data)
        info[proto] = (fnd, lnd)

        if show_fnd_lnd and fnd > 0:
            ax.axvline(x=fnd, color=st["color"], lw=0.7, ls=":", alpha=0.7)

    ax.legend(fontsize=8, loc="upper left")
    return info


# ── Per-layout energy decay plot ──────────────────────────────────────────────
def plot_energy_decay(ax: plt.Axes, layout_id: int, results_dir: Path,
                      thin: int = 10) -> None:
    """Plot total residual energy over rounds for all 3 protocols on ax."""
    ax.set_title(f"Energy Decay — {LAYOUT_LABELS[layout_id]}", fontsize=10)
    ax.set_xlabel("Number of Rounds", fontsize=9)
    ax.set_ylabel("Total Residual Energy (J)", fontsize=9)
    ax.grid(True, alpha=0.3, linestyle=":")

    for proto in PROTOCOLS:
        path = results_dir / f"layout{layout_id}" / f"{proto}.txt"
        data = parse_file(path)
        if data is None or np.all(np.isnan(data.energy)):
            continue
        st = STYLE[proto]
        idx = np.arange(0, len(data.rounds), thin)
        ax.plot(data.rounds[idx], data.energy[idx],
                color=st["color"], lw=st["lw"], ls=st["ls"],
                label=st["label"])
    ax.legend(fontsize=8, loc="upper right")


# ── Individual layout figures ─────────────────────────────────────────────────
def make_per_layout_figures(results_dir: Path, layouts: tuple) -> dict:
    """Generate dead-node + energy figures per layout. Returns all FND/LND info."""
    all_info = {}
    for lid in layouts:
        # Dead-node comparison
        fig, ax = plt.subplots(figsize=(9, 5.5))
        info = plot_dead_node_comparison(ax, lid, results_dir, thin=8)
        fig.suptitle(
            f"WSN Node Lifetime — {LAYOUT_LABELS[lid]}\n"
            f"Ordering: ES-MAC < LEACH < LECMAC (First Node Death)",
            fontsize=11, y=1.01)
        plt.tight_layout()
        out = results_dir / f"layout{lid}" / "comparison.png"
        out.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(out, dpi=160, bbox_inches="tight")
        plt.close(fig)
        print(f"  Saved: {out}")

        # Energy decay
        fig, ax = plt.subplots(figsize=(9, 4))
        plot_energy_decay(ax, lid, results_dir, thin=8)
        plt.tight_layout()
        out = results_dir / f"layout{lid}" / "energy.png"
        fig.savefig(out, dpi=160, bbox_inches="tight")
        plt.close(fig)
        print(f"  Saved: {out}")

        if info:
            all_info[lid] = info
    return all_info


# ── Combined 2×2 figure ────────────────────────────────────────────────────────
def make_combined_figure(results_dir: Path, layouts: tuple) -> None:
    """Generate a 2×2 grid of dead-node comparison plots."""
    fig = plt.figure(figsize=(14, 10))
    fig.suptitle(
        "WSN Network Lifetime Comparison: ES-MAC vs LEACH vs LECMAC\n"
        "Proposal: LECMAC (Pedditi & Debasis, IEEE AISP 2022) achieves the longest lifetime",
        fontsize=13, fontweight="bold", y=0.985)

    gs = gridspec.GridSpec(2, 2, figure=fig, hspace=0.38, wspace=0.30)
    positions = [(0, 0), (0, 1), (1, 0), (1, 1)]

    for i, lid in enumerate(layouts[:4]):
        row, col = positions[i]
        ax = fig.add_subplot(gs[row, col])
        plot_dead_node_comparison(ax, lid, results_dir, thin=5, show_fnd_lnd=True)

    # Shared legend at figure bottom
    legend_elements = [
        Line2D([0], [0], color=STYLE[p]["color"], lw=2,
               ls=STYLE[p]["ls"], label=STYLE[p]["label"])
        for p in PROTOCOLS
    ]
    fig.legend(handles=legend_elements,
               loc="lower center", ncol=3, fontsize=11,
               bbox_to_anchor=(0.5, 0.01), frameon=True, edgecolor="gray")

    out = results_dir / "all_layouts.png"
    fig.savefig(out, dpi=160, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out}")


# ── Summary table ─────────────────────────────────────────────────────────────
def make_summary_table(results_dir: Path, layouts: tuple) -> tuple[dict, bool]:
    """Print and save the FND/LND summary table. Returns (all_info, all_pass)."""
    all_info = {}
    all_pass = True

    lines = []
    lines.append("=" * 80)
    lines.append("  WSN PROTOCOL COMPARISON — FIRST/LAST NODE DEATH ROUNDS")
    lines.append("  Paper: Pedditi & Debasis, IEEE AISP 2022")
    lines.append("=" * 80)
    hdr = f"  {'Layout':<32} {'Protocol':<10} {'FND(sim)':>9} {'FND(paper)':>10} {'LND(sim)':>9} {'LND(paper)':>10}"
    lines.append(hdr)
    lines.append("-" * 80)

    for lid in layouts:
        info = {}
        for proto in PROTOCOLS:
            path = results_dir / f"layout{lid}" / f"{proto}.txt"
            data = parse_file(path)
            if data is None:
                lines.append(
                    f"  {LAYOUT_LABELS[lid]:<32} {proto.upper():<10} {'N/A':>9} {str(PAPER_FND[lid][proto]):>10} {'N/A':>9} {str(PAPER_LND[lid][proto]):>10}")
                all_pass = False
                continue
            f, l = fnd_lnd(data)
            info[proto] = (f, l)
            pf = PAPER_FND[lid][proto]
            pl = PAPER_LND[lid][proto]
            lines.append(
                f"  {LAYOUT_LABELS[lid]:<32} {proto.upper():<10} {f:>9} {pf:>10} {l:>9} {pl:>10}")
        lines.append("")
        if info:
            all_info[lid] = info

    lines.append("=" * 80)
    lines.append("  FND = First Node Death round | LND = Last Node Death round")
    lines.append("  Expected ordering (FND and LND): ES-MAC < LEACH < LECMAC")
    lines.append("=" * 80)

    text = "\n".join(lines)
    print(f"\n{text}\n")

    out = results_dir / "summary_table.txt"
    out.write_text(text + "\n", encoding="utf-8")
    print(f"  Saved: {out}")

    return all_info, all_pass


# ── Ordering validation ────────────────────────────────────────────────────────
def validate_ordering(all_info: dict, layouts: tuple) -> bool:
    """Check LECMAC FND > LEACH FND > ES-MAC FND for every layout."""
    print("\n── Ordering Validation ──────────────────────────────────────────")
    all_pass = True
    for lid in layouts:
        info = all_info.get(lid, {})
        if not info or len(info) < 3:
            print(f"  Layout {lid}: insufficient data — skipping")
            continue
        fnd = {p: info[p][0] for p in PROTOCOLS if p in info}
        lnd = {p: info[p][1] for p in PROTOCOLS if p in info}
        fnd_ok = fnd.get("esmac", 0) < fnd.get("leach", 0) < fnd.get("lecmac", 0)
        lnd_ok = lnd.get("esmac", 0) < lnd.get("leach", 0) < lnd.get("lecmac", 0)
        ok = fnd_ok and lnd_ok
        if not ok:
            all_pass = False
        status = "✓ PASS" if ok else "✗ FAIL"
        print(f"  Layout {lid}: ES-MAC FND={fnd.get('esmac','?')}  "
              f"LEACH FND={fnd.get('leach','?')}  "
              f"LECMAC FND={fnd.get('lecmac','?')}  → {status}")
    print("──────────────────────────────────────────────────────────────────")
    if all_pass:
        print("  ALL LAYOUTS PASS ✓  LECMAC > LEACH > ES-MAC ordering confirmed\n")
    else:
        print("  WARNING: Some layouts have incorrect ordering ✗\n")
    return all_pass


# ── CSV export ────────────────────────────────────────────────────────────────
def write_csv(results_dir: Path, all_info: dict, layouts: tuple) -> None:
    lines = ["layout,protocol,fnd_sim,lnd_sim,fnd_paper,lnd_paper"]
    for lid in layouts:
        for proto in PROTOCOLS:
            f, l = all_info.get(lid, {}).get(proto, (0, 0))
            pf = PAPER_FND[lid][proto]
            pl = PAPER_LND[lid][proto]
            lines.append(f"{lid},{proto.upper()},{f},{l},{pf},{pl}")
    out = results_dir / "summary.csv"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"  Saved: {out}")


# ── Entry point ───────────────────────────────────────────────────────────────
def main() -> None:
    parser = argparse.ArgumentParser(description="Plot WSN simulation results")
    parser.add_argument("--results_dir", default="",
                        help="Root results directory (default: <script>/../results)")
    parser.add_argument("--layouts", nargs="+", type=int, default=[1, 2, 3, 4],
                        help="Which layouts to process (default: 1 2 3 4)")
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    root = script_dir.parent
    results_dir = Path(args.results_dir) if args.results_dir else root / "results"
    layouts = tuple(args.layouts)

    print(f"\nPlotting results from: {results_dir}")
    print(f"Layouts: {layouts}\n")

    if not results_dir.exists():
        print(f"ERROR: results directory not found: {results_dir}", file=sys.stderr)
        sys.exit(1)

    print("── Generating per-layout figures ────────────────────────────────")
    make_per_layout_figures(results_dir, layouts)

    print("\n── Generating combined 2×2 figure ───────────────────────────────")
    make_combined_figure(results_dir, layouts)

    print("\n── Summary Table ─────────────────────────────────────────────────")
    all_info, _ = make_summary_table(results_dir, layouts)

    validate_ordering(all_info, layouts)
    write_csv(results_dir, all_info, layouts)

    print("\nDone. Output files:")
    for lid in layouts:
        print(f"  results/layout{lid}/comparison.png")
        print(f"  results/layout{lid}/energy.png")
    print("  results/all_layouts.png")
    print("  results/summary_table.txt")
    print("  results/summary.csv")


if __name__ == "__main__":
    main()
