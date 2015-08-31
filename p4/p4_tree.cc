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
#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/traced-callback.h"
#include "ns3/trace-source-accessor.h"


#define MAX_CONNECTS 100


using namespace ns3;

uint16_t	globalInfectCount = 100;
uint16_t	globalNFlows = 0;

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
	.AddAttribute ("Interval", "Interval in seconds for packets",
					DoubleValue(0.1),
					MakeDoubleAccessor(&WormApplication::m_interval),
					MakeDoubleChecker<double> ())
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
				
				if(m_packsRec++ >= 4 && !m_infected) {
					m_infected = true;
					m_infectTrace (packet);
					if (m_aP != NULL) {m_aP->UpdateNodeColor(node,0,0,250); }
					std::cout << "Node "<< m_nodeID << " infected at " << 
								Simulator::Now().GetSeconds() << "s by " << temp.str() << std::endl;

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
	
	Ipv4Address newPeer;
	
	do {
		randAdd[0] = 10;
		randAdd[1] = 1;
		//randAdd[2] = U->GetValue();
		randAdd[2] = U->GetValue();
		randAdd[3] = 2;
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
		WormApplication::udpDataSend(socket, 10);
	}
	
}

void WormApplication::udpDataSend(Ptr<Socket> socket, uint32_t remaining) {
	NS_LOG_FUNCTION(this);
	
	
	if( remaining > 0 ) {
		Ptr<Packet> packet = Create<Packet> (m_sendSize);
		socket->Send (packet);
		Time onInterval = Seconds (m_interval);
		Simulator::Schedule (onInterval, &WormApplication::udpDataSend, this, socket, remaining-1 );
	}
	else {
		WormApplication::NewPeer(socket);
	}
}

} // namespace ns3

void infectCounter ( ns3::Ptr<ns3::Packet const> a ) {
	globalInfectCount++;
	if ( globalInfectCount == globalNFlows ) {
		Simulator::Stop (Seconds(0.1));
	}
}

int main (int argc, char *argv[])
{

Time::SetResolution (Time::NS);

LogComponentEnable("WormApplication", LOG_LEVEL_INFO);

std::string queueType = "DropTail"; 
uint32_t numNodes = 125;
uint32_t winSize = 64000; 
uint32_t  pketSize = 1024;

uint32_t maxPackets = 30;


	uint32_t	scanRate = 10;
	double		interval = 0.1;
	
	
	CommandLine cmd;
	cmd.AddValue ("scanRate","Worm scan rate in nodes per second", scanRate);
	cmd.Parse (argc, argv);
	
	interval = (double)1 / (scanRate * 10);
	NS_LOG_INFO("Using worm packet interval of " << interval << "s");

std::string appDataRate = "1Mbps"; 
  std::string bottleNeckLinkBw = "8Mbps";
  std::string bottleNeckLinkDelay = "5ms";



if(queueType=="DropTail"){
Config::SetDefault ("ns3::DropTailQueue::Mode", EnumValue(DropTailQueue::QUEUE_MODE_PACKETS));
Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue(maxPackets));
}



Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue(winSize));
Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (pketSize));
Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (appDataRate));

NodeContainer central;
central.Create(1);

NodeContainer layerOne;
layerOne.Create(4);

NodeContainer layerTwo;
layerTwo.Create(20);

NodeContainer layerThree;
layerThree.Create(100);


NodeContainer cl[4];
for (int i=0;i <4;i++)
cl[i] = NodeContainer(central.Get(0),layerOne.Get(i));


NodeContainer l1e[5];
NodeContainer l2e[5];
NodeContainer l3e[5];
NodeContainer l4e[5];

for (int i=1;i <6;i++) {
l1e[i-1] = NodeContainer(layerOne.Get(0),layerTwo.Get(i-1));
l2e[i-1] = NodeContainer(layerOne.Get(1),layerTwo.Get(i+4));
l3e[i-1] = NodeContainer(layerOne.Get(2),layerTwo.Get(i+9));
l4e[i-1] = NodeContainer(layerOne.Get(3),layerTwo.Get(i+14));
}



