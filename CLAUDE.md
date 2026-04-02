# LECMAC NS-3 Project — Claude Context File

## Who I Am
I am Ajitesh, a university student implementing a research paper as a project.
- **Machine:** MacBook Pro M4 chip (Apple Silicon)
- **Package Manager:** Homebrew
- **Shell:** zsh (default on Mac)
- **Project folder:** `~/Desktop/dc_proj/`

---

## The Research Paper

**Title:** A Low Energy Consuming MAC Protocol for Wireless Sensor Networks
**Authors:** Ramesh Babu Pedditi, Kumar Debasis — VIT-AP University
**Published:** IEEE AISP 2022 Conference
**DOI:** 10.1109/AISP53593.2022.9760530

### What the paper proposes
A protocol called **LECMAC** (Low Energy Consuming Medium Access Control) for Wireless Sensor Networks (WSNs). WSNs are networks of small battery-powered sensor nodes. Nodes waste most energy on radio communication, not sensing. LECMAC reduces unnecessary communication to extend battery life (network lifetime).

### The problem LECMAC solves
Existing protocols waste energy through:
- **Idle listening** — radio stays ON even when no data is coming
- **Overhearing** — nodes receive packets not meant for them
- **Collisions** — multiple nodes transmit simultaneously, causing retransmissions
- **Over-emitting** — transmitting when the receiver isn't ready
- **Redundant data** — nodes transmit even when nothing has changed
- **Bad cluster head selection** — random selection ignores battery levels, some areas get overloaded

### How LECMAC works — 5 parameters
| Parameter | Value | What it does |
|-----------|-------|-------------|
| Threshold Energy (TE) | 0.1 J | Nodes with less energy than this CANNOT become cluster heads |
| Proximity Threshold (PT) | 10 m | No two cluster heads can be within 10m of each other |
| Maximum Cluster Size (MCS) | 15 nodes | No cluster can have more than 15 member nodes |
| Verification Period (VP) | short duration | CH turns radio ON briefly at start of each slot; turns off if no packet detected |
| Distance-to-Event (DE) | predefined meters | Node skips transmission if sensed event is farther than DE meters away |

### LECMAC Round Structure
Every round has two phases:
1. **Setup Phase** — CH selection + cluster formation
2. **Steady State Phase** — n frames of TDMA data transmission

**Setup Phase step by step:**
1. Each node checks: is my Residual Energy (RE) > TE (0.1J)?
2. If NO → node is automatically a non-CH
3. If YES → node is a candidate CH, attempts to broadcast ADV packet
4. If a candidate hears another node's ADV first → checks if that node is within PT (10m)
   - If within PT → backs off, becomes non-CH
   - If NOT within PT → continues trying to broadcast ADV
5. CH broadcasts INV (invitation) message to nearby non-CH nodes
6. Non-CH nodes receive multiple INV messages, join the CH with strongest signal (closest)
7. CH accepts JOIN-REQ up to MCS (15 nodes); sends REJECT if full
8. Rejected nodes try next closest CH
9. CH broadcasts TDMA transmission schedule

**Steady State Phase:**
- Each member gets one data slot per frame
- Member turns radio OFF in all other slots
- CH uses VP: turns radio ON briefly at slot start, stays ON if packet detected, turns OFF otherwise
- DE check: if event is more than DE meters away, node skips its slot entirely
- CH aggregates data and sends to Base Station (BS)

---

## Protocols Being Compared

### LEACH (Low Energy Adaptive Clustering Hierarchy)
- Random number RN generated per node per round (0 to 1)
- If RN < threshold → node becomes CH
- No energy check, no proximity check, no cluster size limit
- CH broadcasts ADV, non-CH joins strongest ADV sender
- CH creates TDMA schedule, members transmit in their slots
- CH aggregates and sends to BS
- **Weakness:** Random CH selection ignores battery, uneven CH distribution

### ES-MAC (Energy-Saving MAC)
- Same as LEACH PLUS one improvement:
- CH turns radio ON only for a short VP at the start of each slot
- If no incoming packet detected during VP → radio turns OFF
- **Weakness:** Better than LEACH but still no energy check for CH selection, no cluster size limit, no DE suppression

