/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
 * 
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
 * Author:  Tom Henderson (tomhend@u.washington.edu)
 */

#include "mftp_server.h"
#include "ns3/data-rate.h"
#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MY_PacketSink");

NS_OBJECT_ENSURE_REGISTERED (PacketSink);

TypeId 
PacketSink::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PacketSink")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<PacketSink> ()
    .AddAttribute ("Local",
                   "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&PacketSink::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&PacketSink::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&PacketSink::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
  ;
  return tid;
}

PacketSink::PacketSink ()

{
  m_dataRate = DataRate("56kbps");
  m_packetSize = 1040;
  m_running = false;
  NS_LOG_INFO("SERVER Creation");
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_totalRx = 0;
}

PacketSink::~PacketSink()
{
  NS_LOG_FUNCTION (this);
}

uint64_t PacketSink::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
PacketSink::GetListeningSocket (void) const
{
  NS_LOG_INFO("SERVER GetListeningSocket");
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket> >
PacketSink::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void PacketSink::DoDispose (void)
{
  NS_LOG_INFO("SERVER DoDispose");
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}


// Application Methods
void PacketSink::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_INFO("SERVER StartApplication");
  NS_LOG_FUNCTION (this);
  m_running = true;
  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      m_socket->Bind (m_local);
      m_socket->Listen ();
      //m_socket->ShutdownSend ();

      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: joining multicast on a non-UDP socket");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&PacketSink::HandleRead, this));
  m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&PacketSink::HandleAccept, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&PacketSink::HandlePeerClose, this),
    MakeCallback (&PacketSink::HandlePeerError, this));
}

void PacketSink::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_INFO("SERVER StopApplication");
  NS_LOG_FUNCTION (this);
  m_running = false;
  if (m_sendEvent.IsRunning ())
  {
    Simulator::Cancel (m_sendEvent);
  }


  while(!m_socketList.empty ()) //these are accepted sockets, close them
    {
      Ptr<Socket> acceptedSocket = m_socketList.front ();
      m_socketList.pop_front ();
      acceptedSocket->Close ();
    }
  if (m_socket) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void PacketSink::SendPacket(Ptr<Socket> socket, const char *payload, uint32_t payload_length)
{
  NS_LOG_INFO("SERVER SendPacket " << payload);
  Ptr<Packet> packet = Create<Packet> ((const uint8_t*)payload , payload_length);
  socket->Send (packet);
}

#if 0
void
PacketSink::ScheduleTx (Ptr<Socket> socket, const char * payload, uint32_t packetSize)
{
  NS_LOG_INFO("SERVER ScheduleTx");
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &PacketSink::SendPacket, socket, payload, packetSize);
    }
}

#endif

void PacketSink::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_INFO("SERVER HandleRead");
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from))){
      
      uint8_t  *buffer = new uint8_t [packet->GetSize()];
      packet->CopyData(buffer, packet->GetSize());
      std::string s = std::string((char*)buffer);
      delete [] buffer;
      NS_LOG_INFO ("SERVER Received Packet. Payload = '" << s <<"'");

// do analysis of incoming packet and reply here
//      if(s == "GET hello.mp3"){
//        SendPacket(socket, s.c_str(),s.size()+1);
//      }
      std::string outgoing = "";
      if (0 != s.substr(0,4).compare("GET "))
      {
// there is only one legal command, and they did not send it
// bounce them with 202 Command Not Implemented
          outgoing = "202 Command Not Implemented\n\n";
      }
      else
      { 
         s.erase(0,4);
         if (0==s.compare("little.txt\n\n"))
         {
           outgoing = "200 OK 1\n\nA";
         }
         else if (0==s.compare("big.txt\n\n"))
         {
           outgoing = "200 OK 11\n\nA big file.";
         }
	 else if (0==s.compare("huge.txt\n\n"))
         {
           outgoing = "200 OK 31\n\nAn even bigger file.\nAnd more!";
         }
         else if (0==s.compare("giant.txt\n\n"))
         {
           outgoing = "200 OK 12\n\nJolly, Green";
         }
         else
         {
           outgoing = "550 File Unavailable\n\n";
         }
      }
     
// do the reply here
      std::cout << "Server outgoing = '" << outgoing << "'\n";
      if (outgoing.size()>0)
      {
        std::cout << "Server sending '" << outgoing << "'" << std::endl;
        SendPacket(socket, outgoing.c_str(), outgoing.size()+1);
      }
	
      if (packet->GetSize () == 0)
        { //EOF
          break;
        }
      m_totalRx += packet->GetSize ();
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s SERVER received "
                       <<  packet->GetSize () << " bytes from "
                       << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                       << " port " << InetSocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << m_totalRx << " bytes");
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s SERVER received "
                       <<  packet->GetSize () << " bytes from "
                       << Inet6SocketAddress::ConvertFrom(from).GetIpv6 ()
                       << " port " << Inet6SocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << m_totalRx << " bytes");
        }
	
//      if(s == "GET hello.mp3"){
//	NS_LOG_INFO("SERVER Sending MSG 'hello.mp3' to CLIENT");	
//      }

      m_rxTrace (packet, from); // receives packet source address
    }
}


void PacketSink::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_INFO("SERVER HandlePeerClose");
  NS_LOG_FUNCTION (this << socket);
}
 
void PacketSink::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_INFO("SERVER HandlePeerError");
  NS_LOG_FUNCTION (this << socket);
}
 

void PacketSink::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_INFO("SERVER HandleAccept");
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback (&PacketSink::HandleRead, this));
  m_socketList.push_back (s);
}

} // Namespace ns3