NodeContainer l1e1f[5];
NodeContainer l1e2f[5];
NodeContainer l1e3f[5];
NodeContainer l1e4f[5];
NodeContainer l1e5f[5];

NodeContainer l2e1f[5];
NodeContainer l2e2f[5];
NodeContainer l2e3f[5];
NodeContainer l2e4f[5];
NodeContainer l2e5f[5];

NodeContainer l3e1f[5];
NodeContainer l3e2f[5];
NodeContainer l3e3f[5];
NodeContainer l3e4f[5];
NodeContainer l3e5f[5];

NodeContainer l4e1f[5];
NodeContainer l4e2f[5];
NodeContainer l4e3f[5];
NodeContainer l4e4f[5];
NodeContainer l4e5f[5];




for (int i=1;i <6;i++) {
l1e1f[i-1] = NodeContainer(layerTwo.Get(0),layerThree.Get(i-1));
l1e2f[i-1] = NodeContainer(layerTwo.Get(1),layerThree.Get(4+i));
l1e3f[i-1] = NodeContainer(layerTwo.Get(2),layerThree.Get(9+i));
l1e4f[i-1] = NodeContainer(layerTwo.Get(3),layerThree.Get(14+i));
l1e5f[i-1] = NodeContainer(layerTwo.Get(4),layerThree.Get(19+i));

l2e1f[i-1] = NodeContainer(layerTwo.Get(5),layerThree.Get(24+i));
l2e2f[i-1] = NodeContainer(layerTwo.Get(6),layerThree.Get(29+i));
l2e3f[i-1] = NodeContainer(layerTwo.Get(7),layerThree.Get(34+i));
l2e4f[i-1] = NodeContainer(layerTwo.Get(8),layerThree.Get(39+i));
l2e5f[i-1] = NodeContainer(layerTwo.Get(9),layerThree.Get(44+i));

l3e1f[i-1] = NodeContainer(layerTwo.Get(10),layerThree.Get(49+i));
l3e2f[i-1] = NodeContainer(layerTwo.Get(11),layerThree.Get(54+i));
l3e3f[i-1] = NodeContainer(layerTwo.Get(12),layerThree.Get(59+i));
l3e4f[i-1] = NodeContainer(layerTwo.Get(13),layerThree.Get(64+i));
l3e5f[i-1] = NodeContainer(layerTwo.Get(14),layerThree.Get(69+i));

l4e1f[i-1] = NodeContainer(layerTwo.Get(15),layerThree.Get(74+i));
l4e2f[i-1] = NodeContainer(layerTwo.Get(16),layerThree.Get(79+i));
l4e3f[i-1] = NodeContainer(layerTwo.Get(17),layerThree.Get(84+i));
l4e4f[i-1] = NodeContainer(layerTwo.Get(18),layerThree.Get(89+i));
l4e5f[i-1] = NodeContainer(layerTwo.Get(19),layerThree.Get(94+i));
}



std::cout<<"Done till here"<<std::endl;




PointToPointHelper link;
link.SetDeviceAttribute ("DataRate", StringValue (bottleNeckLinkBw));
link.SetChannelAttribute ("Delay", StringValue (bottleNeckLinkDelay));


NetDeviceContainer NCcl[4];
for(int i=0;i<4;i++)
NCcl[i] = link.Install(cl[i]);

NetDeviceContainer NCl1e[5];
NetDeviceContainer NCl2e[5];
NetDeviceContainer NCl3e[5];
NetDeviceContainer NCl4e[5];

for(int i=0;i<5;i++) {
NCl1e[i] = link.Install(l1e[i]);
NCl2e[i] = link.Install(l2e[i]);
NCl3e[i] = link.Install(l3e[i]);
NCl4e[i] = link.Install(l4e[i]);
}

