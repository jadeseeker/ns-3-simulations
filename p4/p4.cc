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

#define MAX_CONNECTS 100

using namespace ns3;

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
	DataRate		m_dataRate; 
	BooleanValue	m_infected;
	TypeId			m_tid;

	TracedCallback<Ptr<const Packet> > m_txTrace;

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
					MakeBooleanChecker());
	/*.AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&WormApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")*/
	
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
	Address from;
	while ((packet = socket->RecvFrom (from)))
	{
		if (InetSocketAddress::IsMatchingType (from))
			{
			NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
						InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
						InetSocketAddress::ConvertFrom (from).GetPort ());
						
			if(m_packsRec++ >= 4) {
				m_infected = true;
				//WormApplication::NewPeer(m_socket);
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
}

void WormApplication::NewPeer(Ptr<Socket> socket) {
	NS_LOG_FUNCTION (this);
	std::stringstream temp;
	int randAdd[4];
	uint32_t randAdd32 = 0;
	Ptr<UniformRandomVariable> U = CreateObject<UniformRandomVariable> ();
	U->SetAttribute ("Min", DoubleValue (0.0));
	U->SetAttribute ("Max", DoubleValue (256));
	
	Ipv4Address newPeer;
	
	do {
		randAdd[0] = 10;
		randAdd[1] = 2;
		//randAdd[2] = U->GetValue();
		randAdd[2] = U->GetValue();
		randAdd[3] = 1;
		randAdd32 |= randAdd[0]<<24;
		randAdd32 |= randAdd[1]<<16;
		randAdd32 |= randAdd[2]<<8;
		randAdd32 |= randAdd[3];

		newPeer.Set(randAdd32);
	} while (InetSocketAddress (newPeer, 5001) == m_local);
	
	NS_LOG_INFO ("Trying new peer! " << randAdd32 << " at " << Simulator::Now());
	
	m_socket->Connect (InetSocketAddress (newPeer, 5001));
	
	if(m_tid == TcpSocketFactory::GetTypeId ()) {
		;
	}
	else {
		WormApplication::udpDataSend(socket, 2);
	}
	
}

void WormApplication::udpDataSend(Ptr<Socket> socket, uint32_t remaining) {
	NS_LOG_FUNCTION(this);
	
	
	if( remaining > 0 ) {
		Ptr<Packet> packet = Create<Packet> (m_sendSize);
		socket->Send (packet);
		Time onInterval = Seconds (0.05);
		Simulator::Schedule (onInterval, &WormApplication::udpDataSend, this, socket, remaining-1 );
	}
	else {
		WormApplication::NewPeer(socket);
	}
}

} // namespace ns3

int main(int argc, char *argv[]) {
	
	LogComponentEnable ("WormApplication", LOG_LEVEL_INFO);
	LogComponentEnable ("PacketSink", LOG_LEVEL_ALL);
	//LogComponentEnable ("Socket", LOG_LEVEL_ALL);
	
	Ptr<UniformRandomVariable> U = CreateObject<UniformRandomVariable> ();
	U->SetAttribute ("Stream", IntegerValue (6110));
	U->SetAttribute ("Min", DoubleValue (0.0));
	U->SetAttribute ("Max", DoubleValue (0.1));
	
	Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpTahoe"));
	
	
	uint32_t	nFlows = 1;
	uint32_t	segSize = 128;
	uint32_t	queueSize = 64000;
	uint32_t	windowSize = 2000;
	
	
	CommandLine cmd;
	cmd.AddValue ("nFlows", "Number of flows and nodes", nFlows);
	cmd.AddValue ("segSize", "segment size", segSize);
	cmd.AddValue ("queueSize", "Queue size", queueSize);
	cmd.AddValue ("windowSize", "Window size", windowSize);
	cmd.Parse (argc, argv);
	
	
	PointToPointHelper bottleneckHelper;
	bottleneckHelper.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
	bottleneckHelper.SetChannelAttribute ("Delay", StringValue ("20ms"));
	
	PointToPointHelper leftHelper;
	leftHelper.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	leftHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
	
	PointToPointHelper rightHelper;
	rightHelper.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	rightHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
	
	NS_LOG_INFO("Creating " << nFlows << " source and sinkes.");
	PointToPointDumbbellHelper dumbBell (nFlows,leftHelper, nFlows, rightHelper, bottleneckHelper);
	
	InternetStackHelper stack;
	dumbBell.InstallStack (stack);
	
	Ipv4AddressHelper ipv4L;
	ipv4L.SetBase ("10.3.1.0", "255.255.255.0");
	Ipv4AddressHelper ipv4R;
	ipv4R.SetBase ("10.2.1.0", "255.255.255.0");
	Ipv4AddressHelper ipv4B;
	ipv4B.SetBase ("10.1.1.0", "255.255.255.0");
	
	NS_LOG_INFO ("Assign IP Addresses.");
	dumbBell.AssignIpv4Addresses(ipv4L, ipv4R, ipv4B);
	
	NS_LOG_INFO ("Create Applications.");
	
	double rn[nFlows];
	ApplicationContainer sourceApps[20];
	for(uint32_t i = 0; i < 1; ++i) {
		WormHelper source ("ns3::UdpSocketFactory", InetSocketAddress (dumbBell.GetLeftIpv4Address(i), 5001));
		
		if(i == 0){ source.SetAttribute("Infected",BooleanValue(true));}
		source.SetAttribute("ConnectCount",UintegerValue(1));
		sourceApps[i] = source.Install (dumbBell.GetLeft (i));
		rn[i] = U->GetValue();
		sourceApps[i].Start (Seconds (rn[i]));
		sourceApps[i].Stop (Seconds (50));
	}
		
	
	ApplicationContainer sinkApps[20];
	for(uint32_t i = 0; i < nFlows; ++i) {
		WormHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (dumbBell.GetRightIpv4Address(i), 5001));
		sinkApps[i] = sink.Install (dumbBell.GetRight (i));
		sinkApps[i].Start (Seconds (0.0));
		sinkApps[i].Stop (Seconds (50));
	}
	
	
	//Turn on global static routing
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	
	Simulator::Stop (Seconds (50));
	Simulator::Run ();
	Simulator::Destroy ();
	
	double infectCount = 0;
	Ptr<WormApplication> wApp;
	
	
	for( uint32_t i = 0; i < nFlows; ++i ) {
		wApp = DynamicCast<WormApplication> (sinkApps[i].Get(0));
		if ( wApp->isInfected() ) {infectCount++;}
	}
	std::cout << infectCount << " nodes infected."<< std::endl;

	
	NS_LOG_INFO ("Done.");
	return 0;
}