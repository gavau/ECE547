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
 */

#include "mftp_server_helper.h"
#include "mftp_client_helper.h"
#include "mftp_client.h"
#include "ns3/csma-helper.h"

#include <list>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("main");

int
main (int argc, char *argv[])
{
  
  uint32_t packetSize = 1040; // bytes
  uint32_t numNodes = 10;  // by default, 5x5
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 1;
  uint32_t nPackets = 1;
  
  bool verbose = true;
  bool tracing = true;
  bool useV6 = false;
       
  CommandLine cmd;

  cmd.AddValue ("useIpv6", "Use Ipv6", useV6);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("nPackets", "number of packets generated", nPackets);
  cmd.AddValue ("verbose", "turn off all WifiNetDevice log components", verbose);
  cmd.AddValue ("tracing", "turn on ascii and pcap tracing", tracing);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
  cmd.AddValue ("sinkNode", "Receiver node number", sinkNode);
  cmd.AddValue ("sourceNode", "Sender node number", sourceNode);
  cmd.Parse (argc, argv);


  NodeContainer nodesClient;
  NodeContainer nodesServer;
  nodesServer.Create(2);
  nodesClient.Create(numNodes >2 ? numNodes : 2);
  NodeContainer nodes(nodesServer,nodesClient);
  

  if(verbose){
  	LogComponentEnable("MY_PacketSink",LOG_INFO);
	LogComponentEnable("MiniFTP",LOG_INFO);	
  } 

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("56Kbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = csma.Install (nodes); 
  InternetStackHelper stack;
  stack.Install (nodes);

  uint16_t sinkPort = 8080;
  Address sinkAddress;
  Address anyAddress;
  std::string probeType;
  std::string tracePath;
  if (useV6 == false)
    {
      Ipv4AddressHelper address;
      address.SetBase ("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer interfaces = address.Assign (devices);
      sinkAddress = InetSocketAddress (interfaces.GetAddress (0), sinkPort);//1 to 7
      anyAddress = InetSocketAddress (Ipv4Address::GetAny (), sinkPort);
    }
  else
    {
      Ipv6AddressHelper address;
      address.SetBase ("2001:0000:f00d:cafe::", Ipv6Prefix (64));
      Ipv6InterfaceContainer interfaces = address.Assign (devices);
      sinkAddress = Inet6SocketAddress (interfaces.GetAddress (7,7), sinkPort);//changed from (1,1) to (7,7)
      anyAddress = Inet6SocketAddress (Ipv6Address::GetAny (), sinkPort);
    }

     PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", anyAddress);
     ApplicationContainer sinkApps2 = packetSinkHelper.Install (nodesServer);
     sinkApps2.Start (Seconds (0.));
     sinkApps2.Stop (Seconds (20));

     MyAppHelper MyAppHelper ("ns3::TcpSocketFactory", anyAddress);
     ApplicationContainer sourceApps2 = MyAppHelper.Install (nodesClient);

     sinkApps2.Start (Seconds (1.));
     sinkApps2.Stop (Seconds (20));

  std::list <Ptr<Socket> > socket_list;
  uint32_t index = 0;
  for (NodeContainer::Iterator i = nodesClient.Begin (); i != nodesClient.End (); ++i)
  {
    Ptr<Socket> sock = Socket::CreateSocket ( *i, TcpSocketFactory::GetTypeId ());
    socket_list.push_back(sock);
    Ptr<Application> myapp = (*i)->GetApplication(0);
    Ptr<MyApp> *app = (Ptr<MyApp> *) &myapp;
    (*app)->Setup(sock, sinkAddress, packetSize, nPackets, DataRate ("56kbps"));
    (*app)->SetStartTime(Seconds(2*index + 1.0));
    (*app)->SetStopTime(Seconds(2*index + 2.0));
    index++;
  }


  if (tracing == true)
    {
      csma.EnablePcapAll ("project_4");
      csma.EnablePcap ("project_4", devices.Get (0), true); // output packets from server 0
      csma.EnablePcap ("project_4", devices.Get (1), true); // output packets from server 1
    }
  
  Simulator::Stop (Seconds (23));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

