#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/traced-callback.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/netanim-module.h"

#define MAX_CONNECTS 100

using namespace ns3;

uint16_t	globalInfectCount = 0;
uint16_t	globalnNodes = 0;

namespace ns3 {

class Address;
class Socket;



class WormApplication : public Application
{
public:
	static TypeId GetTypeId (void);
	
	WormApplication ();
	
	virtual ~WormApplication ();
	
	bool	isInfected();
	
	void	setAnimPoint(AnimationInterface *p);
		

protected:
	virtual void DoDispose (void);
private:
	// inherited from Application base class.
	virtual void StartApplication (void);    // Called at time specified by Start
	virtual void StopApplication (void);     // Called at time specified by Stop
	
	//void SendData ();

	Ptr<Socket>		m_socket;
	Ptr<Socket>		m_sockets[MAX_CONNECTS];
	Address			m_local;
	Address			m_peer;
	uint16_t		m_port;
	uint32_t		m_sendSize;
	uint32_t		m_cCount;
	uint16_t		m_packsRec;
	uint16_t		m_nodeID;
	uint16_t		m_payload;
	double			m_interval;
	DataRate		m_dataRate; 
	BooleanValue	m_infected;
	TypeId			m_tid;
	AnimationInterface *m_aP;

	TracedCallback<Ptr<const Packet> > m_infectTrace;

private:
	void ConnectionSucceeded (Ptr<Socket> socket);
	void NewPeer (Ptr<Socket> socket);
	void HandleReceive (Ptr<Socket> socket);
	void udpDataSend (Ptr<Socket> socket, uint32_t remaining);
	void buildSendSockets ( void );
};

class WormHelper
{
	public:
	WormHelper (std::string protocol, Address address);
	
	void SetAttribute (std::string name, const AttributeValue &value);
	
	ApplicationContainer Install (NodeContainer c) const;
	
	ApplicationContainer Install (Ptr<Node> node) const;
	
	ApplicationContainer Install (std::string nodeName) const;
	
	private:
	Ptr<Application> InstallPriv (Ptr<Node> node) const;
	
	ObjectFactory m_factory; 
};
NS_LOG_COMPONENT_DEFINE ("WormApplication");

NS_OBJECT_ENSURE_REGISTERED (WormApplication);

WormHelper::WormHelper (std::string protocol, Address address)
{
  m_factory.SetTypeId ("ns3::WormApplication");
  m_factory.Set ("Protocol", StringValue (protocol));
  m_factory.Set ("Local", AddressValue (address));
}

void
WormHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
WormHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
WormHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
WormHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
WormHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<Application> ();
  node->AddApplication (app);

  return app;
}

TypeId
WormApplication::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::WormApplication")
	.SetParent<Application> ()
	.AddConstructor<WormApplication> ()
	.AddAttribute ("SendSize", "The amount of data to send each time.",
					UintegerValue (32),
					MakeUintegerAccessor (&WormApplication::m_sendSize),
					MakeUintegerChecker<uint32_t> (1))
	.AddAttribute ("DataRate", "The data rate in on state.",
					DataRateValue (DataRate ("500kb/s")),
					MakeDataRateAccessor (&WormApplication::m_dataRate),
					MakeDataRateChecker ())
	.AddAttribute ("Local",
					"The Address on which to Bind the rx socket.",
					AddressValue (),
					MakeAddressAccessor (&WormApplication::m_local),
					MakeAddressChecker ())
	.AddAttribute ("Protocol", "The type of protocol to use.",
					TypeIdValue (UdpSocketFactory::GetTypeId ()),
					MakeTypeIdAccessor (&WormApplication::m_tid),
					MakeTypeIdChecker ())
	.AddAttribute ("Port", "Port on which we listen for incoming packets.",
					UintegerValue(1000),
					MakeUintegerAccessor (&WormApplication::m_port),
					MakeUintegerChecker<uint16_t> ())
	.AddAttribute ("ConnectCount", "Number of simultaneous send connects.",
					UintegerValue(1),
					MakeUintegerAccessor (&WormApplication::m_cCount),
					MakeUintegerChecker<uint16_t> ())
	.AddAttribute ("Infected", "State of infection.",
					BooleanValue(false),
					MakeBooleanAccessor (&WormApplication::m_infected),
					MakeBooleanChecker())
	.AddAttribute ("NodeID", "ID of node.",
					UintegerValue(1),
					MakeUintegerAccessor (&WormApplication::m_nodeID),
					MakeUintegerChecker<uint16_t> ())	
	.AddAttribute ("Interval", "Interval in seconds for peers choosing",
					DoubleValue(1),
					MakeDoubleAccessor(&WormApplication::m_interval),
					MakeDoubleChecker<double> ())
	.AddAttribute ("Payload", "Size of payload to send per interval in packets",
					UintegerValue(10),
					MakeUintegerAccessor (&WormApplication::m_payload),
					MakeUintegerChecker<uint16_t> ())
	.AddTraceSource ("InfectEvent", "The application is infected",
                     MakeTraceSourceAccessor (&WormApplication::m_infectTrace))
	
	;
  return tid;
}