NetDeviceContainer NCl1e1f[5];
NetDeviceContainer NCl1e2f[5];
NetDeviceContainer NCl1e3f[5];
NetDeviceContainer NCl1e4f[5];
NetDeviceContainer NCl1e5f[5];

NetDeviceContainer NCl2e1f[5];
NetDeviceContainer NCl2e2f[5];
NetDeviceContainer NCl2e3f[5];
NetDeviceContainer NCl2e4f[5];
NetDeviceContainer NCl2e5f[5];

NetDeviceContainer NCl3e1f[5];
NetDeviceContainer NCl3e2f[5];
NetDeviceContainer NCl3e3f[5];
NetDeviceContainer NCl3e4f[5];
NetDeviceContainer NCl3e5f[5];

NetDeviceContainer NCl4e1f[5];
NetDeviceContainer NCl4e2f[5];
NetDeviceContainer NCl4e3f[5];
NetDeviceContainer NCl4e4f[5];
NetDeviceContainer NCl4e5f[5];



for(int i=0; i<5;i++) {

NCl1e1f[i] = link.Install(l1e1f[i]);
NCl1e2f[i] = link.Install(l1e2f[i]);
NCl1e3f[i] = link.Install(l1e3f[i]);
NCl1e4f[i] = link.Install(l1e4f[i]);
NCl1e5f[i] = link.Install(l1e5f[i]);

NCl2e1f[i] = link.Install(l2e1f[i]);
NCl2e2f[i] = link.Install(l2e2f[i]);
NCl2e3f[i] = link.Install(l2e3f[i]);
NCl2e4f[i] = link.Install(l2e4f[i]);
NCl2e5f[i] = link.Install(l2e5f[i]);

NCl3e1f[i] = link.Install(l3e1f[i]);
NCl3e2f[i] = link.Install(l3e2f[i]);
NCl3e3f[i] = link.Install(l3e3f[i]);
NCl3e4f[i] = link.Install(l3e4f[i]);
NCl3e5f[i] = link.Install(l3e5f[i]);

NCl4e1f[i] = link.Install(l4e1f[i]);
NCl4e2f[i] = link.Install(l4e2f[i]);
NCl4e3f[i] = link.Install(l4e3f[i]);
NCl4e4f[i] = link.Install(l4e4f[i]);
NCl4e5f[i] = link.Install(l4e5f[i]);

}



////////////////
std::cout<<"Building Net Device Containers"<<std::endl;



NetDeviceContainer devsArray[124];

for(int i=0;i<124;i++)
{
if(i<4)
devsArray[i] = NCcl[i];
/////
else if(i<9)
devsArray[i] = NCl1e[i-4];

else if(i<14)
devsArray[i] = NCl2e[i-9];

else if(i<19)
devsArray[i] = NCl3e[i-14];

else if(i<24)
devsArray[i] = NCl4e[i-19];

//////


else if(i<29)
devsArray[i] = NCl1e1f[i-24];

else if(i<34)
devsArray[i] = NCl1e2f[i-29];

else if(i<39)
devsArray[i] = NCl1e3f[i-34];

else if(i<44)
devsArray[i] = NCl1e4f[i-39];

else if(i<49)
devsArray[i] = NCl1e5f[i-44];
/////

else if(i<54)
devsArray[i] = NCl2e1f[i-49];

else if(i<59)
devsArray[i] = NCl2e2f[i-54];

else if(i<64)
devsArray[i] = NCl2e3f[i-59];

else if(i<69)
devsArray[i] = NCl2e4f[i-64];

else if(i<74)
devsArray[i] = NCl2e5f[i-69];
////////////

else if(i<79)
devsArray[i] = NCl3e1f[i-74];

else if(i<84)
devsArray[i] = NCl3e2f[i-79];

else if(i<89)
devsArray[i] = NCl3e3f[i-84];

else if(i<94)
devsArray[i] = NCl3e4f[i-89];

else if(i<99)
devsArray[i] = NCl3e5f[i-94];
////////////

else if(i<104)
devsArray[i] = NCl4e1f[i-99];

else if(i<109)
devsArray[i] = NCl4e2f[i-104];

else if(i<114)
devsArray[i] = NCl4e3f[i-109];

else if(i<119)
devsArray[i] = NCl4e4f[i-114];

else if(i<124)
devsArray[i] = NCl4e5f[i-119];


}


