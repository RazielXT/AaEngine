#define SAFE_SIMULATING 1

#include "AaPhysicsManager.h"
#include "PxToolkit.h"
#include <boost/lexical_cast.hpp>
PxMaterial* AaPhysicsManager::defaultMaterial;

AaPhysicsManager::AaPhysicsManager()
{
	mAccumulator = 0.0f;
	mStepSize = 1.0f / 50.0f;

	mFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);
	if(!mFoundation)
	{
	}

	PxCookingParams cookParams;
	cookParams.skinWidth = 0.001;
	mCooking = PxCreateCooking(PX_PHYSICS_VERSION, *mFoundation, cookParams);
	if (!mCooking)
	{
	}

	mProfileZoneManager = &PxProfileZoneManager::createProfileZoneManager(mFoundation);
	if(!mProfileZoneManager)
	{
	}
	
	bool recordMemoryAllocations = true;
	mPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, PxTolerancesScale(), recordMemoryAllocations, mProfileZoneManager );
	if(!mPhysics)
	{
	}

	PxSceneDesc sceneDesc(mPhysics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	sceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVETRANSFORMS;

	int mNbThreads=2;
	if(!sceneDesc.cpuDispatcher)
	{
		sceneDesc.cpuDispatcher    = PxDefaultCpuDispatcherCreate(mNbThreads);
	}

	if(!sceneDesc.filterShader)
    sceneDesc.filterShader    = &PxDefaultSimulationFilterShader;

	mScene = mPhysics->createScene(sceneDesc);
	if (!mScene)
	{
	}

	defaultMaterial = mPhysics->createMaterial(0.5f, 0.5f, 0.5f);    //static friction, dynamic friction, restitution


#ifdef _DEBUG
	// check if PvdConnection manager is available on this platform
	if(mPhysics->getPvdConnectionManager() == NULL)
		return;

	// setup connection parameters
	const char*     pvd_host_ip = "127.0.0.1";  // IP of the PC which is running PVD
	int             port        = 5425;         // TCP port to connect to, where PVD is listening
	unsigned int    timeout     = 100;          // timeout in milliseconds to wait for PVD to respond,
												// consoles and remote PCs need a higher timeout.
	PxVisualDebuggerConnectionFlags connectionFlags = PxVisualDebuggerExt::getAllConnectionFlags();

	// and now try to connect
	PVD::PvdConnection* theConnection = PxVisualDebuggerExt::createConnection(mPhysics->getPvdConnectionManager(),
		pvd_host_ip, port, timeout, connectionFlags);

	// remember to release the connection by manual in the end
		if (theConnection)
				theConnection->release();
#endif

}

PxPhysics* AaPhysicsManager::getPhysics()
{
	return mPhysics;
}

PxScene* AaPhysicsManager::getScene()
{
	return mScene;
}

AaPhysicsManager::~AaPhysicsManager()
{
	mPhysics->release();
	mFoundation->release();
}

void AaPhysicsManager::startSimulating(float time)
{
	mAccumulator += time;

    if(mAccumulator < mStepSize)
        return;

#ifdef SAFE_SIMULATING

	//safest simulation results, in lower framerates blocking multiple simulates

	mAccumulator-=mStepSize;

	while(mAccumulator >= mStepSize)
	{
		mScene->simulate(mStepSize);
		mScene->fetchResults(true);
		mAccumulator-=mStepSize;
	}

    mScene->simulate(mStepSize);

#else

	//best paralelism, unsafe different simulation times

	float updateTime=0;

	while(mAccumulator >= mStepSize)
	{
		mAccumulator-=mStepSize;
		updateTime+=mStepSize;
	}

    mScene->simulate(updateTime);

#endif

}

bool AaPhysicsManager::fetchResults(bool blocking)
{
	return mScene->fetchResults(blocking);
}

void AaPhysicsManager::synchronizeEntities()
{
	PxU32 nbActiveTransforms = mScene->getNbActors(physx::PxActorTypeSelectionFlag::eRIGID_STATIC);
	nbActiveTransforms = mScene->getNbActors(physx::PxActorTypeSelectionFlag::eRIGID_DYNAMIC);
	PxActiveTransform* activeTransforms = mScene->getActiveTransforms(nbActiveTransforms);

	// update each render object with the new transform
	for (PxU32 i=0; i < nbActiveTransforms; ++i)
	{
			AaEntity* renderObject = static_cast<AaEntity*>(activeTransforms[i].userData);
			PxVec3 pos = activeTransforms[i].actor2World.p;
			activeTransforms[i].actor2World.rotate(PxVec3(0,180,0));
			PxQuat quat = activeTransforms[i].actor2World.q;
			//quat.rotate(PxVec3(0,180,180));
			renderObject->setPhysicsTranform(XMFLOAT3(pos.x,pos.y,pos.z),XMFLOAT4(quat.x,quat.y,quat.z,quat.w));
	}
}