WormApplication::WormApplication ()
  : m_socket (0),
	m_infected (false)
{
  NS_LOG_FUNCTION (this);
  m_packsRec = 0;
  m_cCount = 4;
  m_aP = NULL;
  for(uint32_t i = 0; i < MAX_CONNECTS; i++) {
	  m_sockets[i] = NULL;
  }
}

WormApplication::~WormApplication ()
{
  NS_LOG_FUNCTION (this);
}

void WormApplication::DoDispose (void)
{
	NS_LOG_FUNCTION (this);
	m_socket = 0;
	// chain up
	Application::DoDispose ();
}

bool WormApplication::isInfected()
{
	return m_infected;
}

void WormApplication::setAnimPoint(AnimationInterface *p) {
	m_aP = p;
}

void WormApplication::StartApplication ()    // Called at time specified by Start
{
	NS_LOG_FUNCTION (this);
	// Create the socket if not already
	if (!m_socket)
	{
		m_socket = Socket::CreateSocket (GetNode (), m_tid);
		m_socket->Bind (m_local);
		m_socket->Listen ();
		
		if(m_tid == TcpSocketFactory::GetTypeId ()) {
			m_socket->SetConnectCallback (
				MakeCallback (&WormApplication::ConnectionSucceeded, this),
				MakeCallback (&WormApplication::NewPeer, this));
			m_socket->SetCloseCallbacks (
				MakeCallback (&WormApplication::NewPeer, this),
				MakeCallback (&WormApplication::NewPeer, this));
		}
				
	}
	
	m_socket->SetRecvCallback (MakeCallback (&WormApplication::HandleReceive, this));
	if(m_infected == true ) {
		//WormApplication::buildSendSockets();
		WormApplication::NewPeer(m_socket);
	}
}

void WormApplication::buildSendSockets (void) {
	NS_LOG_FUNCTION(this);
	for(uint32_t i = 0; i < m_cCount; i++) {
		m_sockets[i] = Socket::CreateSocket (GetNode (), m_tid);
		m_sockets[i]->Bind(m_local);
		m_sockets[i]->Listen();
		
		if(m_tid == TcpSocketFactory::GetTypeId ()) {
			m_sockets[i]->SetConnectCallback (
				MakeCallback (&WormApplication::ConnectionSucceeded, this),
				MakeCallback (&WormApplication::NewPeer, this));
			m_sockets[i]->SetCloseCallbacks (
				MakeCallback (&WormApplication::NewPeer, this),
				MakeCallback (&WormApplication::NewPeer, this));
		}
		WormApplication::NewPeer(m_sockets[i]);
	}
}

