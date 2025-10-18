#pragma once
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include <string>
#include <vector>
#include <unordered_map>

using namespace ns3;

struct NodeConfig {
    std::string name;
    std::string type;
    double x{}, y{}, z{};
    double freqMHz{};
    double txPowerBm{};
    std::string txRate;
    std::string rxRate;
    std::vector<std::string> links;
};

// Helper: install ConstantPositionMobilityModel on a node and set its position
inline void SetNodePosition(Ptr<Node> node, const Vector& pos)
{
    MobilityHelper mh;
    mh.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mh.Install(node);
    node->GetObject<MobilityModel>()->SetPosition(pos);
}

struct NodePosition;
std::vector<std::string> findOptimalPath(
    const std::unordered_map<std::string, NodePosition>& nodes,
    const std::unordered_map<std::string, std::vector<std::string>>& adjacency,
    const std::string& start,
    const std::string& goal);