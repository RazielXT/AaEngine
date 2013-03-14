#ifndef _AA_ENTITY_
#define _AA_ENTITY_

#include <string>

#include "PxRigidActor.h"
#include "AaRenderSystem.h"
#include "AaMaterial.h"
#include "AaModelInfo.h"
#include "AaCamera.h"


struct perEntityInfo
{
	XMMATRIX worldViewProjection_matrix;
};

struct VertexPosE
{
XMFLOAT3 pos;
XMFLOAT2 tex0;
};

class AaSceneManager;

class AaEntity
{

public:

	AaEntity(AaSceneManager* sceneMgr, AaCamera** cameraPtr);
	AaEntity(std::string name, AaSceneManager* sceneMgr, AaCamera** cameraPtr);
	~AaEntity();
	std::string getName() {return name;}
	void setMaterial(AaMaterial* material);
	XMMATRIX getWorldMatrix();
	XMMATRIX getWorldViewProjectionMatrix();
	void setPosition(float x,float y,float z);
	void setPosition(XMFLOAT3 position);
	void setScale(XMFLOAT3 scale);
	void setModel(std::string filename);
	XMFLOAT3 getPosition();
	XMFLOAT3 getScale();
	XMFLOAT3 getYawPitchRoll();
	void yaw(float yaw);
	void pitch(float pitch);
	void roll(float roll);
	void resetRotation();
	ID3D11InputLayout* inputLayout;
	ID3D11Buffer* vertexBuffer;
	ID3D11Buffer* indexBuffer;
	AaModelInfo* model;
	char renderQueueOrder;
	bool visible;
	
	void removePhysicsBody();
	void setPhysicsBody(physx::PxRigidActor* actor, bool dynamic = true);
	physx::PxRigidActor* getPhysicsBody();
	void setPhysicsTranform(XMFLOAT3 position, XMFLOAT4 orientation);

private:

	void createInputLayout();

	physx::PxRigidActor* body;
	bool dirtyWM, hasPhysics, hasDynamicPhysics;
	XMMATRIX* wMatrix;
	XMVECTOR* quaternion;
	AaMaterial* material;
	std::string name;
	XMFLOAT3 position;
	XMFLOAT3 scale;
	float mYaw,mPitch,mRoll;
	AaCamera** cameraPtr;
	AaSceneManager* mSceneMgr;

};

#endif