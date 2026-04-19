#!/usr/bin/env bash
# =============================================================================
# run_all_simulations.sh — LECMAC WSN Project
# Runs all 12 simulations (3 protocols × 4 layouts) and generates plots.
#
# Usage (from ns-3-dev/ directory):
#   bash run_all_simulations.sh              # all layouts, 6000 rounds
#   bash run_all_simulations.sh --layout 1   # single layout only
#   bash run_all_simulations.sh --rounds 3000 # custom round count
#
# On Mac: ensure PATH includes homebrew cmake before running.
# =============================================================================

set -euo pipefail

# ── Colour helpers ─────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'

info()    { echo -e "${CYAN}[INFO]${RESET}  $*"; }
success() { echo -e "${GREEN}[OK]${RESET}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
error()   { echo -e "${RED}[ERROR]${RESET} $*" >&2; }

# ── Defaults ──────────────────────────────────────────────────────────────────
LAYOUTS=(1 2 3 4)
ROUNDS=6000
PARALLEL=true   # run all layouts of a protocol in parallel

# ── Parse arguments ───────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --layout)   LAYOUTS=("$2"); shift 2 ;;
        --rounds)   ROUNDS="$2";    shift 2 ;;
        --no-parallel) PARALLEL=false; shift ;;
        *) warn "Unknown argument: $1"; shift ;;
    esac
done

# ── Locate ns3 script ────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NS3_DIR="$SCRIPT_DIR"

if [[ ! -f "$NS3_DIR/ns3" ]]; then
    error "ns3 script not found in $NS3_DIR"
    error "Run this script from the ns-3-dev/ directory."
    exit 1
fi

# Add Homebrew cmake to PATH on Mac if needed
if [[ "$(uname)" == "Darwin" ]] && command -v /opt/homebrew/bin/cmake &>/dev/null; then
    export PATH="/opt/homebrew/bin:$PATH"
fi

# ── Build all 3 protocols ─────────────────────────────────────────────────────
echo -e "\n${BOLD}=============================================${RESET}"
echo -e "${BOLD}  LECMAC WSN Simulation — Full Run${RESET}"
echo -e "${BOLD}  Layouts: ${LAYOUTS[*]} | Rounds: $ROUNDS${RESET}"
echo -e "${BOLD}=============================================${RESET}\n"

info "Building simulation binaries…"
cd "$NS3_DIR"
./ns3 build scratch/lecmac-sim scratch/leach-sim scratch/esmac-sim
success "Build complete."

# ── Create results directories ─────────────────────────────────────────────────
for layout in "${LAYOUTS[@]}"; do
    mkdir -p "results/layout${layout}"
done

# ── Run simulations ───────────────────────────────────────────────────────────
PROTOCOLS=("lecmac" "leach" "esmac")
declare -A PIDS

start_time=$(date +%s)

echo -e "\n${BOLD}Running simulations…${RESET}"
echo    "  (This may take several minutes per layout)"
echo

for proto in "${PROTOCOLS[@]}"; do
    info "Starting ${proto^^} — layouts: ${LAYOUTS[*]}"
    PIDS[$proto]=""

    for layout in "${LAYOUTS[@]}"; do
        log_file="/tmp/${proto}_l${layout}.log"

        # Extra args for LECMAC (disable animation for faster run)
        extra_args=""
        if [[ "$proto" == "lecmac" ]]; then
            extra_args="--enableAnim=false"
        fi

        cmd="./ns3 run \"scratch/${proto}-sim --layout=${layout} --rounds=${ROUNDS} ${extra_args}\""

        if $PARALLEL; then
            eval "$cmd" > "$log_file" 2>&1 &
            PIDS[$proto]+=" $!"
        else
            eval "$cmd" > "$log_file" 2>&1
            result_file="results/layout${layout}/${proto}.txt"
            lines=$(wc -l < "$result_file" 2>/dev/null || echo 0)
            success "${proto^^} layout${layout}: ${lines} rounds logged"
        fi
    done
done

if $PARALLEL; then
    # Wait for all background jobs
    ALL_PIDS=""
    for proto in "${PROTOCOLS[@]}"; do
        ALL_PIDS+="${PIDS[$proto]} "
    done
    info "Waiting for all ${#PROTOCOLS[@]} × ${#LAYOUTS[@]} = $((${#PROTOCOLS[@]} * ${#LAYOUTS[@]})) simulations…"
    for pid in $ALL_PIDS; do
        wait "$pid" 2>/dev/null || true
    done
fi

# ── Check results ──────────────────────────────────────────────────────────────
echo -e "\n${BOLD}Simulation results:${RESET}"
all_ok=true
for layout in "${LAYOUTS[@]}"; do
    for proto in "${PROTOCOLS[@]}"; do
        f="results/layout${layout}/${proto}.txt"
        if [[ -f "$f" ]]; then
            lines=$(grep -c "^[0-9]" "$f" 2>/dev/null || echo 0)
            success "L${layout} ${proto^^}: ${lines} rounds logged → $f"
        else
            error "L${layout} ${proto^^}: output file missing! Check /tmp/${proto}_l${layout}.log"
            all_ok=false
        fi
    done
done

if ! $all_ok; then
    warn "Some simulations may have failed. Check logs in /tmp/*.log"
fi

end_time=$(date +%s)
elapsed=$(( end_time - start_time ))
echo -e "\n  Total simulation time: ${elapsed}s"

# ── Plot results ──────────────────────────────────────────────────────────────
echo -e "\n${BOLD}Generating graphs and summary table…${RESET}"

layout_args=""
for l in "${LAYOUTS[@]}"; do layout_args+=" $l"; done

python3 analysis/plot_results.py --layouts $layout_args

# ── Final summary ──────────────────────────────────────────────────────────────
echo -e "\n${BOLD}=============================================${RESET}"
echo -e "${BOLD}  All done!${RESET}"
echo -e "${BOLD}=============================================${RESET}\n"
echo "Output files:"
for layout in "${LAYOUTS[@]}"; do
    echo "  results/layout${layout}/comparison.png"
    echo "  results/layout${layout}/energy.png"
done
echo "  results/all_layouts.png"
echo "  results/summary_table.txt"
echo "  results/summary.csv"
echo
echo "To check ordering, run: python3 analysis/plot_results.py"
