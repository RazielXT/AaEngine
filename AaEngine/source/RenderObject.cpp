#include "RenderObject.h"
#include "Camera.h"

RenderObjectsStorage::RenderObjectsStorage(Order o) : order(o)
{
}

RenderObjectsStorage::~RenderObjectsStorage()
{
}

UINT RenderObjectsStorage::createId(RenderObject* obj)
{
	if (!freeIds.empty())
	{
		auto id = freeIds.back();
		freeIds.pop_back();

		objectsData.transformation[id] = {};
		objectsData.bbox[id] = {};
		objectsData.worldBbox[id] = {};
		objectsData.worldMatrix[id] = {};
		objectsData.prevWorldMatrix[id] = {};
		objectsData.objects[id] = obj;

		auto pos = std::lower_bound(ids.begin(), ids.end(), id);
		ids.insert(pos, id);

		return id;
	}

	auto id = (UINT)ids.size();

	objectsData.transformation.push_back({});
	objectsData.bbox.emplace_back();
	objectsData.worldBbox.emplace_back();
	objectsData.worldMatrix.emplace_back();
	objectsData.prevWorldMatrix.emplace_back();
	objectsData.objects.push_back(obj);

	ids.push_back(id);

	return id;
}

void RenderObjectsStorage::deleteId(UINT id)
{
	auto pos = std::lower_bound(ids.begin(), ids.end(), id);
	ids.erase(pos);

	freeIds.push_back(id);

	if (ids.empty())
		reset();
}

void RenderObjectsStorage::updateTransformation()
{
	for (auto id : ids)
	{
		auto& transformation = objectsData.transformation[id];

		if (transformation.dirty)
			updateTransformation(id, transformation);
	}
}

void RenderObjectsStorage::updateTransformation(UINT id, ObjectTransformation& transformation)
{
	transformation.dirty = false;
	objectsData.prevWorldMatrix[id] = objectsData.worldMatrix[id];
	objectsData.worldMatrix[id] = transformation.createWorldMatrix();
	objectsData.bbox[id].Transform(objectsData.worldBbox[id], objectsData.worldMatrix[id]);
}

void RenderObjectsStorage::initializeTransformation(UINT id, ObjectTransformation& transformation)
{
	updateTransformation(id, transformation);
	objectsData.prevWorldMatrix[id] = objectsData.worldMatrix[id];
}

void RenderObjectsStorage::updateVisibility(const BoundingFrustum& frustum, RenderObjectsVisibilityState& visible) const
{
	visible.resize(ids.size());

	for (auto id : ids)
	{
		visible[id] = frustum.Intersects(objectsData.worldBbox[id]);
	}
}

void RenderObjectsStorage::updateVisibility(const BoundingOrientedBox& box, RenderObjectsVisibilityState& visible) const
{
	visible.resize(ids.size());

	for (auto id : ids)
	{
		visible[id] = box.Intersects(objectsData.worldBbox[id]);
	}
}

void RenderObjectsStorage::updateVisibility(const BoundingFrustum& frustum, RenderObjectsVisibilityState& visible, const RenderObjectsFilter& filter) const
{
	visible.resize(ids.size());

	for (auto id : filter)
	{
		visible[id] = frustum.Intersects(objectsData.worldBbox[id]);
	}
}

void RenderObjectsStorage::updateVisibility(const BoundingOrientedBox& box, RenderObjectsVisibilityState& visible, const RenderObjectsFilter& filter) const
{
	visible.resize(ids.size());

	for (auto id : filter)
	{
		visible[id] = box.Intersects(objectsData.worldBbox[id]);
	}
}

void RenderObjectsStorage::updateVisibility(const Camera& camera, RenderObjectsVisibilityData& info) const
{
	if (camera.isOrthographic())
		updateVisibility(camera.prepareOrientedBox(), info.visibility);
	else
		updateVisibility(camera.prepareFrustum(), info.visibility);
}

