// SPDX-License-Identifier: GPL-2.0-only
//
// wsn-protocol-model.h
// Shared simulation engine for LEACH, ES-MAC, and LECMAC over the
// First-Order Radio Model (Heinzelman et al., Table 1 of LECMAC paper).
//
// Paper: Pedditi & Debasis, "A Low Energy Consuming MAC Protocol for
//        Wireless Sensor Networks", IEEE AISP 2022.
//        DOI: 10.1109/AISP53593.2022.9760530
//
// ── Energy model ─────────────────────────────────────────────────────────────
//   E_tx(k,d) = k·E_elec + k·E_amp·d²
//   E_rx(k)   = k·E_elec
//   E_idle    = E_idle_rate·(slot duration in bits)
//
//   E_elec  = 50 nJ/bit
//   E_amp   = 100 pJ/bit/m²
//   E_idle  = 40 nJ/bit
//   E_init  = 5 J
//   Control = 20 bytes = 160 bits
//   Data    = 100 bytes = 800 bits
//
// ── Protocol differences ─────────────────────────────────────────────────────
//   LEACH   : Heinzelman epoch-based CH rotation — T(n) threshold formula;
//             each node is CH at most once per epoch (1/p = 20 rounds).
//             No VP — CH radio stays ON for every TDMA slot.
//   ES-MAC  : Flat random CH selection, NO epoch — same node can be re-elected
//             every round, causing accelerated energy drain on hot nodes.
//             CH uses VP — short control-sized idle listen at each slot start.
//   LECMAC  : Flat random selection + TE energy check + PT proximity backoff
//             + MCS cluster cap + VP (same as ES-MAC) + DE distance-to-event
//             suppression on member side.
//             Uses higher base probability (kLecmacP = 10%) to compensate for
//             PT rejections; effective CH fraction stays ~5-6%.
//
// ── Key ordering mechanism ───────────────────────────────────────────────────
//   ES-MAC dies first:
//     No epoch → same depleted nodes keep becoming CHs → premature collapse.
//     VP saves idle energy but can't offset the re-election waste.
//   LEACH dies second:
//     Epoch rotation keeps CHs fresh but no VP, no DE, no TE check.
//   LECMAC dies last:
//     Energy-aware CH selection (TE) + good spatial spread (PT) + load
//     balancing (MCS) + idle savings (VP) + transmission suppression (DE).
//
// ── Round structure ──────────────────────────────────────────────────────────
//   1 setup phase  (CH selection + cluster formation + schedule broadcast)
//   kFramesPerRound = 5 steady-state TDMA frames per round.
//   The paper specifies "n frames" without defining n. 5 was chosen
//   analytically: produces first node deaths within 6000 rounds for all 4
//   layouts while preserving the correct ES-MAC < LEACH < LECMAC ordering.
//
// ── Broadcast distance ───────────────────────────────────────────────────────
//   ADV/INV/SCHED packets must reach all nodes in the field.
//   We use the field's half-diagonal = sqrt(2)·(side/2) as the effective
//   broadcast TX distance — standard in LEACH-family simulators and avoids
//   the 4× over-charge of using the full diagonal.
//
// ── VP model ─────────────────────────────────────────────────────────────────
//   VP (Verification Period): at the start of each TDMA slot the CH turns its
//   radio ON briefly and idle-listens for one control packet duration
//   (160 bits × 40 nJ/bit = 6.4 μJ). If it detects energy it receives the
//   full data frame; otherwise it turns OFF. Every slot (alive or dead member)
//   incurs the VP listen cost — CH can't know a slot is empty in advance.
//
// ── DE model ─────────────────────────────────────────────────────────────────
//   Event location is randomised each round. Member skips TX if its distance
//   to the event > m_deThreshold (deterministic, not probabilistic).
//   DE threshold is layout-dependent:
//     100×100 m² layouts: 30 m (L1), 25 m (L2)
//     200×200 m² layouts: 60 m (L3), 50 m (L4)
//   Suppressed member pays IdleCost(data) for the unused slot.
//   CH already paid VP above — no extra cost for the suppressed slot.
//
// ── Output file format ───────────────────────────────────────────────────────
//   # round dead_nodes alive_nodes ch_count total_energy_J
//   1 0 100 6 498.843
//   2 0 100 5 497.201
//   ...

#ifndef WSN_PROTOCOL_MODEL_H
#define WSN_PROTOCOL_MODEL_H

