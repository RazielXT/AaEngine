#ifndef _AA_NETWORK_SYS_
#define _AA_NETWORK_SYS_

#include <stdio.h>
#include <map>
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakNetTypes.h"  // MessageID
#include "NatPunchthroughClient.h"
#include <boost/thread.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <Rocket/Core.h>

#define MAX_PLAYERS 8

enum NetworkState
{
	NetworkState_Idle = 0,
	NetworkState_ConnectingToHost = 1,
	NetworkState_StartingHost = 2,
	NetworkState_ConnectedClient = 4,
	NetworkState_ConnectedHost = 8,
	NetworkState_TrasferringHost_Target = 16,
	NetworkState_TrasferringHost_Source = 32,
	NetworkState_TrasferringHost_Bystander = 64
};

class AaNetworkSystem
{
public:

	AaNetworkSystem(Rocket::Core::Element* el);
	~AaNetworkSystem();
	
	void connectToHost(RakNet::RakNetGUID guid);
	void disconnect();

	bool connectNatPtServer();
	void disconnectNatPtServer();

	void update();

	void createHost();
	void becomeHost();
	void transferHost();

	void asyncUpdate();
	void fetchAsynchResults();

	void startUpdating();
	void stopUpdating();

	RakNet::SystemAddress ConnectBlocking(const char *hostName, const char *defaultAddress, unsigned short port);

	NetworkState getState() {return state;}
	unsigned char getLastFailMessage() {return lastFailMessage;}

private:
	Rocket::Core::Element* ele;
	void asyncWorkerThread();
	void asyncWorkerLoopThread();

	void updateConnectingClient();
	void updateConnectedClient();
	void updateTransferringClient();
	void updateTransferBystanderClient();

	void updateConnectedHost();
	void updateTransferringHost();

	boost::thread workerThread;
	boost::thread workerLoopThread;
	boost::condition_variable asyncCond;
	boost::mutex asyncMutex;
	boost::interprocess::interprocess_semaphore asyncCoordSem;
	boost::interprocess::interprocess_semaphore asyncLoopCoordSem;

	NetworkState state;
	unsigned char lastFailMessage;
	RakNet::RakNetGUID hostGuid;
	RakNet::SystemAddress natServerAddress;
	RakNet::RakPeerInterface *peer;
	RakNet::NatPunchthroughClient* npClient;
	std::map<RakNet::RakNetGUID,std::string> connectedPeers;
};

#endif