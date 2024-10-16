#include "AaRenderables.h"

Renderables* instance;

Renderables& Renderables::Get()
{
	return *instance;
}

Renderables::Renderables()
{
	if (instance)
		throw std::exception("Duplicate Renderables");

	instance = this;
}

Renderables::~Renderables()
{
	instance = nullptr;
}

UINT Renderables::createId(RenderObject* obj)
{
	if (!freeIds.empty())
	{
		auto id = freeIds.back();
		freeIds.pop_back();

		coordinates[id] = {};
		renderData.bbox[id] = {};
		renderData.worldBbox[id] = {};
		renderData.worldMatrix[id] = {};

		auto pos = std::lower_bound(ids.begin(), ids.end(), id);
		ids.insert(pos, id);

		return id;
	}

	auto id = (UINT)ids.size();

	coordinates.push_back({});
	renderData.bbox.emplace_back();
	renderData.worldBbox.emplace_back();
	renderData.worldMatrix.emplace_back();

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

void Renderables::updateWorldMatrix()
{
	for (auto id : ids)
	{
		auto& coords = coordinates[id];
		if (!coords.dirty)
			continue;

		coords.dirty = false;

		XMMATRIX translationM = XMMatrixTranslation(coords.position.x, coords.position.y, coords.position.z);
		XMMATRIX rotationM = XMMatrixRotationQuaternion(coords.orientation);
		XMMATRIX scaleM = XMMatrixScaling(coords.scale.x, coords.scale.y, coords.scale.z);

		renderData.worldMatrix[id] = XMMatrixMultiply(XMMatrixMultiply(scaleM, rotationM), translationM);
		renderData.bbox[id].Transform(renderData.worldBbox[id], renderData.worldMatrix[id]);
	}
}

void Renderables::updateWVPMatrix(XMMATRIX viewProjection, const RenderableVisibility& visible, std::vector<XMFLOAT4X4>& wvpMatrix)
{
	wvpMatrix.resize(ids.size());

	for (auto id : ids)
	{
		if (visible[id])
		{
			XMStoreFloat4x4(&wvpMatrix[id], XMMatrixMultiplyTranspose(renderData.worldMatrix[id], viewProjection));
		}
	}
}

void Renderables::updateVisibility(const BoundingFrustum& frustum, RenderableVisibility& visible)
{
	visible.resize(ids.size());

	for (auto id : ids)
	{
		visible[id] = frustum.Intersects(renderData.worldBbox[id]);
	}
}

void Renderables::updateVisibility(const BoundingOrientedBox& box, RenderableVisibility& visible)
{
	visible.resize(ids.size());

	for (auto id : ids)
	{
		visible[id] = box.Intersects(renderData.worldBbox[id]);
	}
}

void Renderables::reset()
{
	freeIds.clear();
	coordinates.clear();
	renderData = {};
}

RenderObject::RenderObject()
{
	id = instance->createId(this);
}

RenderObject::~RenderObject()
{
	instance->deleteId(id);
}

void RenderObject::setPosition(Vector3 position)
{
	auto& coords = instance->coordinates[id];
	coords.dirty = true;
	coords.position = position;
}

void RenderObject::setScale(Vector3 scale)
{
	auto& coords = instance->coordinates[id];
	coords.dirty = true;
	coords.scale = scale;
}

Vector3 RenderObject::getPosition() const
{
	return instance->coordinates[id].position;
}

Vector3 RenderObject::getScale() const
{
	return instance->coordinates[id].scale;
}

Quaternion RenderObject::getOrientation() const
{
	Quaternion q;
	XMStoreFloat4(&q, instance->coordinates[id].orientation);
	return q;
}

void RenderObject::setOrientation(Quaternion orientation)
{
	auto& coords = instance->coordinates[id];
	coords.dirty = true;
	coords.orientation = orientation;
}

void RenderObject::yaw(float yaw)
{
	auto& coords = instance->coordinates[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(0, yaw, 0));
}

void RenderObject::pitch(float pitch)
{
	auto& coords = instance->coordinates[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(pitch, 0, 0));
}

void RenderObject::roll(float roll)
{
	auto& coords = instance->coordinates[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(0, 0, roll));
}

void RenderObject::resetRotation()
{
	auto& coords = instance->coordinates[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionIdentity();
}

bool RenderObject::isVisible(const RenderableVisibility& visible) const
{
	return visible[id];
}

XMMATRIX RenderObject::getWorldMatrix() const
{
	return instance->renderData.worldMatrix[id];
}

DirectX::XMFLOAT4X4 RenderObject::getWvpMatrix(const std::vector<XMFLOAT4X4>& wvpMatrix) const
{
	return wvpMatrix[id];
}

void RenderObject::setBoundingBox(BoundingBox bbox)
{
	instance->renderData.bbox[id] = bbox;
}
