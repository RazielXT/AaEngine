#include "AaNetworkSystem.h"
#include "time.h"
#include "RakSleep.h"
#include "Gets.h"
#include <iostream>
#include <string>

#define DEFAULT_SERVER_PORT "61111"
#define DEFAULT_SERVER_ADDRESS "94.198.81.195"


RakNet::SystemAddress AaNetworkSystem::ConnectBlocking(const char *hostName, const char *defaultAddress, unsigned short port)
{
	char ipAddr[64];
	strcpy_s(ipAddr, defaultAddress);
	
	
	if (peer->Connect(ipAddr, port, 0, 0)!=RakNet::CONNECTION_ATTEMPT_STARTED)
	{
		printf("Failed connect call for %s.\n", hostName);
		return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
	}

	RakNet::Packet *packet;
	while (1)
	{
		for (packet=peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
		{
			if (packet->data[0]==ID_CONNECTION_REQUEST_ACCEPTED)
			{
				printf("Successfuly connected to %s.\n", hostName);
				Rocket::Core::String innRml;
				ele->GetInnerRML(innRml);
				ele->SetInnerRML(innRml+"Successfuly connected to NAT!");
				return packet->systemAddress;
			}
			else
			{
				return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
			}
			RakSleep(50);
		}
	}
}


AaNetworkSystem::AaNetworkSystem(Rocket::Core::Element* el) : asyncCoordSem(1), asyncLoopCoordSem(1)
{
	ele = el;
	peer = RakNet::RakPeerInterface::GetInstance();
	RakNet::SocketDescriptor sd(0,0);
	//all players, -1 me, +1 nat server
	peer->Startup(MAX_PLAYERS,&sd, 1);
	//all other players
	peer->SetMaximumIncomingConnections(MAX_PLAYERS-1);
	
	printf("Our client GUID is : %s\n",peer->GetMyGUID().ToString());
	std::string server;

	lastFailMessage = 0;
	hostGuid = RakNet::UNASSIGNED_RAKNET_GUID;
	natServerAddress = RakNet::UNASSIGNED_SYSTEM_ADDRESS;
	state = NetworkState_Idle;

	workerThread = boost::thread(boost::bind(&AaNetworkSystem::asyncWorkerThread, this));
}

AaNetworkSystem::~AaNetworkSystem()
{
}

void AaNetworkSystem::transferHost()
{
	state = NetworkState_Idle;
	disconnectNatPtServer();
}

bool AaNetworkSystem::connectNatPtServer()
{
	if(natServerAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
	{
		natServerAddress = ConnectBlocking("NAT punchthrough server",DEFAULT_SERVER_ADDRESS ,atoi(DEFAULT_SERVER_PORT));
		
		if(natServerAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
			return false;

		npClient = new RakNet::NatPunchthroughClient;
		peer->AttachPlugin(npClient);
	}

	return true;
}

void AaNetworkSystem::disconnectNatPtServer()
{
	if(natServerAddress != RakNet::UNASSIGNED_SYSTEM_ADDRESS)
	{
		peer->DetachPlugin(npClient);
		peer->CloseConnection(natServerAddress,true);
		natServerAddress = RakNet::UNASSIGNED_SYSTEM_ADDRESS;
		delete npClient;
	}
}

void AaNetworkSystem::connectToHost(RakNet::RakNetGUID guid)
{
	if(state == NetworkState_Idle)
	{
		connectNatPtServer();
		state = NetworkState_ConnectingToHost;
		hostGuid = guid;
		npClient->OpenNAT(guid, natServerAddress);
	}
	else
	{
		printf("Already connected!\n");
	}
}

void AaNetworkSystem::disconnect()
{
	if(hostGuid != RakNet::UNASSIGNED_RAKNET_GUID)
	if(state == NetworkState_ConnectedClient || state == NetworkState_TrasferringHost_Bystander)
	{
		peer->CloseConnection(hostGuid,true);
		state = NetworkState_Idle;
	}

	if(state == NetworkState_ConnectedHost)
	{
		transferHost();
	}
}

void AaNetworkSystem::update()
{
	//peer
	//NetworkState_ConnectedClient || NetworkState_ConnectingToHost || NetworkState_TrasferringHost_Target || NetworkState_TrasferringHost_Bystander
	if(state & 85)
	{
		if(state == NetworkState_ConnectedClient)
			updateConnectedClient();
		else
		if(state == NetworkState_ConnectingToHost)
			updateConnectingClient();
		else
		if(state == NetworkState_TrasferringHost_Target)
			updateTransferringClient();
		else
			updateTransferBystanderClient();
	}
	else
	//host
	//NetworkState_StartingHost || NetworkState_ConnectedHost || NetworkState_TrasferringHost_Source
	if(state & 42)
	{
		if(state == NetworkState_ConnectedHost)
			updateConnectedHost();
		else
			updateTransferringHost();
	}
	else
	//idle
	{
		RakNet::Packet *packet;

		for (packet=peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
		{
		}
	}
}

void AaNetworkSystem::updateConnectingClient()
{
	RakNet::Packet *packet;

	for (packet=peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
	{
		
		if (
				packet->data[0]==ID_NAT_TARGET_NOT_CONNECTED ||
				packet->data[0]==ID_NAT_TARGET_UNRESPONSIVE ||
				packet->data[0]==ID_NAT_CONNECTION_TO_TARGET_LOST ||
				packet->data[0]==ID_NAT_PUNCHTHROUGH_FAILED
				)
			{

				switch (packet->data[0])
				{
				case ID_NAT_TARGET_NOT_CONNECTED:
					printf("Client: Failed: ID_NAT_TARGET_NOT_CONNECTED\n");
					break;
				case ID_NAT_TARGET_UNRESPONSIVE:
					printf("Client: Failed: ID_NAT_TARGET_UNRESPONSIVE\n");
					break;
				case ID_NAT_CONNECTION_TO_TARGET_LOST:
					printf("Client: Failed: ID_NAT_CONNECTION_TO_TARGET_LOST\n");
					break;
				case ID_NAT_PUNCHTHROUGH_FAILED:
					printf("Client: Failed: ID_NAT_PUNCHTHROUGH_FAILED\n");
					break;
				}

				lastFailMessage = packet->data[0];
				state = NetworkState_Idle;
				disconnectNatPtServer();

			}
			else if (packet->data[0]==ID_NAT_PUNCHTHROUGH_SUCCEEDED)
			{
				unsigned char weAreTheSender = packet->data[1];
				if (weAreTheSender)
				{
					printf("NAT punch success to remote system %s.\n", packet->systemAddress.ToString(true));
					peer->Connect(packet->systemAddress.ToString(false), packet->systemAddress.GetPort(),0,0);
					disconnectNatPtServer();
				}
				else
					printf("NAT punch success from remote system %s.\n", packet->systemAddress.ToString(true));
			}
			else
			{
				switch (packet->data[0])
				{
				case ID_CONNECTION_REQUEST_ACCEPTED:
					printf("Our connection request has been accepted.\n");
					state = NetworkState_ConnectedClient;
					break;
				case ID_NO_FREE_INCOMING_CONNECTIONS:
					printf("The server is full.\n");
					state = NetworkState_Idle;
					lastFailMessage = packet->data[0];
					break;
				case ID_DISCONNECTION_NOTIFICATION:
					printf("We have been disconnected.\n");
					state = NetworkState_Idle;
					lastFailMessage = packet->data[0];
					break;
				case ID_CONNECTION_LOST:
					printf("Connection lost.\n");
					state = NetworkState_Idle;
					lastFailMessage = packet->data[0];
					break;
				default:
					printf("Message with identifier %i has arrived.\n", packet->data[0]);
					break;
				}
			}

		
	}
}

void AaNetworkSystem::updateConnectedClient()
{
	RakNet::Packet *packet;

	for (packet=peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
	{
		printf("Message with identifier %i has arrived.\n", packet->data[0]);
	}
}

void AaNetworkSystem::updateTransferringClient()
{
	RakNet::Packet *packet;

	for (packet=peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
	{
		printf("Message with identifier %i has arrived.\n", packet->data[0]);
	}
}

void AaNetworkSystem::updateTransferBystanderClient()
{
	RakNet::Packet *packet;

	for (packet=peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
	{
		printf("Message with identifier %i has arrived.\n", packet->data[0]);
	}
}

void AaNetworkSystem::updateConnectedHost()
{
	RakNet::Packet *packet;

	for (packet=peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
	{
		switch (packet->data[0])
			{
			case ID_REMOTE_DISCONNECTION_NOTIFICATION:
				printf("Another client has disconnected.\n");
				break;
			case ID_REMOTE_CONNECTION_LOST:
				printf("Another client has lost the connection.\n");
				break;
			case ID_REMOTE_NEW_INCOMING_CONNECTION:
				printf("Another client has connected.\n");
				break;
			case ID_NEW_INCOMING_CONNECTION:
				printf("A connection is incoming.\n");
				break;
			case ID_DISCONNECTION_NOTIFICATION:
				printf("A client has disconnected.\n");
				break;
			case ID_CONNECTION_LOST:
				printf("A client lost the connection.\n");
				break;
			
			default:
				printf("Message with identifier %i has arrived.\n", packet->data[0]);
				break;
			}
	}
}

void AaNetworkSystem::updateTransferringHost()
{
	RakNet::Packet *packet;

	for (packet=peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
	{
		printf("Message with identifier %i has arrived.\n", packet->data[0]);
	}
}

void AaNetworkSystem::createHost()
{
	if(state != NetworkState_Idle)
	{
		disconnect();
	}

	connectNatPtServer();
	state = NetworkState_ConnectedHost;
}

void AaNetworkSystem::asyncUpdate()
{
	asyncCond.notify_one();
}

void AaNetworkSystem::fetchAsynchResults()
{
	asyncCoordSem.wait();
}

void AaNetworkSystem::asyncWorkerThread()
{
	boost::unique_lock<boost::mutex> lock(asyncMutex);

	while(1)
	{	
        asyncCond.wait(lock);
		update();
		asyncCoordSem.post();
	}
}

void AaNetworkSystem::startUpdating()
{
	workerLoopThread = boost::thread(boost::bind(&AaNetworkSystem::asyncWorkerLoopThread, this));
}

void AaNetworkSystem::stopUpdating()
{
	asyncLoopCoordSem.wait();
	workerLoopThread.join();
}

void AaNetworkSystem::asyncWorkerLoopThread()
{
	while(asyncLoopCoordSem.try_wait())
	{	
		update();
		asyncLoopCoordSem.post();
		Sleep(20);
	}
}