void RenderObjectsStorage::updateVisibility(const Camera& camera, RenderObjectsVisibilityData& info, const RenderObjectsFilter& filter) const
{
	if (camera.isOrthographic())
		updateVisibility(camera.prepareOrientedBox(), info.visibility, filter);
	else
		updateVisibility(camera.prepareFrustum(), info.visibility, filter);
}

RenderObject* RenderObjectsStorage::getObject(UINT id) const
{
	return objectsData.objects.size() > id ? objectsData.objects[id] : nullptr;
}

void RenderObjectsStorage::iterateObjects(std::function<void(RenderObject&)> func) const
{
	for (auto id : ids)
	{
		func(*objectsData.objects[id]);
	}
}

void RenderObjectsStorage::reset()
{
	freeIds.clear();
	objectsData = {};
}

DirectX::XMMATRIX ObjectTransformation::createWorldMatrix() const
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

RenderObject::RenderObject(RenderObjectsStorage& r) : source(r)
{
	id = source.createId(this);
}

RenderObject::~RenderObject()
{
	source.deleteId(id);
}

void RenderObject::setPosition(Vector3 position)
{
	auto& coords = source.objectsData.transformation[id];
	coords.dirty = true;
	coords.position = position;
}

void RenderObject::setScale(Vector3 scale)
{
	auto& coords = source.objectsData.transformation[id];
	coords.dirty = true;
	coords.scale = scale;
}

Vector3 RenderObject::getPosition() const
{
	return source.objectsData.transformation[id].position;
}

Vector3 RenderObject::getScale() const
{
	return source.objectsData.transformation[id].scale;
}

Quaternion RenderObject::getOrientation() const
{
	Quaternion q;
	XMStoreFloat4(&q, source.objectsData.transformation[id].orientation);
	return q;
}

void RenderObject::setOrientation(Quaternion orientation)
{
	auto& coords = source.objectsData.transformation[id];
	coords.dirty = true;
	coords.orientation = orientation;
}

void RenderObject::yaw(float yaw)
{
	auto& coords = source.objectsData.transformation[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(0, yaw, 0));
}

void RenderObject::pitch(float pitch)
{
	auto& coords = source.objectsData.transformation[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(pitch, 0, 0));
}

void RenderObject::roll(float roll)
{
	auto& coords = source.objectsData.transformation[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(0, 0, roll));
}

void RenderObject::resetRotation()
{
	auto& coords = source.objectsData.transformation[id];
	coords.dirty = true;
	coords.orientation = XMQuaternionIdentity();
}

void RenderObject::setTransformation(const ObjectTransformation& transformation, bool initialize)
{
	auto& t = source.objectsData.transformation[id];
	t = transformation;

	if (initialize)
		source.initializeTransformation(id, t);
}

const ObjectTransformation& RenderObject::getTransformation() const
{
	return source.objectsData.transformation[id];
}

bool RenderObject::isVisible(const RenderObjectsVisibilityState& visible) const
{
	return visible[id];
}

XMMATRIX RenderObject::getWorldMatrix() const
{
	return source.objectsData.worldMatrix[id];
}

XMMATRIX RenderObject::getPreviousWorldMatrix() const
{
	return source.objectsData.prevWorldMatrix[id];
}

void RenderObject::setBoundingBox(const BoundingBox& bbox)
{
	source.objectsData.bbox[id] = bbox;
}

const BoundingBox& RenderObject::getBoundingBox() const
{
	return source.objectsData.bbox[id];
}

const BoundingBox& RenderObject::getWorldBoundingBox() const
{
	return source.objectsData.worldBbox[id];
}

UINT RenderObject::getId() const
{
	return id;
}

ObjectId RenderObject::getGlobalId() const
{
	return ObjectId(id, source.order);
}

RenderObjectsStorage& RenderObject::getStorage() const
{
	return source;
}