void WormApplication::HandleReceive (Ptr<Socket> socket)
{
	NS_LOG_FUNCTION (this << socket);
	
	Ptr<Packet> packet;
	Ptr<Node> node;
	node = socket->GetNode();
	Address from;
	while ((packet = socket->RecvFrom (from)))
	{
		if (InetSocketAddress::IsMatchingType (from))
			{			
				std::stringstream temp;
				temp << InetSocketAddress::ConvertFrom (from).GetIpv4 ();
				
				uint16_t calcPayload = 1;
				if (m_payload > 1) calcPayload = m_payload/2;
				if(m_packsRec++ >= (calcPayload) && !m_infected) {
					m_infected = true;
					m_infectTrace (packet);
					if (m_aP != NULL) {m_aP->UpdateNodeColor(node,0,0,250); }
					/*std::cout << "Node "<< m_nodeID << " infected at " << 
								Simulator::Now().GetSeconds() << "s by " << temp.str() << std::endl;*/
					std::cout << Simulator::Now().GetSeconds() <<std::endl;

					WormApplication::NewPeer(m_socket);
				}
			
			}
		
		packet->RemoveAllPacketTags ();
		packet->RemoveAllByteTags ();
	}
}

void WormApplication::StopApplication(void) {
	NS_LOG_FUNCTION (this);
	if (m_socket != 0)
	{
		m_socket->Close ();
	}
}

void WormApplication::ConnectionSucceeded(Ptr<Socket> socket) {
	NS_LOG_FUNCTION (this);
	NS_LOG_INFO ("Found a sink!");
	WormApplication::udpDataSend(socket, 10);
}

void WormApplication::NewPeer(Ptr<Socket> socket) {
	NS_LOG_FUNCTION (this << socket);
	std::stringstream temp;
	int randAdd[4];
	uint32_t randAdd32 = 0;
	Ptr<UniformRandomVariable> U = CreateObject<UniformRandomVariable> ();
	U->SetAttribute ("Min", DoubleValue (0.0));
	U->SetAttribute ("Max", DoubleValue (256));
	Ptr<UniformRandomVariable> U2 = CreateObject<UniformRandomVariable> ();
	U2->SetAttribute ("Min", DoubleValue (0));
	U2->SetAttribute ("Max", DoubleValue (4));
	
	Ipv4Address newPeer;
	
	do {
		randAdd[0] = 10;
		randAdd[1] = 2;
		randAdd[2] = U->GetValue();
		randAdd[3] = 1;
		randAdd32 |= randAdd[0]<<24;
		randAdd32 |= randAdd[1]<<16;
		randAdd32 |= randAdd[2]<<8;
		randAdd32 |= randAdd[3];

		newPeer.Set(randAdd32);
	} while (InetSocketAddress (newPeer, m_port) == m_local);
	
	//NS_LOG_INFO ("Trying new peer! " << randAdd32 << " at " << Simulator::Now() << " from " << m_local);
	
	m_socket->Connect (InetSocketAddress (newPeer, m_port));
	
	if(m_tid == TcpSocketFactory::GetTypeId ()) {
		;
	}
	else {
		WormApplication::udpDataSend(socket, m_payload);
	}
	
}

void WormApplication::udpDataSend(Ptr<Socket> socket, uint32_t remaining) {
	NS_LOG_FUNCTION(this);
	
	//NS_LOG_INFO (this << " Sending packet at " << Simulator::Now());
	if( remaining > 0 ) {
		Ptr<Packet> packet = Create<Packet> (m_sendSize);
		socket->Send (packet);
		Time onInterval = Seconds (m_interval/m_payload);
		Simulator::Schedule (onInterval, &WormApplication::udpDataSend, this, socket, remaining-1 );
	}
	else {
		WormApplication::NewPeer(socket);
	}
}

} // namespace ns3

void infectCounter ( ns3::Ptr<ns3::Packet const> a ) {
	globalInfectCount++;
	if ( globalInfectCount == globalnNodes ) {
		Simulator::Stop (Seconds(0.1));
	}
}


