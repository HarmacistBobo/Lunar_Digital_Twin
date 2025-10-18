/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
  Lunar DT CI Path-Loss Simulation Module (Integrated Version)
  ------------------------------------------------------------
  Refactored for integration with LDT_main.cc.
  Automatically reads configuration file from: ./scratch/config/LTE_config/

  Original code by Dr. Seth
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/node-list.h"
#include "ns3/config-store-module.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <filesystem>

using namespace ns3;
namespace fs = std::filesystem;

// --------------------------- Config File Parser ------------------------------
static bool LoadConfigFile(const std::string& path, std::unordered_map<std::string, std::string>& kv)
{
  std::ifstream in(path.c_str());
  if (!in.is_open())
  {
    std::cerr << "[ERROR] Could not open config file: " << path << std::endl;
    return false;
  }

  auto trim = [](std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
  };

  std::string line;
  while (std::getline(in, line))
  {
    auto posHash = line.find('#');
    if (posHash != std::string::npos)
      line = line.substr(0, posHash);
    trim(line);
    if (line.empty())
      continue;

    auto eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);
    trim(key);
    trim(val);
    if (!key.empty())
      kv[key] = val;
  }
  return true;
}

// --------------------------- Mobility Helpers -------------------------------
static void EnsureMobilityOnAllNodes(double L)
{
  for (uint32_t i = 0; i < NodeList::GetNNodes(); ++i)
  {
    Ptr<Node> node = NodeList::GetNode(i);
    Ptr<MobilityModel> mm = node->GetObject<MobilityModel>();
    if (!mm)
    {
      MobilityHelper mh;
      mh.SetMobilityModel("ns3::ConstantPositionMobilityModel");
      mh.Install(node);
      double x = L + 10.0 + 3.0 * i;
      node->GetObject<MobilityModel>()->SetPosition(Vector(x, -60.0, 0.0));
    }
  }
}

// --------------------------- Utility Functions -------------------------------
static double Fspl1m_dB(double fGHz)
{
  return 32.44 + 20.0 * std::log10(fGHz); // FSPL(1m)
}

static Ptr<NetDevice> PickNearestEnb(Ptr<Node> ueNode, const NetDeviceContainer& enbDevs)
{
  Ptr<MobilityModel> mmUe = ueNode->GetObject<MobilityModel>();
  Vector uePos = mmUe->GetPosition();
  double bestDist2 = std::numeric_limits<double>::infinity();
  Ptr<NetDevice> bestEnb = nullptr;

  for (uint32_t i = 0; i < enbDevs.GetN(); ++i)
  {
    Ptr<Node> enbNode = enbDevs.Get(i)->GetNode();
    Ptr<MobilityModel> mmEnb = enbNode->GetObject<MobilityModel>();
    Vector enbPos = mmEnb->GetPosition();
    double dx = uePos.x - enbPos.x;
    double dy = uePos.y - enbPos.y;
    double dz = uePos.z - enbPos.z;
    double d2 = dx * dx + dy * dy + dz * dz;
    if (d2 < bestDist2)
    {
      bestDist2 = d2;
      bestEnb = enbDevs.Get(i);
    }
  }
  return bestEnb;
}