### LECMAC (proposed)
- All ES-MAC improvements PLUS:
- TE check for CH selection
- PT to prevent CH crowding
- MCS to balance cluster sizes
- DE to suppress redundant transmissions
- **Best network lifetime across all layouts**

---

## Energy Model

**First Order Radio Model** — must be used exactly as specified.

### Formulas
```
Transmit energy:  E_tx(k, d) = k * E_elec + k * E_amp * d²
Receive energy:   E_rx(k)    = k * E_elec
Idle energy:      E_idle     = E_idle_rate * time_idle
```

Where:
- `k` = number of bits in the packet
- `d` = distance between transmitter and receiver in meters

### Exact Parameter Values from Table 1
```
E_elec              = 50 nJ/bit    = 50 × 10⁻⁹ J/bit
E_amp               = 100 pJ/bit/m²= 100 × 10⁻¹² J/bit/m²
E_idle              = 40 nJ/bit    = 40 × 10⁻⁹ J/bit
Node initial energy = 5 J
Threshold Energy    = 0.1 J
Proximity Threshold = 10 m
Maximum Cluster Size= 15 nodes
Data/schedule packet= 100 bytes = 800 bits
Control packet      = 20 bytes  = 160 bits
```

---

## Simulation Layouts

| Layout | Nodes | Area | BS Position (suggested) |
|--------|-------|------|------------------------|
| 1 | 100 | 100×100 m² | (50, 150) |
| 2 | 200 | 100×100 m² | (50, 150) |
| 3 | 100 | 200×200 m² | (100, 250) |
| 4 | 200 | 200×200 m² | (100, 250) |

All nodes placed randomly. All nodes stationary. BS has infinite energy and is outside target area.

---

## Expected Results from Paper

### Layout 1 — 100 nodes, 100×100 m²
| Protocol | First Node Dies (Round) | Last Node Dies (Round) |
|----------|------------------------|----------------------|
| ES-MAC | 821 | 970 |
| LEACH | 2272 | 4080 |
| LECMAC | 3269 | 4890 |

### Layout 2 — 200 nodes, 100×100 m²
| Protocol | First Node Dies (Round) | Last Node Dies (Round) |
|----------|------------------------|----------------------|
| ES-MAC | 718 | 1579 |
| LEACH | 1890 | 3200 |
| LECMAC | 2673 | 4685 |

### Layout 3 — 100 nodes, 200×200 m²
| Protocol | First Node Dies (Round) | Last Node Dies (Round) |
|----------|------------------------|----------------------|
| ES-MAC | 681 | 1360 |
| LEACH | 2064 | 4090 |
| LECMAC | 2527 | 4250 |

### Layout 4 — 200 nodes, 200×200 m²
| Protocol | First Node Dies (Round) | Last Node Dies (Round) |
|----------|------------------------|----------------------|
| ES-MAC | 790 | 1618 |
| LEACH | 1975 | 3329 |
| LECMAC | 2732 | 4750 |

**Key pattern to verify:** LECMAC always outlasts LEACH always outlasts ES-MAC.
Exact numbers may differ slightly from paper (paper used Python scripts, we use NS-3).

---

## Tech Stack

| Tool | Purpose | How to run |
|------|---------|-----------|
| NS-3 | Network simulator — runs the actual simulation | Terminal: `./ns3 run <script>` |
| C++ | Language for writing NS-3 simulation scripts | Edited in Windsurf |
| Python 3 | Post-processing results, drawing graphs | `python3 plot_results.py` |
| matplotlib | Python library for plotting graphs | Import in Python scripts |
| NetAnim | GUI app to visualize network animation | Open separately, load .xml file |
| Windsurf | IDE for writing code | Already installed |
| Homebrew | Package manager for Mac | `brew install ...` |

---

## NS-3 Specific Knowledge