double	globalTxBytes = 0;

void txPacketCounter ( ns3::Ptr<ns3::Packet const> a ) {
	globalTxBytes += 128;
}

int main(int argc, char *argv[]) {
	
	LogComponentEnable ("WormApplication", LOG_LEVEL_INFO);
	//LogComponentEnable ("PacketSink", LOG_LEVEL_ALL);

	
	//LogComponentEnable ("Ipv4AddressHelper", LOG_LEVEL_ALL);
	//LogComponentEnable ("Socket", LOG_LEVEL_ALL);
	
	Ptr<UniformRandomVariable> U = CreateObject<UniformRandomVariable> ();
	U->SetAttribute ("Stream", IntegerValue (6110));
	U->SetAttribute ("Min", DoubleValue (0.0));
	U->SetAttribute ("Max", DoubleValue (0.1));
	
	Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpTahoe"));
	
	
	uint32_t	nNodes = 50;
	uint32_t	scanRate = 10;
	uint32_t	payload = 10;
	double		interval = 1;
	double		endTime = 20.0;
	
	
	CommandLine cmd;
	cmd.AddValue ("nNodes", "Number of flows and nodes", nNodes);
	cmd.AddValue ("scanRate","Worm scan rate in nodes per second", scanRate);
	cmd.AddValue ("Payload","Worm payload packet count per scan", payload);
	cmd.Parse (argc, argv);
	
	interval = (double)1 / ( scanRate );
	NS_LOG_INFO("Using worm peer interval of " << interval << "s");
	
	if( nNodes > 200 ) nNodes = 200;
	globalnNodes = nNodes;
	
	if( payload > 80000 ) payload = 80000;
	if( payload < 1 ) payload =1;
	
	
	PointToPointHelper bottleneckHelper;
	bottleneckHelper.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
	bottleneckHelper.SetChannelAttribute ("Delay", StringValue ("20ms"));
	
	PointToPointHelper leftHelper;
	leftHelper.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	leftHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
	
	PointToPointHelper rightHelper;
	rightHelper.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	rightHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
	
	NS_LOG_INFO("Creating " << nNodes << " sources and sinks.");
	PointToPointDumbbellHelper dumbBell (nNodes,leftHelper, nNodes, rightHelper, bottleneckHelper);
		
	dumbBell.BoundingBox(1,1,100,100);
	
	// Create the animation object and configure for specified output
	AnimationInterface anim ("p4anim.xml");
	AnimationInterface *aP = &anim;
	aP->EnablePacketMetadata (); // Optional
	
	InternetStackHelper stack;
	dumbBell.InstallStack (stack);
	
	Ipv4AddressHelper ipv4L;
	ipv4L.SetBase ("10.3.1.0", "255.255.255.0");
	Ipv4AddressHelper ipv4B;
	ipv4B.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4AddressHelper ipv4R;
	ipv4R.SetBase ("10.2.1.0", "255.255.255.0");
	
	NS_LOG_INFO ("Assign IP Addresses.");
	dumbBell.AssignIpv4Addresses(ipv4L, ipv4R, ipv4B);
	
	NS_LOG_INFO ("Create Applications.");
	
		
	
	ApplicationContainer wormLApps[1000];
	for(uint32_t i = 0; i < 1; ++i) {
		WormHelper worm ("ns3::UdpSocketFactory", InetSocketAddress (dumbBell.GetLeftIpv4Address(i), 5001));
		if(i == 0){ 
			worm.SetAttribute("Infected",BooleanValue(true));
		}
		worm.SetAttribute("NodeID",UintegerValue(i));
		worm.SetAttribute("Port",UintegerValue(5001));
		worm.SetAttribute("Interval",DoubleValue(interval));
		worm.SetAttribute("Payload",UintegerValue(payload));
		wormLApps[i] = worm.Install (dumbBell.GetLeft (i));
		wormLApps[i].Start (Seconds (0.0));
		wormLApps[i].Stop (Seconds (endTime));
		
		Ptr<Application>	app;
		app = wormLApps[i].Get(0);
		app->TraceConnectWithoutContext ( "InfectEvent", MakeCallback (&infectCounter));
		Ptr<WormApplication> wApp;
		wApp = DynamicCast<WormApplication> (wormLApps[i].Get(0));
		wApp->setAnimPoint(aP);
	}
	
	ApplicationContainer wormRApps[1000];
	for(uint32_t i = 0; i < nNodes; ++i) {
		WormHelper worm ("ns3::UdpSocketFactory", InetSocketAddress (dumbBell.GetRightIpv4Address(i), 5001));
		worm.SetAttribute("NodeID",UintegerValue(i+nNodes));
		worm.SetAttribute("Port",UintegerValue(5001));
		worm.SetAttribute("Interval",DoubleValue(interval));
		wormRApps[i] = worm.Install (dumbBell.GetRight (i));
		wormRApps[i].Start (Seconds (0.0));
		wormRApps[i].Stop (Seconds (endTime));
		
		Ptr<Application>	app;
		app = wormRApps[i].Get(0);
		app->TraceConnectWithoutContext ( "InfectEvent", MakeCallback (&infectCounter));
		Ptr<WormApplication> wApp;
		wApp = DynamicCast<WormApplication> (wormRApps[i].Get(0));
		wApp->setAnimPoint(aP);
	}

#if 0 //Background Traffic	
	double rn[nNodes];
	ApplicationContainer sourceApps[200];
		for(uint32_t i = 0; i < nNodes; ++i) {
		OnOffHelper source ("ns3::TcpSocketFactory",
								 InetSocketAddress (dumbBell.GetRightIpv4Address(i), 1000));
		source.SetAttribute ("DataRate", StringValue ("20kbps"));
		source.SetAttribute ("PacketSize", UintegerValue (128));
		source.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.,Max=1.]"));
		source.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.,Max=1.]"));
		sourceApps[i] = source.Install (dumbBell.GetLeft (i));
		rn[i] = U->GetValue();
		sourceApps[i].Start (Seconds (rn[i]));
		sourceApps[i].Stop (Seconds (endTime));
	}
	
	for(uint32_t i = 0; i < nNodes; ++i) {
		Ptr<Application>	app;
		app = (sourceApps[i].Get(0));
		app->TraceConnectWithoutContext ( "Tx", MakeCallback (&txPacketCounter));
	}

  ApplicationContainer sinkApps[200];
  for(uint32_t i = 0; i < nNodes; ++i) {
    PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (dumbBell.GetRightIpv4Address(i), 1000));
  	sinkApps[i] = sink.Install (dumbBell.GetRight (i));
	sinkApps[i].Start (Seconds (0.0));
    sinkApps[i].Stop (Seconds (endTime));
  }
 #endif

	
		
	//Turn on global static routing
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	
	Simulator::Stop (Seconds (endTime));
	Simulator::Run ();
	Simulator::Destroy ();
	
	double infectCount = 0;
	Ptr<WormApplication> wApp;
	
	
	for( uint32_t i = 0; i < nNodes; ++i ) {
		wApp = DynamicCast<WormApplication> (wormRApps[i].Get(0));
		if ( wApp->isInfected() ) {infectCount++;}
	}
	std::cout << infectCount << " nodes infected."<< std::endl;

#if 0 //Background Traffic
	double totaRxlBytes = 0;
	double efficiency = 0;
	Ptr<PacketSink> sink1;
	
	for( uint32_t i = 0; i < nNodes; ++i ) {
		sink1 = DynamicCast<PacketSink> (sinkApps[i].Get(0));
		totaRxlBytes += sink1->GetTotalRx ();
	}
	efficiency =  totaRxlBytes/globalTxBytes;
	std::cout << "Background Traffic Efficiency " << efficiency << std::endl;
#endif
	
	
	NS_LOG_INFO ("Done.");
	return 0;
}