std::cout<<"Done till here"<<std::endl;

std::vector<NetDeviceContainer> devices(devsArray, devsArray + sizeof(devsArray) / sizeof(NetDeviceContainer));

std::cout<<"Installing Stacks"<<std::endl;


InternetStackHelper stack;
stack.Install (central);
stack.Install (layerOne);
stack.Install (layerTwo);
stack.Install (layerThree);


Ipv4AddressHelper ipv4;
std::vector<Ipv4InterfaceContainer> ifaceLinks(numNodes-1);
for(uint32_t i=0; i<devices.size(); ++i) {
std::ostringstream subnet;
subnet << "10.1." << i+1 << ".0";
ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
ifaceLinks[i] = ipv4.Assign(devices[i]);

}

std::cout<<"Installing On/Off Apps"<<std::endl;


uint16_t port = 9;

OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address ());
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable"));
  ApplicationContainer clientApps;

  for (uint32_t i = 74 ; i < 124 ; ++i)
    {
      // Create an on/off app sending packets to the same leaf right side
      AddressValue remoteAddress (InetSocketAddress (ifaceLinks[i].GetAddress(1), port));
      clientHelper.SetAttribute ("Remote", remoteAddress);

	if(i<79)
      clientApps.Add (clientHelper.Install (l1e1f[i-74].Get(1)));
    
	else if(i<84)
      clientApps.Add (clientHelper.Install (l1e2f[i-79].Get(1)));

	else if(i<89)
      clientApps.Add (clientHelper.Install (l1e3f[i-84].Get(1)));

	else if(i<94)
      clientApps.Add (clientHelper.Install (l1e4f[i-89].Get(1)));

	else if(i<99)
      clientApps.Add (clientHelper.Install (l1e5f[i-94].Get(1)));

	else if(i<104)
      clientApps.Add (clientHelper.Install (l2e1f[i-99].Get(1)));

	else if(i<109)
      clientApps.Add (clientHelper.Install (l2e2f[i-104].Get(1)));

	else if(i<114)
      clientApps.Add (clientHelper.Install (l2e3f[i-109].Get(1)));

	else if(i<119)
      clientApps.Add (clientHelper.Install (l2e4f[i-114].Get(1)));

	else if(i<124)
      clientApps.Add (clientHelper.Install (l2e5f[i-119].Get(1)));

}

 clientApps.Start (Seconds (2.0));
 clientApps.Stop (Seconds (3));

 
 std::cout<<"Installing Sink Apps"<<std::endl;


ApplicationContainer sinkApps;

PacketSinkHelper sinkUdp("ns3::UdpSocketFactory",
Address(InetSocketAddress(Ipv4Address::GetAny(), port)));

for(int i=0;i<5;i++)
{

sinkApps.Add(sinkUdp.Install(l3e1f[i].Get(1)));
sinkApps.Add(sinkUdp.Install(l3e2f[i].Get(1)));
sinkApps.Add(sinkUdp.Install(l3e3f[i].Get(1)));
sinkApps.Add(sinkUdp.Install(l3e4f[i].Get(1)));
sinkApps.Add(sinkUdp.Install(l3e5f[i].Get(1)));

sinkApps.Add(sinkUdp.Install(l4e1f[i].Get(1)));
sinkApps.Add(sinkUdp.Install(l4e2f[i].Get(1)));
sinkApps.Add(sinkUdp.Install(l4e3f[i].Get(1)));
sinkApps.Add(sinkUdp.Install(l4e4f[i].Get(1)));
sinkApps.Add(sinkUdp.Install(l4e5f[i].Get(1)));
}