### What NS-3 is
NS-3 is a discrete-event network simulator. You write C++ code describing your network (nodes, positions, protocols, energy) and NS-3 simulates every packet transmission, energy drain, and node death over virtual time. It produces log/trace files you then analyze.

### NS-3 folder structure
```
ns-3-dev/
├── scratch/          ← PUT YOUR SIMULATION FILES HERE
│   ├── lecmac-sim.cc
│   ├── leach-sim.cc
│   └── esmac-sim.cc
├── src/              ← NS-3 built-in modules (don't edit)
├── build/            ← compiled output (auto-generated)
├── cmake-cache/      ← build cache (auto-generated)
└── ns3               ← the main script to build/run
```

### How to run a simulation
```bash
cd ~/Desktop/dc_proj/ns-3-dev
./ns3 run scratch/lecmac-sim
```

### How to build
```bash
./ns3 build
```

### Key NS-3 modules used in this project
- `ns3/core-module.h` — basic NS-3 infrastructure, scheduling, logging
- `ns3/network-module.h` — nodes, packets
- `ns3/mobility-module.h` — node positions
- `ns3/energy-module.h` — battery/energy tracking
- `ns3/wifi-module.h` — wireless communication
- `ns3/internet-module.h` — internet stack
- `ns3/applications-module.h` — sending/receiving data
- `ns3/netanim-module.h` — animation output

### Energy model in NS-3
Use `BasicEnergySource` for per-node battery tracking. Override consumption rates to match First Order Radio Model. Track residual energy each round. Mark node as dead when energy hits 0.

---

## Known Issues & Fixes

### Issue 1 — NS-3 build fails with SyntaxError in lock file
**Error message:**
```
SyntaxError: EOL while scanning string literal
NS3_MODULE_PATH = [..., 'Unknown command', ' "bin"'
```

**Root cause:** Antigravity + Windsurf are injecting a broken entry into the system PATH. When NS-3 writes its lock file, it captures the current PATH, and the broken entry corrupts the Python-readable lock file.

**Fix — run these commands in order:**
```bash
cd ~/Desktop/dc_proj/ns-3-dev
rm -rf cmake-cache
find . -name "*.lock" -delete
find . -name "ns3rc" -delete

# Sanitize PATH before running NS-3 — removes bad entries
export PATH=$(echo $PATH | tr ':' '\n' | grep -v "Unknown" | grep -v "antigravity" | tr '\n' ':' | sed 's/:$//')

# Now configure and build with clean PATH
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

**Alternative fix if above doesn't work:**
```bash
# Use the absolute system Python directly
/usr/bin/python3 -c "import sys; print(sys.version)"  # verify system python works
cd ~/Desktop/dc_proj/ns-3-dev
rm -rf cmake-cache
/usr/bin/python3 ns3 configure --enable-examples --enable-tests
/usr/bin/python3 ns3 build
```

**Permanent fix — add this to your ~/.zshrc:**
```bash
# Clean PATH helper for NS-3
alias ns3clean='export PATH=$(echo $PATH | tr ":" "\n" | grep -v "Unknown" | grep -v "antigravity" | tr "\n" ":" | sed "s/:$//") && echo "PATH cleaned"'
```
Then run `ns3clean` before any NS-3 commands.

### Issue 2 — M4 chip / Apple Silicon
NS-3 builds fine on M4 but make sure you downloaded the Apple Silicon version of any GUI tools. Use `arch -arm64` prefix if any commands fail with architecture errors.

### Issue 3 — Antigravity Python conflicts
When running Python scripts for plotting, use:
```bash
python3 plot_results.py
```
If Antigravity intercepts and breaks things, use:
```bash
/usr/bin/python3 plot_results.py
```

---

## Project File Structure

```
~/Desktop/dc_proj/
└── ns-3-dev/
    ├── scratch/
    │   ├── lecmac-sim.cc        # LECMAC protocol simulation
    │   ├── leach-sim.cc         # LEACH protocol simulation
    │   └── esmac-sim.cc         # ES-MAC protocol simulation
    ├── analysis/
    │   ├── plot_results.py      # Generates graphs like paper Figs 2-5
    │   └── parse_traces.py      # Parses NS-3 trace output files
    └── results/
        ├── layout1/
        │   ├── lecmac.txt
        │   ├── leach.txt
        │   └── esmac.txt
        ├── layout2/
        ├── layout3/
        └── layout4/
