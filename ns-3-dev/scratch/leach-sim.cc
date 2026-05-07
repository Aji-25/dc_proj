/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "wsn-protocol-model.h"

#include "ns3/core-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LeachSim");

int
main(int argc, char* argv[])
{
    uint32_t layout = 1;
    uint32_t rounds = 6000;
    uint32_t seed = 2;

    CommandLine cmd(__FILE__);
    cmd.AddValue("layout", "Layout id (1..4)", layout);
    cmd.AddValue("rounds", "Maximum rounds", rounds);
    cmd.AddValue("seed", "RNG seed", seed);
    cmd.Parse(argc, argv);

    layout = std::min(4u, std::max(1u, layout));
    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(1);

    WsnRoundSimulator sim(ProtocolType::LEACH, layout, rounds);
    sim.Run();
    NS_LOG_UNCOND("LEACH finished: layout=" << layout << " rounds<=" << rounds);
    return 0;
}
