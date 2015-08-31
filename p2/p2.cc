//
// Network topology (TCP/IP Protocol)
//                  
//	SOURCE						SINK
//               15 Mbs                       10 Mbps 
//       UDP   l4 ----                        ----- r7   UDP
//                   |                        |
//                   |                        |
//                   |                        |        
//       TCP   l5 -- c1 -------- c2 -------  c3 -- r8   TCP
//                   |   10 Mbp      1 Mbps   |
//                   |                        |
//                   |                        |      
//       TCP   l6 ---                         ---- r9    TCP



#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/uinteger.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/random-variable-stream.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/point-to-point-layout-module.h"



using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RED vs DropTail Queuing");

int main (int argc, char *argv[])
{

  Time::SetResolution (Time::NS);
  
 
  std::string queueType = "DropTail";  
  uint32_t numNodes = 9;              
  double wgt = 128;             
  double load = 0.5;   		

  uint32_t maxBytes = 9000;  

  double Wq = 1./128.;     
  double maxP = 2;         


  CommandLine cmd;

  cmd.AddValue ("queueType", "Set Queue type to DropTail or RED", queueType);
  cmd.AddValue ("winSize", "Receiver window size (Bytes)", maxBytes);
  cmd.AddValue ("load", "Load", load);
  cmd.AddValue ("Wq", "Weighting factor for average queue length", wgt);

  

  cmd.Parse(argc, argv);

  double minTh = maxBytes*0.5;    
  double maxTh = maxBytes*0.8;
   Wq=1.0/wgt;

  //Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue(winSize));
  Config::SetDefault ("ns3::TcpSocketBase::MaxWindowSize", UintegerValue(maxBytes));

  std::string qType;
  if (queueType == "RED") 
  {
    qType = "ns3::RedQueue";
    Config::SetDefault ("ns3::RedQueue::Mode", StringValue("QUEUE_MODE_BYTES"));
    Config::SetDefault ("ns3::RedQueue::QueueLimit", UintegerValue (maxBytes));
    Config::SetDefault ("ns3::RedQueue::QW", DoubleValue (Wq));
    Config::SetDefault ("ns3::RedQueue::LInterm", DoubleValue (maxP));
    Config::SetDefault ("ns3::RedQueue::MinTh", DoubleValue (minTh));
    Config::SetDefault ("ns3::RedQueue::MaxTh", DoubleValue (maxTh)); 
  }
  else if (queueType == "DropTail")
  {
    qType = "ns3::DropTailQueue";
    Config::SetDefault ("ns3::DropTailQueue::Mode", StringValue("QUEUE_MODE_BYTES"));
    Config::SetDefault ("ns3::DropTailQueue::MaxBytes", UintegerValue(maxBytes));
    
  } 
  else 
  {
    NS_ABORT_MSG ("Invalid queue type: Use --queueType=RED or --queueType=DropTail");
  }


  NodeContainer n;
  n.Create(numNodes);

// Router links
  NodeContainer c1c2 = NodeContainer(n.Get(0), n.Get(1));
  NodeContainer c2c3 = NodeContainer(n.Get(1), n.Get(2));
  
// Source links
  NodeContainer c1l4 = NodeContainer(n.Get(0), n.Get(3));
  NodeContainer c1l5 = NodeContainer(n.Get(0), n.Get(4));
  NodeContainer c1l6 = NodeContainer(n.Get(0), n.Get(5));

// Sink links
  NodeContainer c3r7 = NodeContainer(n.Get(2), n.Get(6));
  NodeContainer c3r8 = NodeContainer(n.Get(2), n.Get(7));
  NodeContainer c3r9 = NodeContainer(n.Get(2), n.Get(8));

  NodeContainer routerNodes;
  routerNodes.Add(n.Get(0));
  routerNodes.Add(n.Get(1));
  routerNodes.Add(n.Get(2));

  NodeContainer leftNodes;
  leftNodes.Add(n.Get(3));
  leftNodes.Add(n.Get(4));
  leftNodes.Add(n.Get(5));
  
  NodeContainer rightNodes;
  rightNodes.Add(n.Get(6));
  rightNodes.Add(n.Get(7));
  rightNodes.Add(n.Get(8));
  

// RouterLink 1
  PointToPointHelper routerLink1;
  routerLink1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  routerLink1.SetChannelAttribute ("Delay", StringValue ("5ms"));
  if (queueType == "RED")
    {
      routerLink1.SetQueue (qType, "LinkBandwidth", StringValue ("10Mbps"),
                "LinkDelay", StringValue ("5ms") );
                                
	}else{
		routerLink1.SetQueue (qType);
	}
  NetDeviceContainer dc1c2 = routerLink1.Install(c1c2);

// RouterLink 2
  PointToPointHelper routerLink2;
  routerLink2.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  routerLink2.SetChannelAttribute ("Delay", StringValue ("10ms"));
  
  if (queueType == "RED")
    {
      routerLink2.SetQueue (qType, "LinkBandwidth", StringValue ("1Mbps"),
                "LinkDelay", StringValue ("10ms") );
	}else{
		routerLink1.SetQueue (qType);
	}
  
  NetDeviceContainer dc2c3 = routerLink2.Install(c2c3);


  PointToPointHelper p2pLeft;
  p2pLeft.SetDeviceAttribute ("DataRate", StringValue ("15Mbps"));
  p2pLeft.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer dc1l4 = p2pLeft.Install(c1l4);
  NetDeviceContainer dc1l5 = p2pLeft.Install(c1l5);
  NetDeviceContainer dc1l6 = p2pLeft.Install(c1l6);


  PointToPointHelper p2pRight;
  p2pRight.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pRight.SetChannelAttribute ("Delay", StringValue ("1ms"));
  NetDeviceContainer dc3r7 = p2pRight.Install(c3r7);
  NetDeviceContainer dc3r8 = p2pRight.Install(c3r8);
  NetDeviceContainer dc3r9 = p2pRight.Install(c3r9);
  

  NetDeviceContainer devArr[] = {dc1c2, dc2c3, dc1l4, dc1l5, dc1l6, dc3r7, dc3r8, dc3r9};
  std::vector<NetDeviceContainer> devices (devArr, devArr + sizeof(devArr) / sizeof(NetDeviceContainer));

  InternetStackHelper stack;
  stack.Install (n);
  

  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> ifcon (numNodes-1);

  for(uint32_t i=0; i<devices.size(); ++i) {
    std::ostringstream subnet;
    subnet << "10.1." << i+1 << ".0";
    ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
    ifcon[i] = ipv4.Assign(devices[i]);
  }


// SOURCE APPS
  
  double BW = 1000000; // bps
  double numSources = 3;         
  double dutyCycle = 0.5;        
  uint64_t rate = (uint64_t)(load*BW / numSources / dutyCycle); // rate per source app
  uint16_t port = 9;
  
// UDP 1
  OnOffHelper udp1("ns3::UdpSocketFactory", Address(InetSocketAddress(ifcon[5].GetAddress(1), port)));
  udp1.SetConstantRate(DataRate(rate));
  udp1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  udp1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  
// TCP 1
  OnOffHelper tcp1("ns3::TcpSocketFactory", Address(InetSocketAddress(ifcon[6].GetAddress(1), port)));
  tcp1.SetConstantRate(DataRate(rate));
  tcp1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  tcp1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  
// TCP 2
  OnOffHelper tcp2("ns3::TcpSocketFactory", Address(InetSocketAddress(ifcon[7].GetAddress(1), port)));
  tcp2.SetConstantRate(DataRate(rate));
  tcp2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  tcp2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  
  ApplicationContainer sourceApps;
  sourceApps.Add(udp1.Install(c1l4.Get(1)));
  sourceApps.Add(tcp1.Install(c1l5.Get(1)));
  sourceApps.Add(tcp2.Install(c1l6.Get(1)));
  
  sourceApps.Start(Seconds(0.0));
  sourceApps.Stop(Seconds(10.0));


// SINK APPS
//   Udp
  PacketSinkHelper sinkUdp1("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
 
//   Tcp
  PacketSinkHelper sinkTcp1("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  PacketSinkHelper sinkTcp2("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  
  ApplicationContainer sinkApps;
  sinkApps.Add(sinkUdp1.Install(c3r7.Get(1)));
  sinkApps.Add(sinkTcp1.Install(c3r8.Get(1)));
  sinkApps.Add(sinkTcp2.Install(c3r9.Get(1)));
  
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));


  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


// RUN SIMULATION

  std::cout << "\nRuning simulation..." << std::endl;
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  std::cout << "\nSimulation finished!" << std::endl;

  std::cerr << "queueType = " << queueType << "\t"
            << "load = " << load << "\t"
            << "winSize = " << maxBytes << "\t";
  if(queueType == "RED"){
        std::cerr << "\nMinTh = " << minTh << "\t"
            	  << "\tMaxTh = " << maxTh << "\t"
           	  << "\tmaxP  = " << maxP  << "\t"
         	  << "\tWq    = " << Wq    << "\t"
          	 // << "\tqLen  = " << qlen  << "\t"
          	  << std::endl;
  }


  std::vector<uint32_t> goodputs;
  uint32_t j = 0;
  for(ApplicationContainer::Iterator i = sinkApps.Begin(); i != sinkApps.End(); ++i) {
    Ptr<PacketSink> sink = DynamicCast<PacketSink> (*i);
    uint32_t bytesRcvd = sink->GetTotalRx ();
    goodputs.push_back(bytesRcvd / 10.0);
    std::cout << "\nFlow " << j << ":";
    std::cout << "\tGoodput: " << goodputs.back() << " Bytes/seconds" << std::endl;
    ++j;
  }

  std::string dataFileName= "p2.data";
  std::ofstream dataFile;
  dataFile.open(dataFileName.c_str(), std::fstream::out | std::fstream::app);
  dataFile << queueType << "\t"
           << "load:"<<load << "\t"
           << "winSize:"<<maxBytes << "\t";
    if(queueType == "RED"){
        dataFile << "MinTh = " << minTh << "\t"
            	  << "MaxTh = " << maxTh << "\t"
           	  << "Wq    = " << Wq    << "\t"; }
  int i=1;
  for(std::vector<uint32_t>::iterator gp = goodputs.begin(); gp != goodputs.end(); ++gp) {
    dataFile << "\t GP"<<i<<":" << *gp;
    i++;
  }
  dataFile  << "\n";
  dataFile.close();

return 0;
}
