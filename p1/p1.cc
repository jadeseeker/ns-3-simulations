// Network topology
//
//       n0 ---+      +--- n3
//             |      |
//             n1 -- n2


#include <fstream>
#include <string>
#include<iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/tcp-tahoe.h"
#include "ns3/config.h"
#include "ns3/random-variable-stream.h"
#include "ns3/tcp-socket.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Lab2");

int main (int argc, char *argv[])
{


  RngSeedManager::SetSeed (11223344);
  Ptr<UniformRandomVariable> U = CreateObject<UniformRandomVariable> ();
  U->SetAttribute ("Min", DoubleValue (0.0));
  U->SetAttribute ("Max", DoubleValue (0.1));

  //bool tracing = false;
  uint32_t nFlows = 1;
  uint32_t segSize = 128;
  uint32_t queueSize = 32000;
  uint32_t windowSize = 64000;

//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue ("nFlows", "No of simulated flows", nFlows);
  cmd.AddValue ("segSize", "Size of each segment", segSize);
  cmd.AddValue ("queueSize","Size of buffer", queueSize);
  cmd.AddValue ("windowSize", "Size of Window", windowSize);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::DropTailQueue::Mode", EnumValue(DropTailQueue::QUEUE_MODE_BYTES));
  Config::SetDefault ("ns3::DropTailQueue::MaxBytes", UintegerValue(queueSize));
 
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(segSize));
  Config::SetDefault ("ns3::TcpSocketBase::MaxWindowSize", UintegerValue(windowSize));
  Config::SetDefault("ns3::TcpL4Protocol::SocketType",StringValue("ns3::TcpTahoe"));

  
  NS_LOG_INFO ("Create nodes.");
  
  NodeContainer c; // ALL Nodes
  int nodes=2*nFlows+2;
  c.Create(nodes);

  NodeContainer n0n1[nFlows];
  NodeContainer n3n2[nFlows];

  for(uint32_t i=0;i<nFlows;i++){
   n0n1[i]= NodeContainer (c.Get (i), c.Get (nFlows));
   n3n2[i] = NodeContainer (c.Get (nFlows+2+i), c.Get (nFlows+1));
  }

  NodeContainer n1n2 = NodeContainer (c.Get (nFlows), c.Get (nFlows+1));


  NS_LOG_INFO ("Create internet stack.");

  InternetStackHelper internet;
  internet.Install (c);


  NS_LOG_INFO ("Create channels.");
  
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));
  
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("20ms"));
  //p2p2.SetQueue("ns3::DropTailQueue","MaxBytes", UintegerValue(queueSize));
  
  NetDeviceContainer d1d2 = p2p2.Install (n1n2);
  NetDeviceContainer d0d1[nFlows];
  NetDeviceContainer d2d3[nFlows];

for(uint32_t i=0;i<nFlows;i++){
  d0d1[i] = p2p.Install (n0n1[i]);
  d2d3[i] = p2p.Install (n3n2[i]);
}


  NS_LOG_INFO ("Assign IP Addresses.");

  Ipv4AddressHelper ipv4;
  //uint32_t pp= 10.10.1.1;
  //std::cout<<pp << std::endl;
  
  Ipv4InterfaceContainer i0i1[nFlows];
  Ipv4InterfaceContainer i3i2[nFlows];


for(uint32_t i=0;i<nFlows;i++){
  
  std::ostringstream subnet;
  subnet<<"10.1."<<i+1<<".0";
  ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
  //ipv4.SetBase ("10.1.1.0", "255.255.255.0" , i+1);
  i0i1[i]= ipv4.Assign (d0d1[i]);
}
  std::ostringstream subnet;
  subnet<<"10.1."<<nFlows+1<<".0";
  ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");

  ipv4.SetBase ("10.10.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);

for(uint32_t i=0;i<nFlows;i++){
  std::ostringstream subnet;
  subnet<<"10.1."<<nFlows+2+i<<".0";
  ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
  i3i2[i] = ipv4.Assign (d2d3[i]);
}


  NS_LOG_INFO ("Enable static global routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  NS_LOG_INFO ("Create Applications.");

  //Bulk packet send at node0
 
  uint16_t port = 9;
   ApplicationContainer sourceApps[nFlows];
   ApplicationContainer sinkApps[nFlows]; 
  double rnum[nFlows]; 

for(uint32_t i=0;i<nFlows;i++){
   BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (i3i2[i].GetAddress(0), port));
   source.SetAttribute("MaxBytes", UintegerValue (0));
   source.SetAttribute("SendSize",UintegerValue(512));
   sourceApps[i] = source.Install (c.Get (i));
   rnum[i] = U->GetValue(); 
//   std::cout << rnum[i] << std::endl;
   sourceApps[i].Start (Seconds (rnum[i]));
   sourceApps[i].Stop (Seconds (10.0));
}

//Sink at node 1


for(uint32_t i=0;i<nFlows;i++){

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
   sinkApps[i] = sink.Install (c.Get (nFlows+2+i));
   sinkApps[i].Start (Seconds (0.0));
   sinkApps[i].Stop (Seconds (10.0));
}


   NS_LOG_INFO("run Simulation");

   Simulator::Stop(Seconds (10.0));
   Simulator::Run();
   Simulator::Destroy();

   NS_LOG_INFO("End");



Ptr<PacketSink> sink1[nFlows];   
for(uint32_t i=0;i<nFlows;i++){
   sink1[i] = DynamicCast<PacketSink> (sinkApps[i].Get(0));
   std::cout << "flow " << i << " windowSize " << windowSize << " queueSize " << queueSize << " segSize " << segSize << " goodput " << (sink1[i]->GetTotalRx())/(10.0-rnum[i]) << std::endl;
}
 
}
