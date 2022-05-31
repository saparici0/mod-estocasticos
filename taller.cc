/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

//
// This ns-3 example demonstrates the use of helper functions to ease
// the construction of simulation scenarios.
//
// The simulation topology consists of a mixed wired and wireless
// scenario in which a hierarchical mobility model is used.
//
// The simulation layout consists of N backbone routers interconnected
// by an ad hoc wifi network.
// Each backbone router also has a local 802.11 network and is connected
// to a local LAN.  An additional set of (K-1) nodes are connected to
// this backbone.  Finally, a local LAN is connected to each router
// on the backbone, with L-1 additional hosts.
//
// The nodes are populated with TCP/IP stacks, and OLSR unicast routing
// on the backbone.  An example UDP transfer is shown.  The simulator
// be configured to output tcpdumps or traces from different nodes.
//
//
//          +--------------------------------------------------------+
//          |                                                        |
//          |              802.11 ad hoc, ns-2 mobility              |
//          |                                                        |
//          +--------------------------------------------------------+
//                   |       o o o (N backbone routers)       |
//               +--------+                               +--------+
//     wired LAN | mobile |                     wired LAN | mobile |
//    -----------| router |                    -----------| router |
//               ---------                                ---------
//                   |                                        |
//          +----------------+                       +----------------+
//          |     802.11     |                       |     802.11     |
//          |   infra net    |                       |   infra net    |
//          |   K-1 hosts    |                       |   K-1 hosts    |
//          +----------------+                       +----------------+
//
// We'll send data from the first wired LAN node on the first wired LAN
// to the last wireless STA on the last infrastructure net, thereby
// causing packets to traverse CSMA to adhoc to infrastructure links
//
// Note that certain mobility patterns may cause packet forwarding
// to fail (if nodes become disconnected)

#include "ns3/command-line.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/qos-txop.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/animation-interface.h"
#include "random"
using namespace ns3;

//
// Define logging keyword for this file
//
NS_LOG_COMPONENT_DEFINE("MixedWireless");

