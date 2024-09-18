#pragma once

#include "AaRenderSystem.h"
#include "AaMaterial.h"
#include "AaModel.h"
#include "AaCamera.h"

class AaSceneManager;

class AaEntity
{
public:

	AaEntity(AaSceneManager* sceneMgr);
	AaEntity(std::string name, AaSceneManager* sceneMgr);
	~AaEntity();

	std::string getName() { return name; }
	void setMaterial(AaMaterial* material);
	void setModel(std::string filename);

	XMMATRIX getWorldMatrix();

	void setPosition(float x, float y, float z);
	void setPosition(XMFLOAT3 position);
	void setScale(XMFLOAT3 scale);

	XMFLOAT3 getPosition();
	XMFLOAT3 getScale();
	XMVECTOR* getQuaternion();

	void setQuaternion(XMFLOAT4 orientation);
	void yaw(float yaw);
	void pitch(float pitch);
	void roll(float roll);
	void resetRotation();

	AaModel* model{};
	UCHAR renderQueueOrder{};
	bool visible = true;

	const BoundingBox& getWorldBoundingBox();

private:

	bool dirtyWM = true;
	XMMATRIX* wMatrix{};
	XMVECTOR* quaternion{};
	AaMaterial* material{};
	std::string name;
	XMFLOAT3 position{};
	XMFLOAT3 scale{};
	AaSceneManager* mSceneMgr{};

	BoundingBox bbox;
};
