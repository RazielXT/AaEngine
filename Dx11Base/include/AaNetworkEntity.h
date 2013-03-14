#ifndef _AA_NETWORK_ENTITY_
#define _AA_NETWORK_ENTITY_

#include "AaEntity.h"
#include "AaCamera.h"

class AaNetworkEntity: public AaEntity
{
public:

	AaNetworkEntity(AaSceneManager* mSceneMgr, AaCamera** cameraPtr);
	~AaNetworkEntity();

private:

};

#endif