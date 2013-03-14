#ifndef _AA_NETWORK_MGR_
#define _AA_NETWORK_MGR_

#include "AaSceneManager.h"
#include "AaNetworkEntity.h"

struct PlayerSyncInfo
{
	float x,y,z;
	float velX,velY,velZ;
	float orX,orY,orZ,orW;
};

class AaNetworkManager
{
public:

	AaNetworkManager(AaSceneManager* mSceneMgr);
	~AaNetworkManager();

	//void gamePacket();
	
	void playersUpdate(unsigned char* data, unsigned int length);

private:

	std::map<char,PlayerSyncInfo> playerUpdates;
	AaSceneManager* mSceneMgr;
};

#endif