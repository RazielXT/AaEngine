#include "AaSceneNode.h"
#include "AaEntity.h"

AaSceneNode::AaSceneNode()
{
}

AaSceneNode::AaSceneNode(XMFLOAT3 pos)
{
	position = pos;
}

AaSceneNode::AaSceneNode(AaSceneNode* parent)
{
	this->parent = parent;
}

AaSceneNode::~AaSceneNode()
{
	if (parent)
		parent->childDeleted(this);

	for (auto c : children)
		c->parentDeleted();
}

void AaSceneNode::move(XMFLOAT3 translate)
{
	position.x += translate.x;
	position.y += translate.y;
	position.z += translate.z;

	for (auto c : children)
		c->move(translate);

	for (auto e : entities)
		e->setPosition(position);
}

void AaSceneNode::setPosition(XMFLOAT3 pos)
{
	XMFLOAT3 translate(pos.x - position.x, pos.y - position.y, pos.z - position.z);

	position = pos;

	for (auto c : children)
		c->move(translate);

	for (auto e : entities)
		e->setPosition(pos);
}

AaSceneNode* AaSceneNode::createChild()
{
	auto child = new AaSceneNode(this);
	children.push_back(child);
	return child;
}

void AaSceneNode::yawPitchRoll(float Yaw, float Pitch, float Roll)
{
	yaw_ += Yaw;
	pitch_ += Pitch;
	roll_ += Roll;

	XMMATRIX m = XMMatrixRotationRollPitchYaw(Roll, Pitch, Yaw);

	for (auto c : children)
	{
		XMFLOAT3 f(c->getPosition().x - position.x, c->getPosition().y - position.y, c->getPosition().z - position.z);
		XMVECTOR v = XMLoadFloat3(&f);
		XMVECTOR nv = XMVector3Transform(v, m);
		XMFLOAT3 np;
		XMStoreFloat3(&np, nv);

		c->setPosition(XMFLOAT3(position.x + np.x, position.y + np.y, position.z + np.z));
		c->yawPitchRoll(Yaw, Pitch, Roll);
	}

	for (auto e : entities)
	{
		e->yaw(Yaw);
		e->pitch(Pitch);
		e->roll(Roll);
	}
}

void AaSceneNode::localYaw(float Yaw)
{
	yaw_ += Yaw;

	for (auto c : children)
		c->localYaw(Yaw);

	for (auto e : entities)
		e->yaw(Yaw);
}

void AaSceneNode::localPitch(float Pitch)
{
	pitch_ += Pitch;

	for (auto c : children)
		c->localPitch(Pitch);

	for (auto e : entities)
		e->pitch(Pitch);
}

void AaSceneNode::localRoll(float Roll)
{
	roll_ += Roll;

	for (auto c : children)
		c->localRoll(Roll);

	for (auto e : entities)
		e->roll(Roll);
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
	parent = nullptr;
}

XMFLOAT3 const AaSceneNode::getPosition()
{
	return position;
}
