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
#include <vector>
#include "mftp_client.h"
#include <string>
#include <cstring>
#include <iostream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MiniFTP");

NS_OBJECT_ENSURE_REGISTERED (MyApp);

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
  NS_LOG_INFO("CLIENT Creation");
}

MyApp::~MyApp ()
{
  NS_LOG_INFO("CLIENT Destruction");
  m_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyApp")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<MyApp> ()
    .AddAttribute ("Local",
                   "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&MyApp::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&MyApp::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&MyApp::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
    ;
  return tid;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  NS_LOG_INFO("CLIENT Setup");
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
  m_current_command = 0;
}

void
MyApp::StartApplication (void)
{
  NS_LOG_INFO("CLIENT StartApplication");
  NS_LOG_FUNCTION_NOARGS();
  m_running = true;
  m_packetsSent = 0;
  if (InetSocketAddress::IsMatchingType (m_peer))
    {
      NS_LOG_INFO("CLIENT Calling Bind");
      m_socket->Bind ();
      NS_LOG_INFO("CLIENT Called Bind");
    }
  else
    {
      NS_LOG_INFO("CLIENT Calling Bind6");
      m_socket->Bind6 ();
      NS_LOG_INFO("CLIENT Called Bind");
    }

  m_socket->SetRecvCallback (MakeCallback (&MyApp::HandleRead, this));
  m_socket->Connect (m_peer);
  SendNextCommand();
  
}

void
MyApp::SendNextCommand(void)
{
  const char * commands[] = { "Foobar\n\n", 
                              "GET UNKNOWNFILE\n\n", 
                              "GET little.txt\n\n",
                              "GET big.txt\n\n",
                              "GET huge.txt\n\n",
			      "GET giant.txt\n\n"
                      };

  if (m_current_command < 6)//4
  {
    SendPacket (commands[m_current_command], strlen(commands[m_current_command])+1); 
    m_current_command++;
  }
}
//add to header

void
MyApp::HandleRead(Ptr<Socket> socket)
{
  NS_LOG_INFO ("CLIENT HandleRead");
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);

    if (packet !=0){ 
    	uint8_t  *buffer = new uint8_t [packet->GetSize()];
    	packet->CopyData(buffer, packet->GetSize());
    	std::string s = std::string((char*)buffer);
    	delete [] buffer;

    	NS_LOG_INFO ("CLIENT Received Packet. Payload = '" << s <<"'");
  }
  SendNextCommand();
}

void
MyApp::StopApplication (void)
{
  NS_LOG_INFO("CLIENT StopApplication");
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (const char * payload, uint32_t packetSize)
{
  NS_LOG_INFO("CLIENT SendPacket");

  Ptr<Packet> packet = Create<Packet> ((const uint8_t*)payload , packetSize);
  NS_LOG_INFO ("CLIENT Sending MSG '" << payload << "' to SERVER");
  m_socket->Send (packet);
  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                          << "s CLIENT sent "
                          <<  packet->GetSize () << " bytes");

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx (payload, packetSize);
    }
}

void
MyApp::ScheduleTx (const char * payload, uint32_t packetSize)
{
  NS_LOG_INFO("CLIENT ScheduleTx");
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this, payload, packetSize);
    }
}


}; // namespace ns3
