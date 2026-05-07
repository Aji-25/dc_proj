/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * lecmac-plus-sim.cc
 * Entry point for LECMAC+ — our extended protocol with three improvements:
 *   1. Adaptive Threshold Energy (ATE)
 *   2. Energy-Proportional CH Election Probability
 *   3. Multi-hop CH-to-BS Relay
 */

#include "wsn-protocol-model.h"

#include "ns3/core-module.h"

#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LecmacPlusSim");

int
main(int argc, char* argv[])
{
    uint32_t layout = 1;
    uint32_t rounds = 6000;
    uint32_t seed = 2;

    CommandLine cmd(__FILE__);
    cmd.AddValue("layout", "Layout id (1..4)", layout);
    cmd.AddValue("rounds", "Maximum simulation rounds", rounds);
    cmd.AddValue("seed", "RNG seed", seed);
    cmd.Parse(argc, argv);

    layout = std::min(4u, std::max(1u, layout));
    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(1);

    WsnRoundSimulator sim(ProtocolType::LECMAC_PLUS, layout, rounds);
    sim.Run();

    NS_LOG_UNCOND("LECMAC+ finished: layout=" << layout << " rounds<=" << rounds);
    return 0;
}
