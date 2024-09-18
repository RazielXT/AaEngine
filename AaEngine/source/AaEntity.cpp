#include "AaEntity.h"
#include "AaSceneManager.h"

static int entityCounter = 0;

AaEntity::AaEntity(AaSceneManager* sceneMgr) : AaEntity("entity" + std::to_string(entityCounter++), sceneMgr)
{
}

AaEntity::AaEntity(std::string name, AaSceneManager* sceneMgr)
{
	mSceneMgr = sceneMgr;
	wMatrix = (XMMATRIX*)_aligned_malloc(sizeof(XMMATRIX), 64);
	quaternion = (XMVECTOR*)_aligned_malloc(sizeof(XMVECTOR), 16);
	auto init = XMFLOAT4(0, 0, 0, 1);
	*quaternion = XMLoadFloat4(&init);
	this->name = name;
	scale = XMFLOAT3(1, 1, 1);
	position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	renderQueueOrder = 4;
}

AaEntity::~AaEntity()
{
	_aligned_free(wMatrix);
	_aligned_free(quaternion);
}

void AaEntity::setMaterial(AaMaterial* material)
{
	this->material = material;
}

XMMATRIX AaEntity::getWorldMatrix()
{
	if (dirtyWM)
	{
		XMMATRIX translationM = XMMatrixTranslation(position.x, position.y, position.z);
		XMMATRIX rotationM = XMMatrixRotationQuaternion(*quaternion);
		XMMATRIX scaleM = XMMatrixScaling(scale.x, scale.y, scale.z);

		*wMatrix = XMMatrixMultiply(XMMatrixMultiply(scaleM, rotationM), translationM);
		dirtyWM = false;
	}

	return *wMatrix;
}

void AaEntity::setPosition(XMFLOAT3 position)
{
	dirtyWM = true;
	this->position = position;
}

void AaEntity::setPosition(float x, float y, float z)
{
	dirtyWM = true;
	this->position = XMFLOAT3(x, y, z);
}

void AaEntity::setScale(XMFLOAT3 scale)
{
	dirtyWM = true;
	this->scale = scale;
}

XMFLOAT3 AaEntity::getPosition()
{
	return position;
}

XMFLOAT3 AaEntity::getScale()
{
	return scale;
}

XMVECTOR* AaEntity::getQuaternion()
{
	return quaternion;
}

void AaEntity::setModel(std::string filename)
{
	model = AaModelResources::get().getModel(filename);
}

void AaEntity::setQuaternion(XMFLOAT4 orientation)
{
	dirtyWM = true;
	*quaternion = XMLoadFloat4(&orientation);
}

void AaEntity::yaw(float yaw)
{
	dirtyWM = true;
	*quaternion = XMQuaternionMultiply(*quaternion, XMQuaternionRotationRollPitchYaw(0, yaw, 0));
}

void AaEntity::pitch(float pitch)
{
	dirtyWM = true;
	*quaternion = XMQuaternionMultiply(*quaternion, XMQuaternionRotationRollPitchYaw(pitch, 0, 0));
}

void AaEntity::roll(float roll)
{
	dirtyWM = true;
	*quaternion = XMQuaternionMultiply(*quaternion, XMQuaternionRotationRollPitchYaw(0, 0, roll));
}

void AaEntity::resetRotation()
{
	dirtyWM = true;
	*quaternion = XMQuaternionIdentity();
}

const DirectX::BoundingBox& AaEntity::getWorldBoundingBox()
{
	model->bbox.Transform(bbox, getWorldMatrix());

	return bbox;
}