sinkApps.Start(Seconds(0.0));
sinkApps.Stop(Seconds(3));


AnimationInterface anim ("p4_topo.xml");
//AnimationInterface *aP = &anim;
anim.SetConstantPosition(central.Get(0),50,50);
anim.SetConstantPosition(layerOne.Get(0),45,50);
anim.SetConstantPosition(layerOne.Get(1),50,55);
anim.SetConstantPosition(layerOne.Get(2),55,50);
anim.SetConstantPosition(layerOne.Get(3),50,45);

std::cout<<"Installing Worm Apps"<<std::endl;


ApplicationContainer wormApps[1000];
for(uint32_t i = 0; i < 100; ++i) {
	WormHelper worm ("ns3::UdpSocketFactory", InetSocketAddress (ifaceLinks[i+24].GetAddress(1), 5001));
	if(i == 0){ 
		worm.SetAttribute("Infected",BooleanValue(true));
		globalInfectCount++;
	}
	worm.SetAttribute("NodeID",UintegerValue(i));
	worm.SetAttribute("Port",UintegerValue(5001));
	worm.SetAttribute("Interval",DoubleValue(interval));
	wormApps[i] = worm.Install (layerThree.Get (i));
	wormApps[i].Start (Seconds (0.0));
	wormApps[i].Stop (Seconds (3));

	Ptr<Application>	app;
	app = wormApps[i].Get(0);
	app->TraceConnectWithoutContext ( "InfectEvent", MakeCallback (&infectCounter));
	Ptr<WormApplication> wApp;
	wApp = DynamicCast<WormApplication> (wormApps[i].Get(0));
	//wApp->setAnimPoint(aP);
}

std::cout<<"Setting anim positions"<<std::endl;


for(int i=0;i<5;i++)
anim.SetConstantPosition(layerTwo.Get(i),35,55-i*2);

for(int i=5;i<10;i++)
anim.SetConstantPosition(layerTwo.Get(i),45+(i-5)*2,65);

for(int i=10;i<15;i++)
anim.SetConstantPosition(layerTwo.Get(i),65,55-(15-i)*2);

for(int i=15;i<20;i++)
anim.SetConstantPosition(layerTwo.Get(i),45+(i-15)*2,35);


for(int i=0;i<25;i++)
anim.SetConstantPosition(layerThree.Get(i),20,78-i*2);

for(int i=0;i<25;i++)
anim.SetConstantPosition(layerThree.Get(25+i),24+i*2,88);

for(int i=0;i<25;i++)
anim.SetConstantPosition(layerThree.Get(50+i),80,28+i*2);

for(int i=0;i<25;i++)
anim.SetConstantPosition(layerThree.Get(75+i),24+i*2,18);


Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

Simulator::Stop (Seconds (3));
Simulator::Run ();
Simulator::Destroy ();


//double total=0;
/*
for(ApplicationContainer::Iterator it = sinkApps.Begin(); it != sinkApps.End(); ++it) {
    Ptr<PacketSink> sink = DynamicCast<PacketSink> (*it);
    uint32_t bytesRcvd = sink->GetTotalRx ();
//    std::cout << "\nFlow " << i << ":" << std::endl;
    std::cout << "\tTotal Bytes Received: " << bytesRcvd << std::endl;
    //++i;
  }
  */
  
double infectCount = 0;
Ptr<WormApplication> wApp;


for( uint32_t i = 0; i < 100; ++i ) {
	wApp = DynamicCast<WormApplication> (wormApps[i].Get(0));
	if ( wApp->isInfected() ) {infectCount++;}
}
std::cout << infectCount << " nodes infected."<< std::endl;

//std::cout<<"Total goodput: "<<total<<std::endl;


return 0;
}
