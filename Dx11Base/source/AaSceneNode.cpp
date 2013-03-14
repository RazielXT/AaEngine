#include "AaSceneNode.h"
#include "AaEntity.h"



AaSceneNode::AaSceneNode()
{
	position = XMFLOAT3(0,0,0);
	parent = NULL;
	yaw_=pitch_=roll_=0;
}

AaSceneNode::AaSceneNode(XMFLOAT3 pos)
{
	position = pos;
	parent = NULL;
	yaw_=pitch_=roll_=0;
}

AaSceneNode::AaSceneNode(AaSceneNode* parent)
{
	position = XMFLOAT3(0,0,0);
	this->parent = parent;
	yaw_=pitch_=roll_=0;
}

AaSceneNode::~AaSceneNode()
{
	if(parent)
		parent->childDeleted(this);

	std::list<AaSceneNode*>::iterator it = children.begin();
	while(it!=children.end())
		(*it)->parentDeleted();
}
	
void AaSceneNode::move(XMFLOAT3 translate)
{
	std::list<AaSceneNode*>::iterator it = children.begin();
	for(;it!=children.end();it++)
		(*it)->move(translate);

	position.x += translate.x;
	position.y += translate.y;
	position.z += translate.z;

	std::list<AaEntity*>::iterator it2 = entities.begin();
	for(;it2!=entities.end();it2++)
		(*it2)->setPosition(position);
}

void AaSceneNode::setPosition(XMFLOAT3 pos)
{
	XMFLOAT3 translate(pos.x-position.x,pos.y-position.y,pos.z-position.z);

	position = pos;

	std::list<AaSceneNode*>::iterator it = children.begin();
	for(;it!=children.end();it++)
		(*it)->move(translate);

	std::list<AaEntity*>::iterator it2 = entities.begin();
	for(;it2!=entities.end();it2++)
		(*it2)->setPosition(pos);
}
	
AaSceneNode* AaSceneNode::createChild()
{
	AaSceneNode* child= new AaSceneNode(this);
	children.push_back(child);
	return child;
}

void AaSceneNode::yawPitchRoll(float Yaw, float Pitch, float Roll)
{
	yaw_ += Yaw;
	pitch_ += Pitch;
	roll_ += Roll;

	XMMATRIX m = XMMatrixRotationRollPitchYaw(Roll,Pitch,Yaw);
	
	std::list<AaSceneNode*>::iterator it = children.begin();
	for(;it!=children.end();it++)
	{

		XMFLOAT3 f((*it)->getPosition().x-position.x, (*it)->getPosition().y-position.y, (*it)->getPosition().z-position.z);
		XMVECTOR v = XMLoadFloat3(&f);
		XMVECTOR nv = XMVector3Transform(v,m);
		XMFLOAT3 np;
		XMStoreFloat3(&np,nv);

		(*it)->setPosition(XMFLOAT3(position.x + np.x, position.y + np.y, position.z + np.z));
		(*it)->yawPitchRoll(Yaw, Pitch, Roll);	
	}

	std::list<AaEntity*>::iterator it2 = entities.begin();
	for(;it2!=entities.end();it2++)
	{
		(*it2)->yaw(Yaw);
		(*it2)->pitch(Pitch);
		(*it2)->roll(Roll);
	}
}

void AaSceneNode::localYaw(float Yaw)
{
	yaw_ += Yaw;

	std::list<AaSceneNode*>::iterator it = children.begin();
	for(;it!=children.end();it++)
		(*it)->localYaw(Yaw);

	std::list<AaEntity*>::iterator it2 = entities.begin();
	for(;it2!=entities.end();it2++)
		(*it2)->yaw(Yaw);
}

void AaSceneNode::localPitch(float Pitch)
{
	pitch_ += Pitch;

	std::list<AaSceneNode*>::iterator it = children.begin();
	for(;it!=children.end();it++)
		(*it)->localPitch(Pitch);

	std::list<AaEntity*>::iterator it2 = entities.begin();
	for(;it2!=entities.end();it2++)
		(*it2)->pitch(Pitch);
}

void AaSceneNode::localRoll(float Roll)
{
	roll_ += Roll;

	std::list<AaSceneNode*>::iterator it = children.begin();
	for(;it!=children.end();it++)
		(*it)->localRoll(Roll);

	std::list<AaEntity*>::iterator it2 = entities.begin();
	for(;it2!=entities.end();it2++)
		(*it2)->roll(Roll);
}


void AaSceneNode::attachEntity(AaEntity* ent)
{
	ent->setPosition(position);
	ent->resetRotation();
	ent->yaw(yaw_);
	ent->pitch(pitch_);
	ent->roll(roll_);
	entities.push_back(ent);
}

void AaSceneNode::childDeleted(AaSceneNode* child)
{
	children.remove(child);
}

void AaSceneNode::parentDeleted()
{
	parent = NULL;
}

XMFLOAT3 const AaSceneNode::getPosition()
{
	return position;
}