// ----------------------------------------------------------------------------
// Callable entry point for LDT_main.cc
// ----------------------------------------------------------------------------
int runLunarDtCI(int argc, char* argv[])
{
  std::cout << "\n[INFO] === Starting Lunar CI LTE Simulation ===" << std::endl;

  double L = 1200.0;
  double fGHz = 2.1;
  double n = 2.2;
  double gEnb = 8.0;
  double gUe = 0.0;
  std::string animFile = "lunar_dt_min_ci.xml";
  std::string conf;

  // Default configuration file path
  std::string defaultConfPath = "../scratch/config/LTE_config/lunar_dt.conf";

  CommandLine cmd;
  cmd.AddValue("L", "Moon offset in meters along +X used for visualization", L);
  cmd.AddValue("fGHz", "Carrier frequency in GHz", fGHz);
  cmd.AddValue("n", "CI path-loss exponent", n);
  cmd.AddValue("gEnb", "eNB isotropic antenna gain (dBi)", gEnb);
  cmd.AddValue("gUe", "UE isotropic antenna gain (dBi)", gUe);
  cmd.AddValue("animFile", "NetAnim output filename", animFile);
  cmd.AddValue("conf", "Path to config file", conf);
  cmd.Parse(argc, argv);

  // If no --conf provided, use default location
  if (conf.empty())
  {
    conf = defaultConfPath;
    std::cout << "[INFO] No configuration path specified. Using default: " << conf << std::endl;
  }

  if (!fs::exists(conf))
  {
    std::cerr << "[ERROR] Configuration file not found: " << conf << std::endl;
    return -1;
  }

  std::unordered_map<std::string, std::string> kv;
  if (LoadConfigFile(conf, kv))
  {
    if (kv.count("L")) L = std::stod(kv["L"]);
    if (kv.count("fGHz")) fGHz = std::stod(kv["fGHz"]);
    if (kv.count("n")) n = std::stod(kv["n"]);
    if (kv.count("gEnb")) gEnb = std::stod(kv["gEnb"]);
    if (kv.count("gUe")) gUe = std::stod(kv["gUe"]);
    if (kv.count("animFile")) animFile = kv["animFile"];
  }

  NodeContainer earth;     earth.Create(1);
  NodeContainer lunarGw;   lunarGw.Create(1);
  NodeContainer gnbNodes;  gnbNodes.Create(2);
  NodeContainer ueNodes;   ueNodes.Create(3);

  Ptr<Node> nEarth = earth.Get(0);
  Ptr<Node> nGw    = lunarGw.Get(0);
  Ptr<Node> nGnb0  = gnbNodes.Get(0);
  Ptr<Node> nGnb1  = gnbNodes.Get(1);
  Ptr<Node> nUe0   = ueNodes.Get(0);
  Ptr<Node> nUe1   = ueNodes.Get(1);
  Ptr<Node> nUe2   = ueNodes.Get(2);

  SetNodePosition(nEarth, Vector(0.0, 0.0, 0.0));
  SetNodePosition(nGw,    Vector(L + 0.0, 0.0, 0.0));
  SetNodePosition(nGnb0,  Vector(L + 40.0, 10.0, 0.0));
  SetNodePosition(nGnb1,  Vector(L + 180.0, -5.0, 0.0));
  SetNodePosition(nUe0,   Vector(L + 60.0, 25.0, 0.0));
  SetNodePosition(nUe1,   Vector(L + 200.0, -20.0, 0.0));
  SetNodePosition(nUe2,   Vector(L + 220.0, 15.0, 0.0));

  InternetStackHelper internet;
  internet.Install(ueNodes);

  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  double refLoss = Fspl1m_dB(fGHz);
  lteHelper->SetAttribute("PathlossModel", StringValue("ns3::LogDistancePropagationLossModel"));
  Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceDistance", DoubleValue(1.0));
  Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue(refLoss));
  Config::SetDefault("ns3::LogDistancePropagationLossModel::Exponent", DoubleValue(n));

  Config::SetDefault("ns3::IsotropicAntennaModel::Gain", DoubleValue(gEnb));
  NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(gnbNodes);
  Config::SetDefault("ns3::IsotropicAntennaModel::Gain", DoubleValue(gUe));
  NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

  for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
  {
    Ptr<NetDevice> ueDev = ueDevs.Get(i);
    Ptr<Node> ueNode = ueDev->GetNode();
    Ptr<NetDevice> bestEnbDev = PickNearestEnb(ueNode, enbDevs);
    lteHelper->Attach(ueDev, bestEnbDev);
  }

  EnsureMobilityOnAllNodes(L);

  AnimationInterface anim(animFile);
  anim.SetMaxPktsPerTraceFile(1);

  anim.UpdateNodeDescription(nEarth, "Earth");
  anim.UpdateNodeDescription(nGw, "LunarGW");
  anim.UpdateNodeDescription(nGnb0, "gNB0");
  anim.UpdateNodeDescription(nGnb1, "gNB1");
  anim.UpdateNodeDescription(nUe0, "UE0");
  anim.UpdateNodeDescription(nUe1, "UE1");
  anim.UpdateNodeDescription(nUe2, "UE2");

  anim.UpdateNodeColor(nEarth, 255, 0, 0);
  anim.UpdateNodeColor(nGw, 0, 0, 255);
  anim.UpdateNodeColor(nGnb0, 0, 128, 0);
  anim.UpdateNodeColor(nGnb1, 0, 128, 0);
  anim.UpdateNodeColor(nUe0, 255, 165, 0);
  anim.UpdateNodeColor(nUe1, 255, 165, 0);
  anim.UpdateNodeColor(nUe2, 255, 165, 0);

  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();

  std::cout << "[INFO] CI LTE Simulation Complete.\n"
            << "  Config File: " << conf << "\n"
            << "  NetAnim File: " << animFile << "\n"
            << "  Path-Loss: FSPL(1m)=" << refLoss
            << " dB, exponent n=" << n << ", f=" << fGHz << " GHz\n" << std::endl;

  return 0;
}
