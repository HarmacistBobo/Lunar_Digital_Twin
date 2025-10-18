#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "LDT_shared.h"
#include <iostream>

using namespace ns3;

// ---------------------------------------------------------------------
// generateNodeMapXML()
// ---------------------------------------------------------------------
void generateNodeMapXML(const std::vector<NodeConfig>& nodes,
                        const std::string& outputPath)
{
    if (nodes.empty()) {
        std::cerr << "[ERROR] No node data provided to generateNodeMapXML().\n";
        return;
    }

    std::cout << "[INFO] Generating XML map: " << outputPath << std::endl;

    // Create node container
    NodeContainer nodeContainer;
    nodeContainer.Create(nodes.size());

    // Assign static positions using a MobilityHelper
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    for (size_t i = 0; i < nodes.size(); ++i) {
        const NodeConfig& cfg = nodes[i];
        positionAlloc->Add(Vector(cfg.x, cfg.y, cfg.z));
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodeContainer);

    // Create the NetAnim animation interface
    AnimationInterface anim(outputPath);
    anim.SetMaxPktsPerTraceFile(1);

    // Update node visuals and descriptions
    for (size_t i = 0; i < nodes.size(); ++i) {
        Ptr<Node> n = nodeContainer.Get(i);
        const NodeConfig& cfg = nodes[i];

        anim.UpdateNodeDescription(n, cfg.name);

        if (cfg.type.find("Base Station") != std::string::npos ||
            cfg.type.find("gNB") != std::string::npos)
            anim.UpdateNodeColor(n, 0, 128, 0);        // green
        else if (cfg.type.find("User Equipment") != std::string::npos)
            anim.UpdateNodeColor(n, 255, 165, 0);      // orange
        else if (cfg.type.find("Gateway") != std::string::npos)
            anim.UpdateNodeColor(n, 0, 0, 255);        // blue
        else
            anim.UpdateNodeColor(n, 200, 200, 200);    // gray

        std::cout << " - Added " << cfg.name << " (" << cfg.type
                  << ") at (" << cfg.x << ", " << cfg.y << ", " << cfg.z << ")\n";
    }

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "[INFO] Node map written to " << outputPath << std::endl;
    std::cout << "      Open this file in NetAnim to visualize layout.\n";
}