//
// This function will be used below as a trace sink, if the command-line
// argument or default value "useCourseChangeCallback" is set to true
//
/*
static void
CourseChangeCallback (std::string path, Ptr<const MobilityModel> model)
{
  Vector position = model->GetPosition ();
  std::cout << "CourseChange " << path << " x=" << position.x << ", y=" << position.y << ", z=" << position.z << std::endl;
}
*/
int main(int argc, char *argv[])
{
  std::random_device rd;     // only used once to initialise (seed) engine
  std::mt19937 rng(rd());

  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  //
  // First, we declare and initialize a few local variables that control some
  // simulation parameters.
  //
  uint32_t clusterHeadNodes = 2;
  uint32_t infraNodes[clusterHeadNodes] = {4, 3};
  uint32_t stopTime = 20;
  //bool useCourseChangeCallback = false;

  //
  // Simulation defaults are typically set next, before command line
  // arguments are parsed.
  //
  Config::SetDefault("ns3::OnOffApplication::PacketSize", StringValue("1472"));
  Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("100kb/s"));

  if (stopTime < 10)
  {
    std::cout << "Use a simulation stop time >= 10 seconds" << std::endl;
    exit(1);
  }
  ///////////////////////////////////////////////////////////////////////////
  //                                                                       //
  // Construct the Clusters                                                //
  //                                                                       //
  ///////////////////////////////////////////////////////////////////////////

  //
  // Creamos un NodeContainer donde estarán los clusterheads (nivel de jerarquía)
  //
  //
  // Assign IPv4 addresses to the device drivers (actually to the associated
  // IPv4 interfaces) we just created.
  //
  Ipv4AddressHelper ipAddrs[3];
  ipAddrs[0].SetBase("192.167.0.0", "255.255.255.0");
  ipAddrs[1].SetBase("192.168.0.0", "255.255.255.0");
  ipAddrs[2].SetBase("192.169.0.0", "255.255.255.0");
  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsr); // has effect on the next Install ()

  NodeContainer clusters[clusterHeadNodes];
  NetDeviceContainer clusterDevices[clusterHeadNodes];
  WifiHelper wifi;
  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate54Mbps"));
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel[clusterHeadNodes];

  NodeContainer head_cluster = NodeContainer();
  for (int i = 0; i < int(clusterHeadNodes); ++i)
  {
    wifiChannel[i] = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel[i].Create());

    clusters[i].Create(infraNodes[i]);    

    clusterDevices[i] = wifi.Install(wifiPhy, mac, clusters[i]);
    //
    // Add the IPv4 protocol stack to the nodes in our container
    // Add the IPv4 protocol stack to the new LAN nodes
    //
    internet.Install (clusters[i]);
    //
    // Assign IPv4 addresses to the device drivers (actually to the
    // associated IPv4 interfaces) we just created.
    //
    ipAddrs[i].Assign (clusterDevices[i]);
    //
    // Assign a new network prefix for the next LAN, according to the
    // network mask initialized above
    //
    ipAddrs[i].NewNetwork ();

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue ((i+1)*50.0),
                                 "MinY", DoubleValue ((i+1)*20.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
    mobility.Install(clusters[i]);
    /*
    for(int j = 0; j < int(infraNodes[i]); ++j){
      Ptr< NetDevice > dev1 = clusterDevices[i].Get(j);
      for(int k = 0; k < int(infraNodes[i]); ++k){
        if(j == k) continue;
        Ptr< NetDevice > dev2 = clusterDevices[i].Get(k);
        Packet pack = Packet(10);
        //printf("%d", Ptr(&pack)->GetSize());
        //dev1->Send(Ptr(&pack), dev2->GetAddress(), 17);
        bool r = dev1->IsLinkUp();
        printf("%d", r);
        fflush(stdout);
        if(!r){
          printf("Fallo el envio desde");
          
        }
      }
    }*/
    std::uniform_int_distribution<int> uni(0,infraNodes[i]-1); 
    auto random_integer = uni(rng);
    head_cluster.Add(clusters[i].Get(random_integer));
  }

  Ipv4AddressHelper ipAddrsH;
  // Reset the address base-- all of the CSMA networks will be in
  // the "172.16 address space
  ipAddrsH.SetBase ("172.16.0.0", "255.255.255.0");

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate",
                            DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer head_clusterDevices = csma.Install (head_cluster);
  //
  // Assign IPv4 addresses to the device drivers (actually to the
  // associated IPv4 interfaces) we just created.
  //
  ipAddrsH.Assign (head_clusterDevices);
  //
  // Assign a new network prefix for the next LAN, according to the
  // network mask initialized above
  //
  ipAddrsH.NewNetwork ();


  /*
  ///////////////////////////////////////////////////////////////////////////
  //                                                                       //
  // Application configuration                                             //
  //                                                                       //
  ///////////////////////////////////////////////////////////////////////////

  // Create the OnOff application to send UDP datagrams of size
  // 210 bytes at a rate of 10 Kb/s, between two nodes
  // We'll send data from the first wired LAN node on the first wired LAN
  // to the last wireless STA on the last infrastructure net, thereby
  // causing packets to traverse CSMA to adhoc to infrastructure links

  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9;   // Discard port (RFC 863)

  // Let's make sure that the user does not define too few nodes
  // to make this example work.  We need lanNodes > 1  and infraNodes > 1
  NS_ASSERT (lanNodes > 1 && infraNodes > 1);
  // We want the source to be the first node created outside of the backbone
  // Conveniently, the variable "backboneNodes" holds this node index value
  Ptr<Node> appSource = NodeList::GetNode (backboneNodes);
  // We want the sink to be the last node created in the topology.
  uint32_t lastNodeIndex = backboneNodes + backboneNodes * (lanNodes - 1) + backboneNodes * (infraNodes - 1) - 1;
  Ptr<Node> appSink = NodeList::GetNode (lastNodeIndex);
  // Let's fetch the IP address of the last node, which is on Ipv4Interface 1
  Ipv4Address remoteAddr = appSink->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (remoteAddr, port)));

  ApplicationContainer apps = onoff.Install (appSource);
  apps.Start (Seconds (3));
  apps.Stop (Seconds (stopTime - 1));

  // Create a packet sink to receive these packets
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  apps = sink.Install (appSink);
  apps.Start (Seconds (3));

  ///////////////////////////////////////////////////////////////////////////
  //                                                                       //
  // Tracing configuration                                                 //
  //                                                                       //
  ///////////////////////////////////////////////////////////////////////////

  NS_LOG_INFO ("Configure Tracing.");
  CsmaHelper csma;

  //
  // Let's set up some ns-2-like ascii traces, using another helper class
  //
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("taller.tr");
  wifiPhy.EnableAsciiAll (stream);
  csma.EnableAsciiAll (stream);
  internet.EnableAsciiIpv4All (stream);

  // Csma captures in non-promiscuous mode
  csma.EnablePcapAll ("taller", false);
  // pcap captures on the backbone wifi devices
  wifiPhy.EnablePcap ("taller", clusterDevices[0], false);
  // pcap trace on the application data sink
  wifiPhy.EnablePcap ("taller", appSink->GetId (), 0);

  if (useCourseChangeCallback == true)
    {
      Config::Connect ("/NodeList/$ns3::MobilityModel/CourseChange", MakeCallback (&CourseChangeCallback));
    }
*/
  AnimationInterface anim("taller.xml");

  ///////////////////////////////////////////////////////////////////////////
  //                                                                       //
  // Run simulation                                                        //
  //                                                                       //
  ///////////////////////////////////////////////////////////////////////////

  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();
  Simulator::Destroy();
}
