#pragma once

#include "AaEntity.h"
#include <list>

class AaSceneNode
{
public:
	
	AaSceneNode();
	AaSceneNode(XMFLOAT3 pos);
	AaSceneNode(AaSceneNode* parent);
	~AaSceneNode();
	
	void move(XMFLOAT3 translate);
	void setPosition(XMFLOAT3 pos);
	XMFLOAT3 const getPosition();
	void localYaw(float Yaw);
	void localPitch(float Pitch);
	void localRoll(float Roll);
	void yawPitchRoll(float Yaw, float Pitch, float Roll);

	AaSceneNode* createChild();
	void attachEntity(AaEntity* ent);
	void childDeleted(AaSceneNode* child);
	void parentDeleted();

private:

	AaSceneNode* parent = nullptr;
	XMFLOAT3 position{};
	std::list<AaSceneNode*> children;
	std::list<AaEntity*> entities;
	float yaw_ = 0;
	float pitch_ = 0;
	float roll_ = 0;
};