```

---

## Implementation Checklist

### Phase 1 — Environment (do this first)
- [ ] Fix NS-3 build error (corrupted lock file / PATH issue)
- [ ] Successfully run `./ns3 run hello-simulator` and see "Hello Simulator"
- [ ] Install Python plotting libraries: `pip3 install matplotlib numpy pandas`

### Phase 2 — LECMAC (most important, do this second)
- [ ] Create `scratch/lecmac-sim.cc`
- [ ] Implement First Order Radio Model with exact parameter values
- [ ] Implement TE check for CH eligibility
- [ ] Implement PT check to prevent CH crowding
- [ ] Implement MCS limit with REJECT messages
- [ ] Implement TDMA slot scheduling
- [ ] Implement VP (Verification Period) for CH idle listening reduction
- [ ] Implement DE check for transmission suppression
- [ ] Log dead node count per round to output file
- [ ] Test on all 4 layouts

### Phase 3 — LEACH
- [ ] Create `scratch/leach-sim.cc`
- [ ] Implement random CH selection with threshold formula
- [ ] Implement ADV/JOIN-REQ/schedule exchange
- [ ] Same energy model as LECMAC
- [ ] Log dead node count per round

### Phase 4 — ES-MAC
- [ ] Create `scratch/esmac-sim.cc`
- [ ] Same as LEACH + VP mechanism for CH
- [ ] Log dead node count per round

### Phase 5 — Results & Visualization
- [ ] Create `analysis/plot_results.py`
- [ ] Generate 4 graphs (one per layout) matching paper Figs 2-5
- [ ] X axis = number of rounds, Y axis = number of dead nodes
- [ ] Three lines per graph: ES-MAC, LEACH, LECMAC
- [ ] Set up NetAnim animation for at least one layout

### Phase 6 — Validation
- [ ] LECMAC first dead node round > LEACH > ES-MAC in all layouts
- [ ] LECMAC last dead node round > LEACH > ES-MAC in all layouts
- [ ] Numbers roughly match paper (within 10-15% is acceptable)

---

## Key C++ Code Patterns for NS-3

### Node creation and positioning
```cpp
NodeContainer nodes;
nodes.Create(100); // 100 sensor nodes

MobilityHelper mobility;
mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
Ptr<UniformRandomVariable> randX = CreateObject<UniformRandomVariable>();
randX->SetAttribute("Min", DoubleValue(0.0));
randX->SetAttribute("Max", DoubleValue(100.0)); // 100x100 area
// Set positions randomly in loop
mobility.Install(nodes);
```

### Energy source setup
```cpp
BasicEnergySourceHelper energySourceHelper;
energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(5.0)); // 5J initial
EnergySourceContainer sources = energySourceHelper.Install(nodes);
```

### Checking residual energy
```cpp
Ptr<BasicEnergySource> energySource = DynamicCast<BasicEnergySource>(sources.Get(i));
double residualEnergy = energySource->GetRemainingEnergy();
if (residualEnergy > 0.1) { // TE = 0.1J
    // node can be cluster head candidate
}
```

### Scheduling events (rounds)
```cpp
// Schedule next round after current one finishes
Simulator::Schedule(Seconds(roundDuration), &StartNewRound, roundNumber + 1);
```

### Logging dead nodes to file
```cpp
std::ofstream logFile;
logFile.open("results/layout1/lecmac.txt", std::ios::app);
logFile << roundNumber << " " << deadNodeCount << "\n";
logFile.close();
```

---

## Key Python Plotting Code

```python
import matplotlib.pyplot as plt
import numpy as np

