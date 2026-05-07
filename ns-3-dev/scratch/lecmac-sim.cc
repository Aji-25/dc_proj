/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * lecmac-sim.cc
 * Entry point for the LECMAC protocol simulation.
 * NetAnim XML is written to results/layoutN/lecmac-animation.xml
 */

#include "wsn-protocol-model.h"

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"

#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LecmacSim");

int
main(int argc, char* argv[])
{
    uint32_t layout = 1;
    uint32_t rounds = 6000;
    uint32_t seed = 2;
    bool enableAnim = true; // set --enableAnim=false to skip XML output

    CommandLine cmd(__FILE__);
    cmd.AddValue("layout", "Layout id (1..4)", layout);
    cmd.AddValue("rounds", "Maximum simulation rounds", rounds);
    cmd.AddValue("seed", "RNG seed", seed);
    cmd.AddValue("enableAnim", "Write NetAnim XML file", enableAnim);
    cmd.Parse(argc, argv);

    layout = std::min(4u, std::max(1u, layout));
    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(1);

    // ── NetAnim: create a minimal node container so NetAnim can record ─────
    // We create NS-3 nodes purely for animation purposes; real simulation
    // state lives inside WsnRoundSimulator.
    AnimationInterface* anim = nullptr;
    NodeContainer animNodes;

    if (enableAnim)
    {
        uint32_t nodeCount = (layout == 2 || layout == 4) ? 200 : 100;

        animNodes.Create(nodeCount + 1); // +1 for the base station

        MobilityHelper mob;
        mob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mob.Install(animNodes);

        // Place sensor nodes randomly in [0, area]
        double area = (layout <= 2) ? 100.0 : 200.0;
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        rng->SetAttribute("Min", DoubleValue(0.0));
        rng->SetAttribute("Max", DoubleValue(area));

        for (uint32_t i = 0; i < nodeCount; ++i)
        {
            Ptr<ConstantPositionMobilityModel> mob_model =
                animNodes.Get(i)->GetObject<ConstantPositionMobilityModel>();
            mob_model->SetPosition(Vector(rng->GetValue(), rng->GetValue(), 0.0));
        }

        // Base station node (last node) at BS coordinates
        double bsX = (layout <= 2) ? 50.0 : 100.0;
        double bsY = (layout <= 2) ? 150.0 : 250.0;
        animNodes.Get(nodeCount)->GetObject<ConstantPositionMobilityModel>()->SetPosition(
            Vector(bsX, bsY, 0.0));

        std::string animFile = "results/layout" + std::to_string(layout) + "/lecmac-animation.xml";
        anim = new AnimationInterface(animFile);

        // Colour all sensor nodes green initially; BS is red and larger
        for (uint32_t i = 0; i < nodeCount; ++i)
        {
            anim->UpdateNodeColor(animNodes.Get(i), 0, 200, 0); // green = alive
            anim->UpdateNodeSize(animNodes.Get(i), 2.0, 2.0);
        }
        anim->UpdateNodeColor(animNodes.Get(nodeCount), 200, 0, 0); // red = BS
        anim->UpdateNodeSize(animNodes.Get(nodeCount), 5.0, 5.0);

        NS_LOG_UNCOND("NetAnim output: " << animFile);
    }

    // ── Run the actual LECMAC simulation ──────────────────────────────────────
    WsnRoundSimulator sim(ProtocolType::LECMAC, layout, rounds);
    sim.Run();

    NS_LOG_UNCOND("LECMAC finished: layout=" << layout << " rounds<=" << rounds);

    // Cleanup
    if (anim)
    {
        delete anim;
    }

    return 0;
}
