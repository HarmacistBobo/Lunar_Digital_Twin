// basicLunarComm1.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <string>
#include "LDT_shared.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LunarCommExample");

void simulateTransmission(double distance, double freqMHz, double txPowerdBm, std::string rate)
{
  bool verbose = true;
  double freqGHz = freqMHz / 1000.0; // MHz → GHz

  LogComponentEnable("LunarCommExample", LOG_LEVEL_INFO);

  if (verbose) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);
  Ptr<Node> txNode = nodes.Get(0);
  Ptr<Node> rxNode = nodes.Get(1);

  SetNodePosition(txNode, Vector(0.0, 0.0, 0.0));
  SetNodePosition(rxNode, Vector(distance, 0.0, 0.0));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel;

  channel.AddPropagationLoss("ns3::FriisPropagationLossModel",
                             "Frequency", DoubleValue(freqGHz * 1e9));
  channel.AddPropagationLoss("ns3::FixedRssLossModel",
                             "Rss", DoubleValue(-3.0));
  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  phy.SetChannel(channel.Create());

  phy.Set("RxNoiseFigure", DoubleValue(8.0));
  phy.Set("TxPowerStart", DoubleValue(txPowerdBm));
  phy.Set("TxPowerEnd", DoubleValue(txPowerdBm));

  WifiMacHelper mac;
  Ssid ssid = Ssid("lunar-link");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  uint16_t port = 4000;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  NS_LOG_INFO("Lunar communication simulation complete!");
}
