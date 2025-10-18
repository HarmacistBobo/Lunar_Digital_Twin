/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
  Lunar DT: minimal geometry only

  What this program does
    1) Creates nodes for Earth, a Lunar Gateway, two gNB placeholders, and three UE placeholders.
    2) Places them in a single global Cartesian frame measured in meters.
       Earth is at (0, 0, 0).
       The lunar frame is a translated copy of the global frame shifted by L on +X.
       All lunar nodes share that +L offset plus small local offsets for realism.
    3) Exports a NetAnim file so you can see the layout.
       There are no NetDevices and no apps yet. This is a clean starting point.

  How to extend later
    Add radios and links: replace placeholders with real 5G-LENA helpers when ready.
    Keep the same positions to preserve your geometry.
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

// Helper: install ConstantPositionMobilityModel on a node and set its position
static void SetNodePosition(Ptr<Node> node, const Vector& pos)
{
  MobilityHelper mh;
  mh.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mh.Install(node);                       // pass Ptr<Node>, not a Node value
  node->GetObject<MobilityModel>()->SetPosition(pos);
}

int main(int argc, char* argv[])
{
  // Visual scale
  // Real Earth-Moon distance is about 384400 km.
  // For NetAnim readability we compress it to L meters along +X.
  double L = 1200.0;                      // meters, global X offset for the Moon frame
  std::string animFile = "lunar_dt_min.xml";

  CommandLine cmd;
  cmd.AddValue("L", "Moon offset in meters along +X used for visualization", L);
  cmd.AddValue("animFile", "NetAnim output filename", animFile);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer earth;     earth.Create(1);
  NodeContainer lunarGw;   lunarGw.Create(1);
  NodeContainer gnbNodes;  gnbNodes.Create(2);   // placeholders for BS
  NodeContainer ueNodes;   ueNodes.Create(3);    // placeholders for UEs

  // Shortcuts
  Ptr<Node> nEarth = earth.Get(0);
  Ptr<Node> nGw    = lunarGw.Get(0);
  Ptr<Node> nGnb0  = gnbNodes.Get(0);
  Ptr<Node> nGnb1  = gnbNodes.Get(1);
  Ptr<Node> nUe0   = ueNodes.Get(0);
  Ptr<Node> nUe1   = ueNodes.Get(1);
  Ptr<Node> nUe2   = ueNodes.Get(2);

  // Place nodes
  // Earth at the origin of the global frame
  SetNodePosition(nEarth, Vector(0.0,    0.0, 0.0));

  // Lunar frame origin sits at (L, 0, 0) in the same global space
  // All lunar positions are this base plus small local offsets
  SetNodePosition(nGw,   Vector(L +   0.0,    0.0, 0.0));  // Lunar Gateway at lunar frame origin

  // Two gNB placeholders on the surface near the gateway
  SetNodePosition(nGnb0, Vector(L +  40.0,   10.0, 0.0));
  SetNodePosition(nGnb1, Vector(L + 180.0,  -5.0, 0.0));

  // Three UE placeholders around those BS
  SetNodePosition(nUe0,  Vector(L +  60.0,   25.0, 0.0));  // near gNB0
  SetNodePosition(nUe1,  Vector(L + 200.0,  -20.0, 0.0));  // near gNB1
  SetNodePosition(nUe2,  Vector(L + 220.0,   15.0, 0.0));  // near gNB1

  // NetAnim setup
  AnimationInterface anim(animFile);
  anim.SetMaxPktsPerTraceFile(1);         // no packets, keeps file tiny

  // Labels
  anim.UpdateNodeDescription(nEarth, "Earth");
  anim.UpdateNodeDescription(nGw,    "LunarGW");
  anim.UpdateNodeDescription(nGnb0,  "gNB0");
  anim.UpdateNodeDescription(nGnb1,  "gNB1");
  anim.UpdateNodeDescription(nUe0,   "UE0");
  anim.UpdateNodeDescription(nUe1,   "UE1");
  anim.UpdateNodeDescription(nUe2,   "UE2");

  // Colors to group nodes visually
  anim.UpdateNodeColor(nEarth, 255,   0,   0);   // Earth in red
  anim.UpdateNodeColor(nGw,      0,   0, 255);   // gateway in blue
  anim.UpdateNodeColor(nGnb0,    0, 128,   0);   // BS in green
  anim.UpdateNodeColor(nGnb1,    0, 128,   0);
  anim.UpdateNodeColor(nUe0,   255, 165,   0);   // UEs in orange
  anim.UpdateNodeColor(nUe1,   255, 165,   0);
  anim.UpdateNodeColor(nUe2,   255, 165,   0);

  // Nothing to run besides writing the animation file
  Simulator::Stop(Seconds(0.1));
  Simulator::Run();
  Simulator::Destroy();

  std::cout << "Wrote NetAnim file: " << animFile << std::endl;
  std::cout << "Open it with NetAnim to see Earth at the origin and the lunar cluster at X near L" << std::endl;
  return 0;
}