PxRigidStatic* AaPhysicsManager::createPlane(float height, PxMaterial* material)
{
	PxRigidStatic* plane = PxCreatePlane(*mPhysics, PxPlane(PxVec3(0,1,0), 0), *material);
	//PxTransform tr = PxTransform(PxVec3(0,height,0));
	mScene->addActor(*plane);
	//plane->setGlobalPose(tr);
	
	return plane;
}

PxRigidDynamic* AaPhysicsManager::createSphereBodyDynamic(AaEntity* owner, float radius, PxMaterial* material)
{
	//PxRigidDynamic* aSphereActor =  PxCreateDynamic(*mPhysicsMgr->getPhysics(), PxTransform(physx::PxVec3(0,10,20)), PxSphereGeometry(2), *mMaterial, 1);
	XMFLOAT3 pos = owner->getPosition();
	PxRigidDynamic* aSphereActor = mPhysics->createRigidDynamic(PxTransform(physx::PxVec3(pos.x,pos.y,pos.z)));
	PxShape* aSphereShape = aSphereActor->createShape( PxSphereGeometry(radius), *material);
	PxRigidBodyExt::updateMassAndInertia(*aSphereActor, 1);
	aSphereActor->setAngularDamping(0.8);
	mScene->addActor(*aSphereActor);
	owner->setPhysicsBody(aSphereActor);

	return aSphereActor;
}

PxRigidDynamic* AaPhysicsManager::createCapsuleBodyDynamic(AaEntity* owner, float radius, float halfHeight, PxMaterial* material)
{
	XMFLOAT3 pos = owner->getPosition();
	PxRigidDynamic* aCapsuleActor = mPhysics->createRigidDynamic(PxTransform(physx::PxVec3(pos.x,pos.y,pos.z)));
	PxTransform relativePose(PxQuat(PxHalfPi, PxVec3(0,1,0)));
	PxShape* aCapsuleShape = aCapsuleActor->createShape(PxCapsuleGeometry(radius, halfHeight), *material, relativePose);
	PxRigidBodyExt::updateMassAndInertia(*aCapsuleActor, 1);
	aCapsuleActor->setAngularDamping(0.8);
	mScene->addActor(*aCapsuleActor);
	owner->setPhysicsBody(aCapsuleActor);

	return aCapsuleActor;
}

PxRigidDynamic* AaPhysicsManager::createBoxBodyDynamic(AaEntity* owner, float x, float y, float z, PxMaterial* material)
{
	XMFLOAT3 pos = owner->getPosition();
	PxRigidDynamic* aBoxActor = mPhysics->createRigidDynamic(PxTransform(physx::PxVec3(pos.x,pos.y,pos.z)));
	PxTransform relativePose(PxQuat(PxHalfPi, PxVec3(0,1,0)));
	PxShape* aBoxShape = aBoxActor->createShape(PxBoxGeometry(x/2, y/2, z/2), *material);
	PxRigidBodyExt::updateMassAndInertia(*aBoxActor, 1);
	aBoxActor->setAngularDamping(0.8);
	mScene->addActor(*aBoxActor);
	owner->setPhysicsBody(aBoxActor);

	return aBoxActor;
}

PxRigidDynamic* AaPhysicsManager::createConvexBodyDynamic(AaEntity* owner, PxMaterial* material)
{
	PxConvexMeshDesc convexDesc;
	convexDesc.points.count     = owner->model->rawInfo->vertex_count;
	convexDesc.points.stride    = sizeof(float)*3;
	convexDesc.points.data      = owner->model->rawInfo->vertices;
	convexDesc.flags            = PxConvexFlag::eCOMPUTE_CONVEX;
	convexDesc.flags            |= PxConvexFlag::eINFLATE_CONVEX;

	PxToolkit::MemoryOutputStream buf;
	if(!mCooking->cookConvexMesh(convexDesc, buf))
		return NULL;

	PxToolkit::MemoryInputData input(buf.getData(), buf.getSize());
	PxConvexMesh* convexMesh = mPhysics->createConvexMesh(input);

	XMFLOAT3 pos = owner->getPosition();
	XMFLOAT3 scale = owner->getScale();
	XMMATRIX rot = XMMatrixRotationQuaternion(*owner->getQuaternion());

	XMFLOAT3X3 rotMatrix;
	XMStoreFloat3x3(&rotMatrix,rot);
	physx::PxMat33* rotPhysx = (physx::PxMat33*)&rotMatrix;
	PxQuat orientation = PxQuat(*rotPhysx);
	PxTransform transf = PxTransform(physx::PxVec3(pos.x,pos.y,pos.z),orientation);
	transf.rotate(PxVec3(0,180,0));

	PxRigidDynamic* aConvexActor = mPhysics->createRigidDynamic(transf);
	PxShape* aConvexShape = aConvexActor->createShape(PxConvexMeshGeometry(convexMesh,PxMeshScale(PxVec3(scale.x,scale.y,scale.z),PxQuat::createIdentity())), *material);

	mScene->addActor(*aConvexActor);
	owner->setPhysicsBody(aConvexActor);

	return aConvexActor;
}

