# LECMAC — WSN Protocol Simulation

**Paper:** A Low Energy Consuming MAC Protocol for Wireless Sensor Networks  
**Authors:** Ramesh Babu Pedditi, Kumar Debasis — VIT-AP University  
**Published:** IEEE AISP 2022 | DOI: 10.1109/AISP53593.2022.9760530  
**Implementation:** NS-3 (C++) with Python post-processing

---

## 1. What This Project Does

This project implements and compares three WSN (Wireless Sensor Network) MAC protocols
using the NS-3 network simulator. The primary goal is to demonstrate that the proposed
**LECMAC** protocol achieves significantly longer network lifetime than LEACH and ES-MAC.

| Protocol | Key Feature | Network Lifetime |
|----------|-------------|-----------------|
| ES-MAC | LEACH + Verification Period (VP) | Dies first |
| LEACH | Epoch-based CH rotation | Dies second |
| **LECMAC (proposed)** | Energy-aware selection + PT + MCS + VP + DE | **Lives longest** |

All simulations use the **First Order Radio Model** (Heinzelman et al.) with
exact parameters from Table 1 of the LECMAC paper.

---

## 2. Project Structure

```
ns-3-dev/
├── scratch/
│   ├── wsn-protocol-model.h    ← Shared simulation engine (all 3 protocols)
│   ├── lecmac-sim.cc           ← LECMAC entry point + NetAnim setup
│   ├── leach-sim.cc            ← LEACH entry point
│   └── esmac-sim.cc            ← ES-MAC entry point
│
├── analysis/
│   ├── plot_results.py         ← Generates graphs + summary table
│   └── parse_traces.py         ← Parses raw output files
│
├── results/
│   ├── layout1/                ← Auto-created by simulations
│   │   ├── lecmac.txt
│   │   ├── leach.txt
│   │   ├── esmac.txt
│   │   ├── comparison.png      ← Dead-node comparison graph
│   │   └── energy.png          ← Energy decay graph
│   ├── layout2/ ... layout4/
│   ├── all_layouts.png         ← Combined 2×2 figure
│   └── summary_table.txt       ← First/last death round table
│
├── run_all_simulations.sh      ← One-command full run
├── README.md                   ← This file
├── FINAL_SUMMARY.md            ← Professor presentation document
└── VALIDATION_CHECKLIST.md     ← Pre-submission checklist
```

---

## 3. Prerequisites (Mac)

NS-3 is already installed in this project. Ensure you have:

```bash
# Verify cmake is available (required by NS-3's build system)
/opt/homebrew/bin/cmake --version
# Should print: cmake version 4.x.x

# Verify Python and matplotlib
python3 -c "import matplotlib, numpy; print('OK')"
# If missing:
pip3 install matplotlib numpy
```

If cmake isn't found, install via Homebrew:
```bash
brew install cmake
```

---

## 4. Building

All commands run from `ns-3-dev/`:
```bash
cd ~/Desktop/dc_proj/ns-3-dev
export PATH="/opt/homebrew/bin:$PATH"   # ensure cmake is found

./ns3 build
```

Build just the simulation scripts (faster):
```bash
./ns3 build scratch/lecmac-sim scratch/leach-sim scratch/esmac-sim
```

---

## 5. Running Simulations

### Option A — One Command (Recommended)
```bash
cd ~/Desktop/dc_proj/ns-3-dev
export PATH="/opt/homebrew/bin:$PATH"
bash run_all_simulations.sh
```

This runs all 12 simulations (3 protocols × 4 layouts) in parallel, then
generates all graphs and the summary table automatically.

### Option B — Individual Runs
```bash
cd ~/Desktop/dc_proj/ns-3-dev
export PATH="/opt/homebrew/bin:$PATH"

# LECMAC — all 4 layouts
./ns3 run "scratch/lecmac-sim --layout=1 --enableAnim=false"
./ns3 run "scratch/lecmac-sim --layout=2 --enableAnim=false"
./ns3 run "scratch/lecmac-sim --layout=3 --enableAnim=false"
./ns3 run "scratch/lecmac-sim --layout=4 --enableAnim=false"

# LEACH
./ns3 run "scratch/leach-sim --layout=1"
# ... etc

# ES-MAC
./ns3 run "scratch/esmac-sim --layout=1"
# ... etc
```

### With NetAnim animation (LECMAC only)
```bash
./ns3 run "scratch/lecmac-sim --layout=1"   # enableAnim=true by default
# Animation → results/layout1/lecmac-animation.xml
```

---

## 6. Generating Graphs

After all simulations complete:
```bash
python3 analysis/plot_results.py
```