def plot_layout(layout_num, esmac_file, leach_file, lecmac_file):
    # Read data
    esmac = np.loadtxt(esmac_file)
    leach = np.loadtxt(leach_file)
    lecmac = np.loadtxt(lecmac_file)
    
    plt.figure(figsize=(8, 6))
    plt.plot(esmac[:,0], esmac[:,1], label='ES-MAC', color='blue')
    plt.plot(leach[:,0], leach[:,1], label='LEACH', color='orange')
    plt.plot(lecmac[:,0], lecmac[:,1], label='LECMAC', color='green')
    
    plt.xlabel('Number of Rounds')
    plt.ylabel('Number of Dead Nodes')
    plt.title(f'Layout {layout_num}')
    plt.legend()
    plt.grid(True)
    plt.savefig(f'results/layout{layout_num}/comparison.png', dpi=150)
    plt.show()

# Run for all 4 layouts
for i in range(1, 5):
    plot_layout(i,
        f'results/layout{i}/esmac.txt',
        f'results/layout{i}/leach.txt',
        f'results/layout{i}/lecmac.txt'
    )
```

---

## NetAnim Setup

```cpp
// Add to your simulation .cc file
#include "ns3/netanim-module.h"

// After setting up nodes, before Simulator::Run():
AnimationInterface anim("lecmac-animation.xml");

// Color nodes: blue = cluster head, green = alive member, red = dead
anim.UpdateNodeColor(nodes.Get(i), 0, 0, 255);   // blue = CH
anim.UpdateNodeColor(nodes.Get(i), 0, 255, 0);   // green = alive
anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);   // red = dead

// Set node size for visibility
anim.UpdateNodeSize(nodes.Get(i), 2.0, 2.0);
```

To view animation:
1. Build NetAnim: `cd ~/Desktop/dc_proj/netanim && qmake NetAnim.pro && make`
2. Run: `./NetAnim`
3. File → Open → select `lecmac-animation.xml`

---

## Deliverables Summary

At the end of this project I need:
1. **Three working NS-3 simulation scripts** — lecmac-sim.cc, leach-sim.cc, esmac-sim.cc
2. **Four comparison graphs** — one per layout, matching Figs 2-5 from the paper
3. **NetAnim animation** — visual of at least one layout showing node deaths over time
4. **Results tables** — first/last node death rounds for all protocols in all layouts
5. **The numbers must show LECMAC > LEACH > ES-MAC** in network lifetime

---

## Quick Command Reference

```bash
# Navigate to project
cd ~/Desktop/dc_proj/ns-3-dev

# Clean build cache (do this if build fails)
rm -rf cmake-cache && find . -name "*.lock" -delete

# Configure NS-3
./ns3 configure --enable-examples --enable-tests

# Build NS-3
./ns3 build

# Run a specific simulation
./ns3 run scratch/lecmac-sim

# Run with command line arguments (for layout selection)
./ns3 run "scratch/lecmac-sim --nodes=100 --area=100"

# Test NS-3 works
./ns3 run hello-simulator

# Clean PATH before NS-3 commands (fixes Antigravity issue)
export PATH=$(echo $PATH | tr ':' '\n' | grep -v "Unknown" | tr '\n' ':' | sed 's/:$//')

# Plot results
python3 analysis/plot_results.py

# Install Python dependencies
pip3 install matplotlib numpy pandas
```

---

## Context for Claude

When helping with this project:
- Always give **complete working code**, not pseudocode or snippets with placeholders
- Always use the **exact parameter values** from Table 1 of the paper (listed above)
- When fixing NS-3 errors, give **exact terminal commands** to run one by one
- The student is **new to networks and NS-3** — explain what each piece of code does
- The IDE is **Windsurf** (VS Code-based) on **MacBook Pro M4**
- Python environment is **Antigravity** — if Python path issues arise, use `/usr/bin/python3`
- The project folder is `~/Desktop/dc_proj/ns-3-dev`
- Custom simulation files go in the `scratch/` subfolder
- The primary goal is **LECMAC outperforming LEACH and ES-MAC** in all 4 layouts
- Results don't need to match paper exactly — the ordering (LECMAC > LEACH > ES-MAC) is what matters
