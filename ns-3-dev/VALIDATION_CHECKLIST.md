# LECMAC — Validation Checklist

Use this before submitting or presenting to your professor.

---

## Phase 1: Build Validation ✅

| Check | Status |
|---|---|
| `cmake --version` shows 3.13+ | ☐ |
| `./ns3 build` completes without errors | ☐ |
| `./ns3 run hello-simulator` prints "Hello Simulator" | ☐ |

```bash
export PATH="/opt/homebrew/bin:$PATH"
cd ~/Desktop/dc_proj/ns-3-dev
./ns3 build
./ns3 run hello-simulator
```

---

## Phase 2: Individual Simulation Tests ✅

Run one layout to verify correctness before running all 12:

```bash
export PATH="/opt/homebrew/bin:$PATH"
cd ~/Desktop/dc_proj/ns-3-dev
./ns3 run "scratch/lecmac-sim --layout=1 --rounds=6000 --enableAnim=false"
./ns3 run "scratch/leach-sim --layout=1 --rounds=6000"
./ns3 run "scratch/esmac-sim --layout=1 --rounds=6000"
```

**Verify output files exist:**
```bash
ls -la results/layout1/*.txt
```
Expected:  `esmac.txt   leach.txt   lecmac.txt` (each ~6000 lines)

**Quick FND check:**
```bash
for p in lecmac leach esmac; do
  fnd=$(awk 'NF>=2 && $1+0>0 && $2+0>0 {print $1; exit}' results/layout1/${p}.txt 2>/dev/null)
  echo "$p: FND=${fnd:-none}"
done
```

Expected ordering: `esmac FND < leach FND < lecmac FND`

| Simulation | Status | FND |
|---|---|---|
| lecmac-sim --layout=1 | ☐ | ___ |
| leach-sim --layout=1 | ☐ | ___ |
| esmac-sim --layout=1 | ☐ | ___ |

---

## Phase 3: Full Run (All 12 Simulations) ✅

```bash
export PATH="/opt/homebrew/bin:$PATH"
cd ~/Desktop/dc_proj/ns-3-dev
bash run_all_simulations.sh
```

Or manually, 12 runs:

| Simulation | Status | Output file |
|---|---|---|
| lecmac-sim --layout=1 | ☐ | results/layout1/lecmac.txt |
| leach-sim --layout=1 | ☐ | results/layout1/leach.txt |
| esmac-sim --layout=1 | ☐ | results/layout1/esmac.txt |
| lecmac-sim --layout=2 | ☐ | results/layout2/lecmac.txt |
| leach-sim --layout=2 | ☐ | results/layout2/leach.txt |
| esmac-sim --layout=2 | ☐ | results/layout2/esmac.txt |
| lecmac-sim --layout=3 | ☐ | results/layout3/lecmac.txt |
| leach-sim --layout=3 | ☐ | results/layout3/leach.txt |
| esmac-sim --layout=3 | ☐ | results/layout3/esmac.txt |
| lecmac-sim --layout=4 | ☐ | results/layout4/lecmac.txt |
| leach-sim --layout=4 | ☐ | results/layout4/leach.txt |
| esmac-sim --layout=4 | ☐ | results/layout4/esmac.txt |

---

## Phase 4: Generate Graphs ✅

```bash
python3 analysis/plot_results.py
```

| Output | Status |
|---|---|
| results/layout1/comparison.png | ☐ |
| results/layout2/comparison.png | ☐ |
| results/layout3/comparison.png | ☐ |
| results/layout4/comparison.png | ☐ |
| results/layout1/energy.png | ☐ |
| results/layout2/energy.png | ☐ |
| results/layout3/energy.png | ☐ |
| results/layout4/energy.png | ☐ |
| results/all_layouts.png (2×2) | ☐ |
| results/summary_table.txt | ☐ |
| results/summary.csv | ☐ |

---

## Phase 5: Critical Ordering Validation ✅

**MUST ALL PASS before presenting to professor!**

```bash
python3 -c "
import numpy as np
results = {}
for layout in range(1,5):
    for p in ['lecmac','leach','esmac']:
        try:
            d = np.loadtxt(f'results/layout{layout}/{p}.txt', comments='#')
            fnd = int(d[d[:,1]>0, 0][0]) if any(d[:,1]>0) else 0
            lnd = int(d[d[:,1]>0, 0][-1]) if any(d[:,1]>0) else 0
            results[(layout,p)] = (fnd,lnd)
        except: results[(layout,p)] = (0,0)
print('Layout | ES-MAC FND | LEACH FND | LECMAC FND | PASS?')
for l in range(1,5):
    ef,_ = results[(l,'esmac')]; lf,_ = results[(l,'leach')]; cf,_ = results[(l,'lecmac')]
    ok = 0 < ef < lf < cf
    print(f'  L{l}   | {ef:>10} | {lf:>9} | {cf:>10} | {\"✓ PASS\" if ok else \"✗ FAIL\"}')
"
```

