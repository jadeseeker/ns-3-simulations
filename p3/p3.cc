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
 * Authors: Josh Lyons; Srikanth Pisharody
 */
 
 
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"


#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#define nMAXNODES	1000

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("P3");

double	globalTxBytes = 0;

void txPacketCounter ( ns3::Ptr<ns3::Packet const> a ) {
	globalTxBytes += 256;
}

int main (int argc, char *argv[])
{
	
	uint32_t		nodeCount = 2;
	uint32_t		areaWidth = 1000;
	uint32_t		pktSize = 256;
	uint32_t		udpRateInt;
	double			intensity = 0.5;
	double			transPower = 100.0;
	double			transPowerDbm = 0.0;
	std::string		protocol = "OLSR";
	std::string 	phyMode ("DsssRate11Mbps");
	std::string		udpDataRate = "1Mbps";
	bool			verbose = 0;
	int 			count = 1;
		
	CommandLine cmd;	
	cmd.AddValue ("nodeCount", "Number of wifi nodes", nodeCount);
	cmd.AddValue ("areaWidth", "Width of square LAN area in meters", areaWidth);
	cmd.AddValue ("transmitPower", "Transmit power of all nodes in mW", transPower);
	cmd.AddValue ("routingProtocol", "Wifi routing protocol to use", protocol);
	cmd.AddValue ("verbose", "Turn on module logging", verbose);
	cmd.AddValue ("intensity","Traffic intensity on the network",intensity);
	cmd.AddValue ("count","run counter",count);
	
	cmd.Parse (argc, argv);
	
	if( verbose ) { LogComponentEnable ("P3", LOG_LEVEL_ALL); }
	
	if( nodeCount < 2 ) { nodeCount = 2; }
	
	if ((protocol != "AODV") && (protocol != "OLSR"))
    {
      NS_ABORT_MSG ("Invalid protocol type: Use --routingProtocol=AODV or --routingProtocol=OLSR");
    }
	
	
	udpRateInt = intensity * 11000;
	std::stringstream temp;
	temp << udpRateInt << "kbps";
	udpDataRate = temp.str();
	NS_LOG_INFO ("UDP data rate set to " << udpDataRate);
	
	Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (pktSize));
	Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (udpDataRate));
	
	// Convet mW to dbm
	transPowerDbm = 10.0 * std::log10(transPower);
	
	NodeContainer clientNodes;
	clientNodes.Create(nodeCount);
	
	NS_LOG_INFO ("Building WIFI model.");
		
	WifiHelper wifi;
	if (0)
	{
	  wifi.EnableLogComponents ();  // Turn on all Wifi logging
	}
	wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
	
	YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
	// set it to zero; otherwise, gain will be added
	wifiPhy.Set ("RxGain", DoubleValue (0) ); 
	// ns-3 supports RadioTap and Prism tracing extensions for 802.11b
	wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
	wifiPhy.Set ("TxPowerStart", DoubleValue (transPowerDbm) );
	wifiPhy.Set ("TxPowerEnd", DoubleValue (transPowerDbm) );

	YansWifiChannelHelper wifiChannel;
	wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
	wifiPhy.SetChannel (wifiChannel.Create ());

	// Add a non-QoS upper mac, and disable rate control
	NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
	wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
	wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
								"DataMode",StringValue (phyMode),
								"ControlMode",StringValue (phyMode));
	// Set it to adhoc mode
	wifiMac.SetType ("ns3::AdhocWifiMac");
	NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, clientNodes);
	
	NS_LOG_INFO ("Building Mobility Model.");
	MobilityHelper mobility;
	
	std::stringstream xyPos;
	xyPos << "ns3::UniformRandomVariable[Min=0.0|Max=" << areaWidth << "]";
		
	mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
									"X", StringValue (xyPos.str ()),
									"Y", StringValue (xyPos.str ()));
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (clientNodes);
	

	// iterate our nodes and print their position.
	for (NodeContainer::Iterator j = clientNodes.Begin (); j != clientNodes.End (); ++j)
	{
		Ptr<Node> object = *j;
		Ptr<MobilityModel> position = object->GetObject<MobilityModel> ();
		NS_ASSERT (position != 0);
		Vector pos = position->GetPosition ();
		if(verbose) {
			std::cout << "x=" << pos.x << ", y=" << pos.y << ", z=" << pos.z << std::endl;
		}
	}
	
	// Enable Routing protocol
	Ipv4StaticRoutingHelper staticRouting;
	Ipv4ListRoutingHelper list;
	if(protocol == "OLSR")	{
		NS_LOG_INFO ("Assigning OLSR Routing Protocol.");
		OlsrHelper olsr;
		
		list.Add (staticRouting, 0);
		list.Add (olsr, 10);
	} else { 
		NS_LOG_INFO ("Assigning AODV Routing Protocol.");
		AodvHelper aodv;
		
		list.Add (staticRouting, 0);
		list.Add (aodv, 10);		
	}

	InternetStackHelper internet;
	internet.SetRoutingHelper (list); // has effect on the next Install ()
	internet.Install (clientNodes);
	
	Ipv4AddressHelper ipv4;
	NS_LOG_INFO ("Assigning IP Addresses.");
	ipv4.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);
	
	// Install on/off app on nodes
	ApplicationContainer udpSourceApps[nMAXNODES];
	
	uint32_t *a=new uint32_t [nodeCount];          
      
	for(uint32_t i = 0; i < nodeCount; ++i) {
		uint32_t randNode;
		inHere : {
			for(int i=0;i<count;i++){
				randNode = rand () % nodeCount;
			}
		}
		uint32_t k=0;
		while (k!=i && i!=0)
		{
			if(randNode == a[k] || randNode == i)
				goto inHere;
			k++;
		}
		
		a[i] = randNode;
                        
		OnOffHelper source ("ns3::UdpSocketFactory",
								 InetSocketAddress (interfaces.GetAddress(randNode), 5001));
		source.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.1,Max=0.9]"));
		source.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0,Max=.1]"));
		udpSourceApps[i] = source.Install (clientNodes.Get (i));
		udpSourceApps[i].Start (Seconds(10));
		udpSourceApps[i].Stop (Seconds (20));
	}
	
	for(uint32_t i = 0; i < nodeCount; ++i) {
		Ptr<Application>	app;
		app = (udpSourceApps[i].Get(0));
		app->TraceConnectWithoutContext ( "Tx", MakeCallback (&txPacketCounter));
	}

	// Install UDP sink apps on nodes
	ApplicationContainer udpSinkApps[nMAXNODES];
	for(uint32_t i = 0; i < nodeCount; ++i) {
		PacketSinkHelper sink ("ns3::UdpSocketFactory",
							 InetSocketAddress (interfaces.GetAddress(i), 5001));
		udpSinkApps[i] = sink.Install (clientNodes.Get (i));
		udpSinkApps[i].Start (Seconds (0.0));
		udpSinkApps[i].Stop (Seconds (20));
	}
	
	NS_LOG_INFO ("Running Simulator");
	Simulator::Stop (Seconds (20));
	Simulator::Run ();
	Simulator::Destroy ();
	
	double	rxBytes = 0;
	double	totalRxBytes = 0;
	Ptr<PacketSink> sink1;
	for( uint32_t i = 0; i < nodeCount; ++i ) {
		sink1 = DynamicCast<PacketSink> (udpSinkApps[i].Get(0));
		rxBytes = sink1->GetTotalRx ();
		totalRxBytes += rxBytes;
		if(verbose) std::cout << "node " << i << " total bytes: " << rxBytes << std::endl;
	}
	
	double	netEfficiency;
	netEfficiency = totalRxBytes/globalTxBytes;
	std::cout << "RP "<< protocol <<" N "<< nodeCount << " P "<< transPower << " I "<< intensity <<  " Efficiency " << netEfficiency << "count" << count << std::endl;
	
	
	NS_LOG_INFO ("Complete.");
	return 0;
	
}