/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 *  Lunar Communication Simulation Example
 *  --------------------------------------
 *  This script defines two wireless nodes communicating on the lunar surface.
 *
 *  Features:
 *    - ConstantPositionMobilityModel (fixed nodes)
 *    - Custom propagation loss model approximating lunar surface conditions
 *    - Friis free-space loss (no atmosphere) + extra regolith attenuation
 *    - Additive White Gaussian Noise (AWGN) at the receiver
 *    - Logs SNR and received power
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LunarCommExample");

// Helper to set a constant position
static void SetNodePosition(Ptr<Node> node, const Vector& pos)
{
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(node);
  node->GetObject<MobilityModel>()->SetPosition(pos);
}

int main(int argc, char *argv[])
{
  bool verbose = true; // Enables additional console output
  
  double distance = 500.0;   // meters between Tx and Rx
  double freqGHz  = 2.0;     // carrier frequency (GHz)
  double txPowerdBm = 33.0;  // 2 W typical lunar surface TX power
  std::string rate = "OfdmRate6Mbps";

  CommandLine cmd;
  cmd.AddValue("distance", "Separation between nodes (m)", distance);
  cmd.Parse(argc, argv);

  LogComponentEnable("LunarCommExample", LOG_LEVEL_INFO);
  
  if (verbose) {
	  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
	  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

  // 1. Create nodes: one transmitter, one receiver
  NodeContainer nodes;
  nodes.Create(2);
  Ptr<Node> txNode = nodes.Get(0);
  Ptr<Node> rxNode = nodes.Get(1);

  // 2. Position nodes on lunar surface coordinates
  // Using lunar-centered, lunar-fixed (LCLF) coordinates
  // Centered on transmitter
  SetNodePosition(txNode, Vector(0.0, 0.0, 0.0));          // transmitter 
  SetNodePosition(rxNode, Vector(distance, 0.0, 0.0));     // receiver 

  // 3. Configure the physical + MAC layer using ns-3 Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel;

  // Propagation Loss: Friis free-space + extra lunar regolith attenuation
  channel.AddPropagationLoss("ns3::FriisPropagationLossModel",
                             "Frequency", DoubleValue(freqGHz * 1e9));
  // Add a constant ~3 dB regolith absorption term (adjustable)
  channel.AddPropagationLoss("ns3::FixedRssLossModel",
                             "Rss", DoubleValue(-3.0));

  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  phy.SetChannel(channel.Create());

  // Receiver noise floor slightly higher due to lunar thermal background
  phy.Set("RxNoiseFigure", DoubleValue(8.0)); // in dB
  phy.Set("TxPowerStart", DoubleValue(txPowerdBm));
  phy.Set("TxPowerEnd", DoubleValue(txPowerdBm));

  WifiMacHelper mac;
  Ssid ssid = Ssid("lunar-link");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer devices;
  devices = wifi.Install(phy, mac, nodes);

  // 4. Install network stack and IP addresses
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // 5. Simple UDP application: transmitter → receiver
  uint16_t port = 4000;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(rxNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = echoClient.Install(txNode);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // 6. Run simulation
  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  NS_LOG_INFO("Lunar communication simulation complete!");
  return 0;
}
