#include "AaRenderables.h"
#include "AaCamera.h"

Renderables::Renderables()
{
}

Renderables::~Renderables()
{
}

UINT Renderables::createId(RenderObject* obj)
{
	if (!freeIds.empty())
	{
		auto id = freeIds.back();
		freeIds.pop_back();

		objectData.coordinates[id] = {};
		objectData.bbox[id] = {};
		objectData.worldBbox[id] = {};
		objectData.worldMatrix[id] = {};

		auto pos = std::lower_bound(ids.begin(), ids.end(), id);
		ids.insert(pos, id);

		return id;
	}

	auto id = (UINT)ids.size();

	objectData.coordinates.push_back({});
	objectData.bbox.emplace_back();
	objectData.worldBbox.emplace_back();
	objectData.worldMatrix.emplace_back();

	ids.push_back(id);

	return id;
}

void Renderables::deleteId(UINT id)
{
	auto pos = std::lower_bound(ids.begin(), ids.end(), id);
	ids.erase(pos);

	freeIds.push_back(id);

	if (ids.empty())
		reset();
}

void Renderables::updateTransformation()
{
	for (auto id : ids)
	{
		auto& coords = objectData.coordinates[id];
		if (!coords.dirty)
			continue;

		coords.dirty = false;
		objectData.worldMatrix[id] = coords.createWorldMatrix();
		objectData.bbox[id].Transform(objectData.worldBbox[id], objectData.worldMatrix[id]);
	}
}

void Renderables::updateWVPMatrix(XMMATRIX viewProjection, const RenderableVisibility& visible, std::vector<XMFLOAT4X4>& wvpMatrix) const
{
	wvpMatrix.resize(ids.size());

	for (auto id : ids)
	{
		if (visible[id])
		{
			XMStoreFloat4x4(&wvpMatrix[id], XMMatrixMultiplyTranspose(objectData.worldMatrix[id], viewProjection));
		}
	}
}

void Renderables::updateVisibility(const BoundingFrustum& frustum, RenderableVisibility& visible) const
{
	visible.resize(ids.size());

	for (auto id : ids)
	{
		visible[id] = frustum.Intersects(objectData.worldBbox[id]);
	}
}

void Renderables::updateVisibility(const BoundingOrientedBox& box, RenderableVisibility& visible) const
{
	visible.resize(ids.size());

	for (auto id : ids)
	{
		visible[id] = box.Intersects(objectData.worldBbox[id]);
	}
}

void Renderables::updateRenderInformation(AaCamera& camera, RenderInformation& info) const
{
	if (camera.isOrthographic())
		updateVisibility(camera.prepareOrientedBox(), info.visibility);
	else
		updateVisibility(camera.prepareFrustum(), info.visibility);

	updateWVPMatrix(camera.getViewProjectionMatrix(), info.visibility, info.wvpMatrix);
}

void Renderables::reset()
{
	freeIds.clear();
	objectData = {};
}

DirectX::XMMATRIX WorldCoordinates::createWorldMatrix() const
{
// 	XMMATRIX translationM = XMMatrixTranslation(position.x, position.y, position.z);
// 	XMMATRIX rotationM = XMMatrixRotationQuaternion(orientation);
// 	XMMATRIX scaleM = XMMatrixScaling(scale.x, scale.y, scale.z);
// 
// 	return XMMatrixMultiply(XMMatrixMultiply(scaleM, rotationM), translationM);

	XMMATRIX affineMatrix = XMMatrixAffineTransformation(
		XMVectorSet(scale.x, scale.y, scale.z, 0.0f),
		XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), // rotation origin at the object's origin
		orientation, // rotation quaternion
		XMVectorSet(position.x, position.y, position.z, 0.0f)); // translation

	return affineMatrix;
}

RenderObject::RenderObject(Renderables& r) : source(r)
{
	id = source.createId(this);
}

RenderObject::~RenderObject()
{
	source.deleteId(id);
}

void RenderObject::setPosition(Vector3 position)
{
	auto& coords = source.objectData.coordinates[id];
	coords.dirty = true;
	coords.position = position;
}

void RenderObject::setScale(Vector3 scale)
{
	auto& coords = source.objectData.coordinates[id];
	coords.dirty = true;
	coords.scale = scale;
}

Vector3 RenderObject::getPosition() const
{
	return source.objectData.coordinates[id].position;
}

Vector3 RenderObject::getScale() const
{
	return source.objectData.coordinates[id].scale;
}

Quaternion RenderObject::getOrientation() const
{
	Quaternion q;
	XMStoreFloat4(&q, source.objectData.coordinates[id].orientation);
	return q;
}

void RenderObject::setOrientation(Quaternion orientation)
{
	auto& coords = source.objectData.coordinates[id];
	coords.dirty = true;
	coords.orientation = orientation;
}

void RenderObject::yaw(float yaw)
{
	auto& coords = source.objectData.coordinates[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(0, yaw, 0));
}

void RenderObject::pitch(float pitch)
{
	auto& coords = source.objectData.coordinates[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(pitch, 0, 0));
}

void RenderObject::roll(float roll)
{
	auto& coords = source.objectData.coordinates[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(0, 0, roll));
}

void RenderObject::resetRotation()
{
	auto& coords = source.objectData.coordinates[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionIdentity();
}

bool RenderObject::isVisible(const RenderableVisibility& visible) const
{
	return visible[id];
}

XMMATRIX RenderObject::getWorldMatrix() const
{
	return source.objectData.worldMatrix[id];
}

DirectX::XMFLOAT4X4 RenderObject::getWvpMatrix(const std::vector<XMFLOAT4X4>& wvpMatrix) const
{
	return wvpMatrix[id];
}

void RenderObject::setBoundingBox(BoundingBox bbox)
{
	source.objectData.bbox[id] = bbox;
}

DirectX::BoundingBox RenderObject::getBoundingBox() const
{
	return source.objectData.bbox[id];
}

DirectX::BoundingBox RenderObject::getWorldBoundingBox() const
{
	auto bbox = getBoundingBox();
	bbox.Center += getPosition();
	bbox.Extents *= getScale();

	return bbox;
}

void RenderInformation::updateVisibility(AaCamera& camera)
{
	source->updateRenderInformation(camera, *this);
}
