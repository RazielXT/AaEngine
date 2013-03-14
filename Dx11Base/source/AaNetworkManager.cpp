#include "AaNetworkManager.h"
//#include "BitStream.h"

AaNetworkManager::AaNetworkManager(AaSceneManager* mSceneMgr)
{
}

AaNetworkManager::~AaNetworkManager()
{
}

void AaNetworkManager::playersUpdate(unsigned char* data, unsigned int length)
{
/*	RakNet::BitStream bsIn(data,length,false);
	bsIn.IgnoreBytes(2);
	char playerID = data[1];
	
	PlayerSyncInfo syncInfo;
	bsIn.Read((char*)&syncInfo,sizeof(PlayerSyncInfo));
	playerUpdates[playerID] = syncInfo;*/
}