#!/usr/bin/env python3
"""
parse_traces.py
===============
Parses the raw simulation result files produced by lecmac-sim, leach-sim,
and esmac-sim and prints a formatted summary table.

File format (one line per round):
    # round dead_nodes alive_nodes      ← header / comment lines ignored
    1 0 100
    2 0 100
    ...

Usage
-----
    python3 analysis/parse_traces.py                 # all 4 layouts
    python3 analysis/parse_traces.py --layout 1      # single layout
    python3 analysis/parse_traces.py --layout 2 --csv results/my_summary.csv
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import NamedTuple

# ── Types ─────────────────────────────────────────────────────────────────────

class RoundRecord(NamedTuple):
    round: int
    dead: int
    alive: int


class ProtocolStats(NamedTuple):
    protocol: str
    layout: int
    first_dead_round: int | None   # first round where dead > 0
    last_dead_round: int | None    # last round logged (all-dead or sim end)
    total_rounds: int
    max_dead: int


# ── IO ────────────────────────────────────────────────────────────────────────

def read_result_file(path: Path) -> list[RoundRecord]:
    """Read a result .txt file and return a list of RoundRecord."""
    records: list[RoundRecord] = []
    if not path.exists():
        return records
    with path.open(encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 3:
                continue
            try:
                records.append(RoundRecord(int(parts[0]), int(parts[1]), int(parts[2])))
            except ValueError:
                continue
    return records


# ── Analysis ──────────────────────────────────────────────────────────────────

def compute_stats(protocol: str, layout: int, records: list[RoundRecord]) -> ProtocolStats:
    if not records:
        return ProtocolStats(protocol, layout, None, None, 0, 0)

    first_dead_round: int | None = None
    last_dead_round: int | None = None
    max_dead = 0

    for rec in records:
        if first_dead_round is None and rec.dead > 0:
            first_dead_round = rec.round
        if rec.dead > 0:
            last_dead_round = rec.round
        if rec.dead > max_dead:
            max_dead = rec.dead

    total_rounds = records[-1].round
    return ProtocolStats(protocol, layout, first_dead_round, last_dead_round, total_rounds, max_dead)


# ── Formatting ────────────────────────────────────────────────────────────────

PROTOCOLS = ["leach", "esmac", "lecmac"]
PROTOCOL_DISPLAY = {"leach": "LEACH", "esmac": "ES-MAC", "lecmac": "LECMAC"}

# Expected ordering from the paper (for validation annotation)
PAPER_FIRST = {
    # layout: {protocol: round}
    1: {"esmac": 821,  "leach": 2272, "lecmac": 3269},
    2: {"esmac": 718,  "leach": 1890, "lecmac": 2673},
    3: {"esmac": 681,  "leach": 2064, "lecmac": 2527},
    4: {"esmac": 790,  "leach": 1975, "lecmac": 2732},
}
PAPER_LAST = {
    1: {"esmac": 970,  "leach": 4080, "lecmac": 4890},
    2: {"esmac": 1579, "leach": 3200, "lecmac": 4685},
    3: {"esmac": 1360, "leach": 4090, "lecmac": 4250},
    4: {"esmac": 1618, "leach": 3329, "lecmac": 4750},
}


def _na(val: int | None) -> str:
    return str(val) if val is not None else "N/A"


def print_layout_table(layout: int, stats_map: dict[str, ProtocolStats]) -> None:
    print(f"\n{'─'*72}")
    print(f"  Layout {layout}  ─  simulation results vs. paper")
    print(f"{'─'*72}")
    print(f"  {'Protocol':<10} {'First Dead':>12} {'Paper':>8}  {'Last Dead':>12} {'Paper':>8}")
    print(f"  {'-'*10} {'-'*12} {'-'*8}  {'-'*12} {'-'*8}")

    ordering_ok = True
    for proto in PROTOCOLS:
        s = stats_map.get(proto)
        pf = PAPER_FIRST.get(layout, {}).get(proto, "?")
        pl = PAPER_LAST.get(layout, {}).get(proto, "?")
        first_s = _na(s.first_dead_round) if s else "N/A"
        last_s = _na(s.last_dead_round) if s else "N/A"
        print(f"  {PROTOCOL_DISPLAY[proto]:<10} {first_s:>12} {str(pf):>8}  {last_s:>12} {str(pl):>8}")

    # Check LECMAC > LEACH > ES-MAC for first dead
    lecmac = stats_map.get("lecmac")
    leach = stats_map.get("leach")
    esmac = stats_map.get("esmac")
    if lecmac and leach and esmac:
        lf = lecmac.first_dead_round or float('inf')
        lhf = leach.first_dead_round or float('inf')
        ef = esmac.first_dead_round or float('inf')
        ll = lecmac.last_dead_round or float('inf')
        lhl = leach.last_dead_round or float('inf')
        el = esmac.last_dead_round or float('inf')
        order_first = lf >= lhf >= ef
        order_last = ll >= lhl >= el
        ordering_ok = order_first and order_last
        status = "✅ PASS" if ordering_ok else "❌ FAIL"
        print(f"\n  Ordering (LECMAC ≥ LEACH ≥ ES-MAC): {status}")
        if not ordering_ok:
            print("  ⚠  Expected LECMAC > LEACH > ES-MAC in both first and last dead round.")
    print()


def write_csv(all_stats: list[ProtocolStats], csv_path: Path) -> None:
    lines = ["layout,protocol,first_dead_round,last_dead_round,total_rounds_logged,max_dead"]
    for s in all_stats:
        lines.append(
            f"{s.layout},{PROTOCOL_DISPLAY[s.protocol]},"
            f"{_na(s.first_dead_round)},{_na(s.last_dead_round)},"
            f"{s.total_rounds},{s.max_dead}"
        )
    csv_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"CSV written → {csv_path}")


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description="Parse LECMAC/LEACH/ES-MAC result files.")
    parser.add_argument("--layout", type=int, default=0,
                        help="Layout to parse (1-4). Default: all layouts.")
    parser.add_argument("--csv", type=str, default="",
                        help="Optional path to write a CSV summary.")
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    results_dir = root / "results"

    layouts = [args.layout] if 1 <= args.layout <= 4 else list(range(1, 5))

    all_stats: list[ProtocolStats] = []

    for layout_id in layouts:
        layout_dir = results_dir / f"layout{layout_id}"
        stats_map: dict[str, ProtocolStats] = {}
        for proto in PROTOCOLS:
            path = layout_dir / f"{proto}.txt"
            records = read_result_file(path)
            if not records:
                print(f"  ⚠  {path} not found or empty — run the simulation first.", file=sys.stderr)
            s = compute_stats(proto, layout_id, records)
            stats_map[proto] = s
            all_stats.append(s)
        print_layout_table(layout_id, stats_map)

    if args.csv:
        write_csv(all_stats, Path(args.csv))
    elif layouts == list(range(1, 5)):
        # Auto-write summary alongside results
        write_csv(all_stats, results_dir / "summary_parsed.csv")


if __name__ == "__main__":
    main()