PxRigidStatic* AaPhysicsManager::createConvexBodyStatic(AaEntity* owner, PxMaterial* material)
{
	PxConvexMeshDesc convexDesc;
	convexDesc.points.count     = owner->model->rawInfo->vertex_count;
	convexDesc.points.stride    = sizeof(float)*3;
	convexDesc.points.data      = owner->model->rawInfo->vertices;
	convexDesc.flags            = PxConvexFlag::eCOMPUTE_CONVEX;
	convexDesc.flags            |= PxConvexFlag::eINFLATE_CONVEX;

	PxToolkit::MemoryOutputStream buf;
	if(!mCooking->cookConvexMesh(convexDesc, buf))
		return NULL;

	PxToolkit::MemoryInputData input(buf.getData(), buf.getSize());
	PxConvexMesh* convexMesh = mPhysics->createConvexMesh(input);

	XMFLOAT3 pos = owner->getPosition();
	XMFLOAT3 scale = owner->getScale();
	XMMATRIX rot = XMMatrixRotationQuaternion(*owner->getQuaternion());

	XMFLOAT3X3 rotMatrix;
	XMStoreFloat3x3(&rotMatrix,rot);
	physx::PxMat33* rotPhysx = (physx::PxMat33*)&rotMatrix;
	PxQuat orientation = PxQuat(*rotPhysx);
	PxTransform transf = PxTransform(physx::PxVec3(pos.x,pos.y,pos.z),orientation);
	transf.rotate(PxVec3(0,180,0));

	PxRigidStatic* aConvexActor = mPhysics->createRigidStatic(transf);
	PxShape* aConvexShape = aConvexActor->createShape(PxConvexMeshGeometry(convexMesh,PxMeshScale(PxVec3(scale.x,scale.y,scale.z),PxQuat::createIdentity())), *material);

	mScene->addActor(*aConvexActor);
	owner->setPhysicsBody(aConvexActor, false);

	return aConvexActor;
}

PxRigidStatic* AaPhysicsManager::createTreeBodyStatic(AaEntity* owner, PxMaterial* material)
{
	PxTriangleMeshDesc meshDesc;
	meshDesc.points.count           = owner->model->rawInfo->vertex_count;
	meshDesc.points.stride          = sizeof(float)*3;
	meshDesc.points.data            = owner->model->rawInfo->vertices;

	meshDesc.triangles.count        = owner->model->rawInfo->index_count/3;
	meshDesc.triangles.stride       = 3*sizeof(WORD);
	meshDesc.triangles.data         = owner->model->rawInfo->indices;
	meshDesc.flags					= PxMeshFlag::e16_BIT_INDICES;

	PxToolkit::MemoryOutputStream buf;
	if(!mCooking->cookTriangleMesh(meshDesc, buf))
		return NULL;

	XMFLOAT3 pos = owner->getPosition();
	XMFLOAT3 scale = owner->getScale();
	XMMATRIX rot = XMMatrixRotationQuaternion(*owner->getQuaternion());

	XMFLOAT3X3 rotMatrix;
	XMStoreFloat3x3(&rotMatrix,rot);
	physx::PxMat33* rotPhysx = (physx::PxMat33*)&rotMatrix;
	PxQuat orientation = PxQuat(*rotPhysx);
	PxTransform transf = PxTransform(physx::PxVec3(pos.x,pos.y,pos.z),orientation);
	transf.rotate(PxVec3(0,180,0));

	PxRigidStatic* aTreeActor = mPhysics->createRigidStatic(transf);
	PxToolkit::MemoryInputData readBuffer(buf.getData(), buf.getSize());
	PxTriangleMesh* triMesh = mPhysics->createTriangleMesh(readBuffer);
	PxShape* aTriMeshShape = aTreeActor->createShape(PxTriangleMeshGeometry(triMesh,PxMeshScale(PxVec3(scale.x,scale.y,scale.z),PxQuat::createIdentity())), *material);
	mScene->addActor(*aTreeActor);
	owner->setPhysicsBody(aTreeActor, false);

	return aTreeActor;
}