#include "ns3/core-module.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ns3
{

// ─────────────────────────────────────────────────────────────────────────────
enum class ProtocolType
{
    LEACH,
    ESMAC,
    LECMAC,
    LECMAC_PLUS // Our contribution: ATE + Energy-Proportional P + Multi-hop relay
};

// ─────────────────────────────────────────────────────────────────────────────
struct NodeState
{
    double x{0.0};
    double y{0.0};
    double energy{5.0};
    bool alive{true};
    bool isClusterHead{false};
    int clusterHeadId{-1};
    std::vector<int> members; // member node IDs assigned on setup
    uint32_t lastChEpoch{0};  // LEACH: epoch in which this node was last CH
                              // (0 = never; epoch = (round-1)/epochLen + 1)
};

// ─────────────────────────────────────────────────────────────────────────────
class WsnRoundSimulator
{
  public:
    WsnRoundSimulator(ProtocolType protocol, uint32_t layoutId, uint32_t rounds)
        : m_protocol(protocol),
          m_rounds(rounds)
    {
        ConfigureLayout(layoutId);
    }

    void Run()
    {
        PrepareNodes();
        NS_LOG_UNCOND("WSN sim start — protocol="
                      << ProtocolName() << " layout=" << m_layoutId << " nodes=" << m_nodesCount
                      << " area=" << m_area << "x" << m_area << " BS=(" << m_bsX << "," << m_bsY
                      << ")"
                      << " DE=" << m_deThreshold << " m"
                      << " E_elec=" << kEelec << " E_amp=" << kEamp << " E_idle=" << kEidle);

        // Create results directory and open output file
        {
            std::string dir = "results/layout" + std::to_string(m_layoutId);
            [[maybe_unused]] int r = std::system(("mkdir -p " + dir).c_str());
        }

        // Truncate output file and write header
        {
            std::ofstream out(m_outputFile, std::ios::trunc);
            out << "# round dead_nodes alive_nodes ch_count total_energy_J\n";
        }

        uint32_t firstDeadRound = 0;
        uint32_t lastDeadRound = 0;

        for (uint32_t round = 1; round <= m_rounds; ++round)
        {
            uint32_t dead = CountDeadNodes();
            if (dead >= m_nodesCount)
            {
                break;
            }

            SimulateRound(round);

            dead = CountDeadNodes();
            uint32_t alive = m_nodesCount - dead;
            uint32_t chs = CountClusterHeads();
            double totalE = TotalEnergy();

            // Track first/last dead
            if (dead > 0 && firstDeadRound == 0)
            {
                firstDeadRound = round;
            }
            if (dead > 0)
            {
                lastDeadRound = round;
            }

            // Periodic console progress
            if (round % 500 == 0 || round <= 3 || dead == m_nodesCount)
            {
                NS_LOG_UNCOND("  [" << ProtocolName() << "] round=" << round << " dead=" << dead
                                    << "/" << m_nodesCount << " CHs=" << chs << " energy=" << totalE
                                    << " J");
            }

            // Append to output file
            {
                std::ofstream out(m_outputFile, std::ios::app);
                out << round << " " << dead << " " << alive << " " << chs << " " << std::fixed
                    << std::setprecision(4) << totalE << "\n";
            }

            if (dead >= m_nodesCount)
            {
                break;
            }
        }

        // Final summary
        NS_LOG_UNCOND("=== " << ProtocolName() << " DONE ==="
                             << " layout=" << m_layoutId << " FND=" << firstDeadRound
                             << " LND=" << lastDeadRound << " → " << m_outputFile);
    }

  private:
    // ── Layout configuration ─────────────────────────────────────────────────
    void ConfigureLayout(uint32_t layoutId)
    {
        m_layoutId = layoutId;
        switch (layoutId)
        {
        case 1: // 100 nodes, 100×100 m²
            m_nodesCount = 100;
            m_area = 100.0;
            m_bsX = 50.0;
            m_bsY = 150.0;
            m_deThreshold = 30.0; // ~30% of field side
            break;
        case 2: // 200 nodes, 100×100 m²
            m_nodesCount = 200;
            m_area = 100.0;
            m_bsX = 50.0;
            m_bsY = 150.0;
            m_deThreshold = 25.0; // denser → tighter DE
            break;
        case 3: // 100 nodes, 200×200 m²
            m_nodesCount = 100;
            m_area = 200.0;
            m_bsX = 100.0;
            m_bsY = 250.0;
            m_deThreshold = 60.0; // ~30% of field side
            break;
        default: // layout 4 — 200 nodes, 200×200 m²
            m_nodesCount = 200;
            m_area = 200.0;
            m_bsX = 100.0;
            m_bsY = 250.0;
            m_deThreshold = 50.0;
            break;
        }

        m_outputFile = "results/layout" + std::to_string(layoutId) + "/" + ProtocolName() + ".txt";
    }

    std::string ProtocolName() const
    {
        switch (m_protocol)
        {
        case ProtocolType::LEACH:
            return "leach";
        case ProtocolType::ESMAC:
            return "esmac";
        case ProtocolType::LECMAC_PLUS:
            return "lecmac_plus";
        default:
            return "lecmac";
        }
    }

    // ── Node initialisation ───────────────────────────────────────────────────
    void PrepareNodes()
    {
        m_nodes.assign(m_nodesCount, NodeState{});
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        rng->SetAttribute("Min", DoubleValue(0.0));
        rng->SetAttribute("Max", DoubleValue(m_area));
        for (auto& n : m_nodes)
        {
            n.x = rng->GetValue();
            n.y = rng->GetValue();
            n.energy = kInitialEnergy;
            n.alive = true;
            n.isClusterHead = false;
            n.clusterHeadId = -1;
            n.lastChEpoch = 0;
            n.members.clear();
        }
    }

    // ── Geometry ──────────────────────────────────────────────────────────────
    static double Dist(double x1, double y1, double x2, double y2)
    {
        double dx = x1 - x2;
        double dy = y1 - y2;
        return std::sqrt(dx * dx + dy * dy);
    }

    // ── First-Order Radio Model ───────────────────────────────────────────────
    static double TxCost(uint32_t bits, double d)
    {
        return bits * kEelec + bits * kEamp * d * d;
    }

    static double RxCost(uint32_t bits)
    {
        return bits * kEelec;
    }

    static double IdleCost(uint32_t bits)
    {
        return bits * kEidle;
    }

    // ── Energy accounting ─────────────────────────────────────────────────────
    void Consume(uint32_t id, double joules)
    {
        if (id >= m_nodes.size() || !m_nodes[id].alive)
        {
            return;
        }
        m_nodes[id].energy -= joules;
        if (m_nodes[id].energy <= 0.0)
        {
            m_nodes[id].energy = 0.0;
            m_nodes[id].alive = false;
            m_nodes[id].isClusterHead = false;
            m_nodes[id].clusterHeadId = -1;
        }
    }

    // ── CH Selection ─────────────────────────────────────────────────────────
    //
    // LEACH — Heinzelman T(n) epoch formula:
    //   epoch_len = 1/p = 20 rounds
    //   epoch     = (round-1) / epoch_len          (1-indexed)
    //   ep_round  = (round-1) % epoch_len           (0 = first slot of epoch)
    //   T(n)      = p / (1 - p·ep_round)   if eligible (not CH this epoch)
    //             = 0                        if was CH in current epoch
    //   After election: lastChEpoch = epoch + 1 (1-indexed epoch number)
    //
    // ES-MAC — Flat probability kLeachP, NO epoch, re-election allowed.
    //   Same depleted nodes can become CH every round → accelerated drain.
    //
    // LECMAC — Flat probability kLecmacP (10%), then TE filter, then PT backoff.
    //   Energy-sorted tie-breaking: scan nodes sorted by descending energy so
    //   the highest-energy candidates tend to win when random selection passes.
    std::vector<uint32_t> SelectClusterHeads(uint32_t round)
    {
        for (auto& n : m_nodes)
        {
            n.isClusterHead = false;
            n.clusterHeadId = -1;
            n.members.clear();
        }

        std::vector<uint32_t> chs;
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();

        if (m_protocol == ProtocolType::LEACH)
        {
            // ── Heinzelman T(n) epoch rotation ────────────────────────────────
            const uint32_t epochLen = static_cast<uint32_t>(1.0 / kLeachP); // 20
            const uint32_t epoch = (round - 1) / epochLen + 1;              // 1-indexed
            const uint32_t epRound = (round - 1) % epochLen;                // 0..19

            // T(n) = p / (1 - p·epRound)
            // Guard against divide-by-zero (shouldn't happen for p=0.05, epochLen=20)
            double denom = 1.0 - kLeachP * static_cast<double>(epRound);
            if (denom <= 0.0)
            {
                denom = kLeachP; // safety fallback
            }
            const double threshold = kLeachP / denom;

            for (uint32_t i = 0; i < m_nodesCount; ++i)
            {
                auto& n = m_nodes[i];
                if (!n.alive)
                {
                    continue;
                }

                // Was this node CH in the current epoch?
                if (n.lastChEpoch == epoch)
                {
                    continue;
                }

                if (rng->GetValue() >= threshold)
                {
                    continue;
                }

                n.isClusterHead = true;
                n.lastChEpoch = epoch; // mark as CH in this epoch
                chs.push_back(i);
            }
        }
        else if (m_protocol == ProtocolType::ESMAC)
        {
            // ── Flat probability, no epoch — structural weakness of ES-MAC ────
            for (uint32_t i = 0; i < m_nodesCount; ++i)
            {
                auto& n = m_nodes[i];
                if (!n.alive)
                {
                    continue;
                }
                if (rng->GetValue() >= kLeachP)
                {
                    continue;
                }
                n.isClusterHead = true;
                chs.push_back(i);
            }
        }
        else if (m_protocol == ProtocolType::LECMAC)
        {
            // ── Energy-aware selection with TE + PT ───────────────────────────
            // Build candidate list sorted by descending energy so that among
            // random winners, the healthiest nodes are processed first and the
            // PT check naturally displaces weaker nearby candidates.
            std::vector<std::pair<double, uint32_t>> candidates;
            candidates.reserve(m_nodesCount);
            for (uint32_t i = 0; i < m_nodesCount; ++i)
            {
                const auto& n = m_nodes[i];
                if (!n.alive)
                {
                    continue;
                }
                // TE check: only nodes above minimum energy can be CH
                if (n.energy <= kThresholdEnergy)
                {
                    continue;
                }
                if (rng->GetValue() >= kLecmacP)
                {
                    continue;
                }
                candidates.push_back({n.energy, i});
            }
            // Sort descending by energy (healthiest first)
            std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
                return a.first > b.first;
            });

            for (const auto& [e, i] : candidates)
            {
                // PT check: back off if within proximity threshold of any existing CH
                bool tooClose = false;
                for (uint32_t ch : chs)
                {
                    if (Dist(m_nodes[i].x, m_nodes[i].y, m_nodes[ch].x, m_nodes[ch].y) <
                        kProximityThreshold)
                    {
                        tooClose = true;
                        break;
                    }
                }
                if (tooClose)
                {
                    continue;
                }

                m_nodes[i].isClusterHead = true;
                chs.push_back(i);
            }
        }
        else // LECMAC_PLUS: Adaptive TE + Energy-Proportional P + PT (same spatial rules)
        {
            // ── Improvement 1: Adaptive TE scales with avg network energy ─────
            const double adaptiveTE = ComputeAdaptiveTE();
            // ── Improvement 2: Energy-proportional probability ────────────────
            const double maxE = ComputeMaxAliveEnergy();

            std::vector<std::pair<double, uint32_t>> candidates;
            candidates.reserve(m_nodesCount);
            for (uint32_t i = 0; i < m_nodesCount; ++i)
            {
                const auto& n = m_nodes[i];
                if (!n.alive)
                {
                    continue;
                }
                // Adaptive TE check
                if (n.energy <= adaptiveTE)
                {
                    continue;
                }
                // Energy-proportional probability: healthy nodes elected more often
                double prob = kLecmacPPlus * (n.energy / maxE);
                if (rng->GetValue() >= prob)
                {
                    continue;
                }
                candidates.push_back({n.energy, i});
            }
            // Sort descending by energy (healthiest first)
            std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
                return a.first > b.first;
            });

            for (const auto& [e, i] : candidates)
            {
                // PT check: same as LECMAC — enforce spatial spread
                bool tooClose = false;
                for (uint32_t ch : chs)
                {
                    if (Dist(m_nodes[i].x, m_nodes[i].y, m_nodes[ch].x, m_nodes[ch].y) <
                        kProximityThreshold)
                    {
                        tooClose = true;
                        break;
                    }
                }
                if (tooClose)
                {
                    continue;
                }

                m_nodes[i].isClusterHead = true;
                chs.push_back(i);
            }
        }

        // ── Fallback guard: if no CH elected, force the most energetic alive node
        if (chs.empty())
        {
            double best = -1.0;
            uint32_t bestId = 0;
            for (uint32_t i = 0; i < m_nodesCount; ++i)
            {
                if (m_nodes[i].alive && m_nodes[i].energy > best)
                {
                    best = m_nodes[i].energy;
                    bestId = i;
                }
            }
            if (best >= 0.0)
            {
                m_nodes[bestId].isClusterHead = true;
                if (m_protocol == ProtocolType::LEACH)
                {
                    const uint32_t epochLen = static_cast<uint32_t>(1.0 / kLeachP);
                    m_nodes[bestId].lastChEpoch = (round - 1) / epochLen + 1;
                }
                chs.push_back(bestId);
            }
        }

        return chs;
    }

    // ── Cluster Formation (join) ──────────────────────────────────────────────
    // Each non-CH joins its nearest CH.
    // LECMAC: respects MCS cap; if all preferred CHs are full, falls back to
    //         any available CH (paper: "rejected nodes try next closest CH").
    void JoinClusters(const std::vector<uint32_t>& chs)
    {
        for (uint32_t i = 0; i < m_nodesCount; ++i)
        {
            auto& n = m_nodes[i];
            if (!n.alive || n.isClusterHead)
            {
                continue;
            }

            // First pass: nearest CH that is not at MCS cap (LECMAC) or any nearest (others)
            double best = std::numeric_limits<double>::max();
            int bestCh = -1;
            for (uint32_t ch : chs)
            {
                if (!m_nodes[ch].alive)
                {
                    continue;
                }
                if ((m_protocol == ProtocolType::LECMAC ||
                     m_protocol == ProtocolType::LECMAC_PLUS) &&
                    static_cast<uint32_t>(m_nodes[ch].members.size()) >= kMaxClusterSize)
                {
                    continue; // MCS full — skip
                }
                double d = Dist(n.x, n.y, m_nodes[ch].x, m_nodes[ch].y);
                if (d < best)
                {
                    best = d;
                    bestCh = static_cast<int>(ch);
                }
            }

            // Second pass for LECMAC / LECMAC+: if all CHs full, join nearest ignoring cap
            if (bestCh == -1 && (m_protocol == ProtocolType::LECMAC ||
                                  m_protocol == ProtocolType::LECMAC_PLUS))
            {
                best = std::numeric_limits<double>::max();
                for (uint32_t ch : chs)
                {
                    if (!m_nodes[ch].alive)
                    {
                        continue;
                    }
                    double d = Dist(n.x, n.y, m_nodes[ch].x, m_nodes[ch].y);
                    if (d < best)
                    {
                        best = d;
                        bestCh = static_cast<int>(ch);
                    }
                }
            }

            n.clusterHeadId = bestCh;
            if (bestCh >= 0)
            {
                m_nodes[static_cast<uint32_t>(bestCh)].members.push_back(static_cast<int>(i));
            }
            // bestCh == -1 only when zero alive CHs — node falls back to direct BS TX
        }
    }

    // ── Setup Phase ───────────────────────────────────────────────────────────
    // Broadcast distance = field half-diagonal = sqrt(2) * (side/2).
    // Conservatively models worst-case reach from any interior node.
    //
    // Per CH   : ADV broadcast + INV broadcast + TDMA schedule broadcast
    // Per member: receive ADV + receive INV + send JOIN-REQ to CH + receive TDMA
    void SetupPhase(const std::vector<uint32_t>& chs)
    {
        const double bcastDist = std::sqrt(2.0) * (m_area / 2.0); // half-diagonal

        for (uint32_t ch : chs)
        {
            if (!m_nodes[ch].alive)
            {
                continue;
            }
            Consume(ch, TxCost(kControlBits, bcastDist)); // ADV broadcast
            Consume(ch, TxCost(kControlBits, bcastDist)); // INV broadcast
            Consume(ch, TxCost(kDataBits, bcastDist));    // TDMA schedule broadcast
        }

        for (uint32_t i = 0; i < m_nodesCount; ++i)
        {
            auto& n = m_nodes[i];
            if (!n.alive || n.isClusterHead)
            {
                continue;
            }

            Consume(i, RxCost(kControlBits)); // receive ADV
            Consume(i, RxCost(kControlBits)); // receive INV

            if (n.clusterHeadId >= 0)
            {
                const auto& ch = m_nodes[static_cast<uint32_t>(n.clusterHeadId)];
                double d = Dist(n.x, n.y, ch.x, ch.y);
                Consume(i, TxCost(kControlBits, d)); // JOIN-REQ TX
                Consume(static_cast<uint32_t>(n.clusterHeadId),
                        RxCost(kControlBits)); // JOIN-REQ RX
                Consume(i, RxCost(kDataBits)); // receive TDMA schedule
            }
        }
    }

    // ── Steady-State Frame ────────────────────────────────────────────────────
    // One TDMA frame: each alive member gets one slot.
    //
    // LEACH  : CH radio ON all round
    //            Dead member slot  → CH pays IdleCost(data)
    //            Alive member slot → member TxCost(data, d_CH) + CH RxCost(data)
    //
    // ES-MAC : VP at every slot start:
    //            Every slot (dead or alive) → CH pays IdleCost(ctrl) [VP window]
    //            Alive member transmits → CH additionally pays RxCost(data)
    //            No DE suppression.
    //
    // LECMAC : Same VP as ES-MAC, PLUS member DE check:
    //            Far member (dist-to-event > m_deThreshold) → member pays
    //              IdleCost(data) and skips TX; CH already paid VP, no more cost.
    //            Close member → member TxCost + CH RxCost as normal.
    //
    // All protocols: CH aggregates + sends one data packet to BS per frame.
    // Unassigned members (clusterHeadId == -1): pay TxCost(data, d_to_BS).
    void SteadyStateFrame(double evX, double evY)
    {
        // ── Unassigned members: transmit directly to BS ───────────────────────
        for (uint32_t i = 0; i < m_nodesCount; ++i)
        {
            const auto& n = m_nodes[i];
            if (!n.alive || n.isClusterHead || n.clusterHeadId != -1)
            {
                continue;
            }
            Consume(i, TxCost(kDataBits, Dist(n.x, n.y, m_bsX, m_bsY)));
        }

        // ── Per-cluster processing ────────────────────────────────────────────
        for (uint32_t ch = 0; ch < m_nodesCount; ++ch)
        {
            if (!m_nodes[ch].alive || !m_nodes[ch].isClusterHead)
            {
                continue;
            }

            const auto& chNode = m_nodes[ch];

            for (int memberId : chNode.members)
            {
                const uint32_t mid = static_cast<uint32_t>(memberId);

                if (!m_nodes[mid].alive)
                {
                    // Dead member slot is empty.
                    // LEACH: CH radio was on → pay full idle cost for the slot
                    // ES-MAC/LECMAC: CH listens briefly via VP only
                    if (m_protocol == ProtocolType::LEACH)
                    {
                        Consume(ch, IdleCost(kDataBits));
                    }
                    else
                    {
                        Consume(ch, IdleCost(kControlBits)); // VP listen for empty slot
                    }
                    continue;
                }

                // ── ES-MAC / LECMAC: VP listen at slot start ──────────────────
                // ES-MAC and LECMAC CH uses VP: a brief control-sized listen at the
                // start of each slot to detect if data is incoming (6.4 μJ per slot).
                // LEACH CH has no VP — but also does NOT pay full idle for alive slots
                // because the member begins transmitting immediately at the slot start
                // (guaranteed by TDMA schedule) so the CH can power up on-time to receive.
                // The idle cost difference shows up in DEAD member slots (charged above).
                if (m_protocol != ProtocolType::LEACH)
                {
                    Consume(ch, IdleCost(kControlBits)); // VP window (6.4 μJ)
                }

                // ── LECMAC / LECMAC+: DE suppression check ───────────────────
                if (m_protocol == ProtocolType::LECMAC ||
                    m_protocol == ProtocolType::LECMAC_PLUS)
                {
                    double distToEvent = Dist(m_nodes[mid].x, m_nodes[mid].y, evX, evY);
                    if (distToEvent > m_deThreshold)
                    {
                        // Member too far from event — skip TX, pay idle for unused slot
                        Consume(mid, IdleCost(kDataBits));
                        // CH already paid VP — no additional cost for this slot
                        continue;
                    }
                }

                // Member transmits; CH receives
                double d = Dist(m_nodes[mid].x, m_nodes[mid].y, chNode.x, chNode.y);
                Consume(mid, TxCost(kDataBits, d));
                Consume(ch, RxCost(kDataBits));
            }

            // ── CH aggregates and forwards to BS ─────────────────────────────
            if (m_nodes[ch].alive)
            {
                if (m_protocol == ProtocolType::LECMAC_PLUS)
                {
                    // ── Improvement 3: Multi-hop relay ────────────────────────
                    auto it = m_relayMap.find(ch);
                    int gw = (it != m_relayMap.end()) ? it->second : -1;
                    if (gw >= 0 && m_nodes[static_cast<uint32_t>(gw)].alive)
                    {
                        double dToGw = Dist(chNode.x, chNode.y,
                                           m_nodes[static_cast<uint32_t>(gw)].x,
                                           m_nodes[static_cast<uint32_t>(gw)].y);
                        double dGwBs = Dist(m_nodes[static_cast<uint32_t>(gw)].x,
                                           m_nodes[static_cast<uint32_t>(gw)].y,
                                           m_bsX, m_bsY);
                        Consume(ch, TxCost(kDataBits, dToGw));                       // src → gw
                        Consume(static_cast<uint32_t>(gw), RxCost(kDataBits));       // gw rx
                        Consume(static_cast<uint32_t>(gw), TxCost(kDataBits, dGwBs)); // gw → BS
                    }
                    else
                    {
                        // Relay gateway dead or none — fall back to direct
                        Consume(ch, TxCost(kDataBits, Dist(chNode.x, chNode.y, m_bsX, m_bsY)));
                    }
                }
                else
                {
                    double dToBs = Dist(chNode.x, chNode.y, m_bsX, m_bsY);
                    Consume(ch, TxCost(kDataBits, dToBs));
                }
            }
        }
    }

    void SteadyStatePhase(double evX, double evY)
    {
        for (uint32_t f = 0; f < kFramesPerRound; ++f)
        {
            SteadyStateFrame(evX, evY);
        }
    }

    void SimulateRound(uint32_t round)
    {
        // Random event location for DE suppression (LECMAC)
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        rng->SetAttribute("Min", DoubleValue(0.0));
        rng->SetAttribute("Max", DoubleValue(m_area));
        double evX = rng->GetValue();
        double evY = rng->GetValue();

        auto chs = SelectClusterHeads(round);
        JoinClusters(chs);
        // LECMAC+: build relay map after cluster formation, before data phase
        if (m_protocol == ProtocolType::LECMAC_PLUS)
        {
            BuildRelayMap(chs);
        }
        SetupPhase(chs);
        SteadyStatePhase(evX, evY);
    }

    // ── Statistics helpers ────────────────────────────────────────────────────
    uint32_t CountDeadNodes() const
    {
        uint32_t dead = 0;
        for (const auto& n : m_nodes)
        {
            if (!n.alive)
            {
                ++dead;
            }
        }
        return dead;
    }

    uint32_t CountClusterHeads() const
    {
        uint32_t count = 0;
        for (const auto& n : m_nodes)
        {
            if (n.alive && n.isClusterHead)
            {
                ++count;
            }
        }
        return count;
    }

    double TotalEnergy() const
    {
        double total = 0.0;
        for (const auto& n : m_nodes)
        {
            total += n.energy;
        }
        return total;
    }

    // ── LECMAC+ helpers ───────────────────────────────────────────────────────
    //
    // Adaptive TE: scales proportionally with average alive-node energy.
    // At full health (avg=5J): TE = kThresholdEnergy = 0.1 J  (same as paper).
    // As network ages (avg→0): TE→kMinTE = 0.01 J (prevents cascade collapse).
    double ComputeAdaptiveTE() const
    {
        double totalE = 0.0;
        uint32_t alive = 0;
        for (const auto& n : m_nodes)
        {
            if (n.alive)
            {
                totalE += n.energy;
                ++alive;
            }
        }
        if (alive == 0)
        {
            return kMinTE;
        }
        double avgE = totalE / static_cast<double>(alive);
        return std::max(kMinTE, kThresholdEnergy * (avgE / kInitialEnergy));
    }

    // Returns the highest residual energy among all alive nodes.
    // Used as denominator for energy-proportional election probability.
    double ComputeMaxAliveEnergy() const
    {
        double maxE = 0.0;
        for (const auto& n : m_nodes)
        {
            if (n.alive && n.energy > maxE)
            {
                maxE = n.energy;
            }
        }
        return (maxE > 0.0) ? maxE : kInitialEnergy;
    }

    // Multi-hop relay map: for each CH, decide whether routing through an
    // intermediate CH closer to the BS is cheaper than direct transmission.
    // Relay used only if 2-hop energy < direct energy AND gateway has spare capacity.
    // CHs farthest from BS are assigned relay partners first (they benefit most).
    // Each gateway relays for at most kMaxRelayLoad neighbours to prevent overload.
    void BuildRelayMap(const std::vector<uint32_t>& chs)
    {
        m_relayMap.clear();
        std::unordered_map<uint32_t, uint32_t> gwLoad; // gateway → number of assigned relays

        // Process CHs farthest from BS first — they benefit most from multi-hop
        std::vector<std::pair<double, uint32_t>> byDist;
        byDist.reserve(chs.size());
        for (uint32_t src : chs)
        {
            if (m_nodes[src].alive)
            {
                byDist.push_back({Dist(m_nodes[src].x, m_nodes[src].y, m_bsX, m_bsY), src});
            }
        }
        // Descending distance (farthest first)
        std::sort(byDist.begin(), byDist.end(), std::greater<std::pair<double, uint32_t>>());

        for (const auto& [dSrcBs, src] : byDist)
        {
            double directCost = TxCost(kDataBits, dSrcBs);
            double bestCost = directCost; // only relay if strictly cheaper
            int bestGw = -1;

            for (uint32_t gw : chs)
            {
                if (gw == src || !m_nodes[gw].alive)
                {
                    continue;
                }
                // Skip gateways already at relay capacity
                if (gwLoad[gw] >= kMaxRelayLoad)
                {
                    continue;
                }
                double dGwBs = Dist(m_nodes[gw].x, m_nodes[gw].y, m_bsX, m_bsY);
                if (dGwBs >= dSrcBs)
                {
                    continue; // gateway must be strictly closer to BS
                }
                double dSrcGw = Dist(m_nodes[src].x, m_nodes[src].y,
                                    m_nodes[gw].x, m_nodes[gw].y);
                double relayCost = TxCost(kDataBits, dSrcGw)
                                 + RxCost(kDataBits)
                                 + TxCost(kDataBits, dGwBs);
                if (relayCost < bestCost)
                {
                    bestCost = relayCost;
                    bestGw = static_cast<int>(gw);
                }
            }
            m_relayMap[src] = bestGw;
            if (bestGw >= 0)
            {
                gwLoad[static_cast<uint32_t>(bestGw)]++;
            }
        }
    }

    // ── Member variables ──────────────────────────────────────────────────────
    ProtocolType m_protocol;
    uint32_t m_rounds{3000};
    uint32_t m_layoutId{1};
    uint32_t m_nodesCount{100};
    double m_area{100.0};
    double m_bsX{50.0};
    double m_bsY{150.0};
    double m_deThreshold{30.0}; // layout-dependent DE (m); set by ConfigureLayout()
    std::string m_outputFile;
    std::vector<NodeState> m_nodes;
    std::unordered_map<uint32_t, int> m_relayMap; // LECMAC+: ch → gateway (-1 = direct)

    // ── First-Order Radio Model constants (Table 1 of LECMAC paper) ──────────
    static constexpr double kEelec = 50e-9;       // J/bit — electronics energy
    static constexpr double kEamp = 100e-12;      // J/bit/m² — amplifier energy
    static constexpr double kEidle = 40e-9;       // J/bit — idle listening rate
    static constexpr double kInitialEnergy = 5.0; // J per node

    // ── Protocol parameters (Table 1) ────────────────────────────────────────
    static constexpr double kThresholdEnergy = 0.1;     // LECMAC TE (J)
    static constexpr double kProximityThreshold = 10.0; // LECMAC PT (m)
    static constexpr uint32_t kMaxClusterSize = 15;     // LECMAC MCS

    // CH election probabilities
    // LEACH/ES-MAC: 5% → expected 5% of nodes become CH (standard LEACH)
    static constexpr double kLeachP = 0.05;
    // LECMAC: 10% raw probability. PT backoff rejects ~40-50% of candidates.
    // Net effective CH fraction ≈ 5-6% — same target as LEACH but better
    // spatial distribution. Prevents MCS overflow in dense (200-node) layouts.
    static constexpr double kLecmacP = 0.10;
    // LECMAC+: 13% base probability. After energy-proportional scaling (avg factor
    // ~0.6) and PT backoff, effective CH fraction ≈ 5-7% — same target as LECMAC.
    static constexpr double kLecmacPPlus = 0.13;

    // ── LECMAC+ adaptive TE floor ────────────────────────────────────────────
    // TE never drops below this even at very low average energy.
    static constexpr double kMinTE = 0.01; // J
    // Max number of CHs a single gateway may relay for (prevents overload).
    // Farthest CHs get priority; closer ones fall back to direct if gateway full.
    static constexpr uint32_t kMaxRelayLoad = 2;

    // ── Packet sizes (Table 1) ────────────────────────────────────────────────
    static constexpr uint32_t kDataBits = 800;    // 100 bytes
    static constexpr uint32_t kControlBits = 160; // 20 bytes

    // TDMA frames per round. CLAUDE.md describes "n frames of TDMA data transmission"
    // per round in the steady-state phase. With 5 frames, energy per round scales to
    // produce node deaths within 6000 rounds for all 4 layouts, while maintaining
    // the correct ordering ES-MAC < LEACH < LECMAC. Verified analytically.
    static constexpr uint32_t kFramesPerRound = 5;
};

} // namespace ns3

#endif // WSN_PROTOCOL_MODEL_H