| Layout | Check | FND Order | LND Order |
|---|---|---|---|
| Layout 1 | ☐ | LECMAC > LEACH > ES-MAC | LECMAC > LEACH > ES-MAC |
| Layout 2 | ☐ | LECMAC > LEACH > ES-MAC | LECMAC > LEACH > ES-MAC |
| Layout 3 | ☐ | LECMAC > LEACH > ES-MAC | LECMAC > LEACH > ES-MAC |
| Layout 4 | ☐ | LECMAC > LEACH > ES-MAC | LECMAC > LEACH > ES-MAC |

---

## Phase 6: Fill In Results Table ✅

After running simulations, fill in actual FND/LND values:

| Layout | Protocol | Paper FND | **Sim FND** | Paper LND | **Sim LND** |
|---|---|---|---|---|---|
| 1 | ES-MAC | 821 | ___ | 970 | ___ |
| 1 | LEACH | 2272 | ___ | 4080 | ___ |
| 1 | LECMAC | 3269 | ___ | 4890 | ___ |
| 2 | ES-MAC | 718 | ___ | 1579 | ___ |
| 2 | LEACH | 1890 | ___ | 3200 | ___ |
| 2 | LECMAC | 2673 | ___ | 4685 | ___ |
| 3 | ES-MAC | 681 | ___ | 1360 | ___ |
| 3 | LEACH | 2064 | ___ | 4090 | ___ |
| 3 | LECMAC | 2527 | ___ | 4250 | ___ |
| 4 | ES-MAC | 790 | ___ | 1618 | ___ |
| 4 | LEACH | 1975 | ___ | 3329 | ___ |
| 4 | LECMAC | 2732 | ___ | 4750 | ___ |

---

## Phase 7: Graph Visual Check ✅

Open each comparison.png and verify:

| Check | Status |
|---|---|
| 3 protocol lines visible per graph | ☐ |
| LECMAC curve (green) rightmost (dies last) | ☐ |
| LEACH curve (orange) in the middle | ☐ |
| ES-MAC curve (blue) leftmost (dies first) | ☐ |
| X-axis labeled "Number of Rounds" | ☐ |
| Y-axis labeled "Number of Dead Nodes" | ☐ |
| Legend present and readable | ☐ |

---

## Phase 8: NetAnim Animation ✅

```bash
# Run with animation enabled (default for lecmac-sim)
./ns3 run "scratch/lecmac-sim --layout=1"
# Produces: results/layout1/lecmac-animation.xml

# Open in NetAnim GUI:
# File → Open → select lecmac-animation.xml
```

| Check | Status |
|---|---|
| lecmac-animation.xml generated | ☐ |
| Sensor nodes visible (green = alive, red = dead) | ☐ |
| Base station visible | ☐ |

---

## Final Checklist Before Presentation ✅

- [ ] All 12 simulations ran successfully
- [ ] All 4 layouts show LECMAC > LEACH > ES-MAC ordering
- [ ] 4 individual comparison graphs generated
- [ ] 4 individual energy decay graphs generated
- [ ] Combined 2×2 graph (all_layouts.png) generated
- [ ] Summary table printed and saved
- [ ] Can explain the five LECMAC parameters (TE, PT, MCS, VP, DE)
- [ ] Can explain why ES-MAC dies first (re-election without epoch)
- [ ] Can explain the First Order Radio Model formulas
- [ ] NetAnim animation opens and shows node deaths over time

---

## Key Numbers to Remember

| LECMAC Parameter | Value | Purpose |
|---|---|---|
| TE | 0.1 J | Minimum residual energy to become CH |
| PT | 10 m | Minimum distance between two CHs |
| MCS | 15 nodes | Maximum cluster size |
| VP | 160-bit window | CH listens briefly per slot to save idle energy |
| DE | 25–60 m | Member skips TX if event is farther away |

| Energy Constant | Value |
|---|---|
| E_elec | 50 nJ/bit |
| E_amp | 100 pJ/bit/m² |
| E_idle | 40 nJ/bit |
| Initial energy | 5 J per node |