**Outputs:**
- `results/layoutN/comparison.png` — Dead-node vs rounds (3 protocol lines)
- `results/layoutN/energy.png` — Total residual energy over rounds
- `results/all_layouts.png` — Combined 2×2 figure for all layouts
- `results/summary_table.txt` — First/last node death table
- `results/summary.csv` — Machine-readable results

**Options:**
```bash
python3 analysis/plot_results.py --layouts 1 2       # specific layouts
python3 analysis/plot_results.py --results_dir /path  # custom directory
```

---

## 7. Energy Model (Table 1 of Paper)

**First Order Radio Model:**

| Parameter | Value | Meaning |
|-----------|-------|---------|
| E_elec | 50 nJ/bit | Electronics energy (Tx and Rx) |
| E_amp | 100 pJ/bit/m² | Amplifier (Tx only, scales with d²) |
| E_idle | 40 nJ/bit | Idle listening energy rate |
| Initial energy | 5 J | Per-node battery |
| Data packet | 800 bits | 100 bytes |
| Control packet | 160 bits | 20 bytes |

**Formulas:**
```
E_tx(k, d)  = k × E_elec + k × E_amp × d²
E_rx(k)     = k × E_elec
E_idle      = E_idle_rate × slot_duration_in_bits
```

---

## 8. LECMAC Parameters

| Parameter | Value | Purpose |
|-----------|-------|---------|
| Threshold Energy (TE) | 0.1 J | Nodes below this cannot be CH |
| Proximity Threshold (PT) | 10 m | No two CHs within 10m of each other |
| Max Cluster Size (MCS) | 15 | CH rejects members beyond 15 |
| Verification Period (VP) | 160-bit window | CH checks for incoming packet before receiving |
| Distance-to-Event (DE) | 30/25/60/50 m | Members skip TX if event is farther away |

---

## 9. Expected Results

The key requirement is: **LECMAC > LEACH > ES-MAC in all 4 layouts.**

Paper reference values (approximate — our NS-3 results may scale proportionally):

| Layout | Protocol | FND (First Dead) | LND (Last Dead) |
|--------|----------|-----------------|-----------------|
| 1 (100n, 100m) | ES-MAC | ~821 | ~970 |
| | LEACH | ~2272 | ~4080 |
| | **LECMAC** | **~3269** | **~4890** |
| 2 (200n, 100m) | ES-MAC | ~718 | ~1579 |
| | LEACH | ~1890 | ~3200 |
| | **LECMAC** | **~2673** | **~4685** |
| 3 (100n, 200m) | ES-MAC | ~681 | ~1360 |
| | LEACH | ~2064 | ~4090 |
| | **LECMAC** | **~2527** | **~4250** |
| 4 (200n, 200m) | ES-MAC | ~790 | ~1618 |
| | LEACH | ~1975 | ~3329 |
| | **LECMAC** | **~2732** | **~4750** |

> **Note:** The paper used Python simulation scripts. Our NS-3 implementation uses
> NS-3's discrete-event scheduler and actual Euclidean distances, which may produce
> proportionally larger round counts while maintaining the correct ordering.

---

## 10. Troubleshooting

### Build fails: `CMake not found`
```bash
export PATH="/opt/homebrew/bin:$PATH"
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

### Build fails: lock file syntax error
```bash
rm -rf cmake-cache && find . -name "*.lock" -delete
export PATH=$(echo $PATH | tr ':' '\n' | grep -v "Unknown" | grep -v "antigravity" | tr '\n' ':' | sed 's/:$//')
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

### Python script fails: `No module named matplotlib`
```bash
pip3 install matplotlib numpy
```

### Results directory not found
```bash
mkdir -p results/layout{1,2,3,4}
```

### Wrong ordering in results
Run quick diagnostic:
```bash
python3 -c "
import numpy as np
for layout in range(1,5):
    for p in ['lecmac','leach','esmac']:
        try:
            d = np.loadtxt(f'results/layout{layout}/{p}.txt', comments='#')
            fnd = d[d[:,1]>0, 0][0] if any(d[:,1]>0) else -1
            print(f'L{layout} {p:8s}: FND={fnd:.0f}')
        except:
            print(f'L{layout} {p:8s}: MISSING or ERROR')
"
```

---

## 11. References

1. R. B. Pedditi and K. Debasis, "A Low Energy Consuming MAC Protocol for Wireless Sensor Networks," *2022 International Conference on Artificial Intelligence and Smart Systems (AISP)*, 2022. DOI: 10.1109/AISP53593.2022.9760530

2. W. B. Heinzelman, A. P. Chandrakasan, and H. Balakrishnan, "An application-specific protocol architecture for wireless microsensor networks," *IEEE Transactions on Wireless Communications*, 2002.

3. NS-3 Consortium, *NS-3 Network Simulator*, https://www.nsnam.org/, 2024.
