#ifndef _AA_PHYSICS_MANAGER_
#define _AA_PHYSICS_MANAGER_

#include <PxPhysicsAPI.h> 
#include <map> 
#include <list> 
#include "AaLogger.h"
#include "AaEntity.h"

using namespace physx;


class AaPhysicsManager
{
public:

	AaPhysicsManager();
	~AaPhysicsManager();

	void startSimulating(float time);
	bool fetchResults(bool blocking);
	PxPhysics* AaPhysicsManager::getPhysics();
	PxScene* AaPhysicsManager::getScene();
	void synchronizeEntities();

	PxRigidDynamic* createSphereBodyDynamic(AaEntity* owner, float radius, PxMaterial* material = defaultMaterial);
	PxRigidDynamic* createCapsuleBodyDynamic(AaEntity* owner, float radius, float halfHeight, PxMaterial* material = defaultMaterial);
	PxRigidDynamic* createBoxBodyDynamic(AaEntity* owner, float x, float y, float z, PxMaterial* material = defaultMaterial);
	PxRigidStatic* createPlane(float height, PxMaterial* material = defaultMaterial);

	PxRigidDynamic* createConvexBodyDynamic(AaEntity* owner, PxMaterial* material = defaultMaterial);
	PxRigidStatic* createConvexBodyStatic(AaEntity* owner, PxMaterial* material = defaultMaterial);
	PxRigidStatic* createTreeBodyStatic(AaEntity* owner, PxMaterial* material = defaultMaterial);

private:

	PxDefaultErrorCallback gDefaultErrorCallback;
	PxDefaultAllocator gDefaultAllocatorCallback;
	PxFoundation* mFoundation;
	PxProfileZoneManager* mProfileZoneManager;
	PxPhysics* mPhysics;
	PxScene* mScene;
	PxCooking* mCooking;
	float mAccumulator;
	float mStepSize;
	static PxMaterial* defaultMaterial;
	
};

#endif