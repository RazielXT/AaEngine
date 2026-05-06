#include "Scene/RenderObject.h"
#include "Scene/Camera.h"

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
		objectsData.dirtyTransformation[id] = {};
		objectsData.bbox[id] = {};
		objectsData.worldBbox[id] = {};
		objectsData.worldMatrix[id] = {};
		objectsData.prevWorldMatrix[id] = {};
		objectsData.objects[id] = obj;
		objectsData.flags[id] = {};

		auto pos = std::lower_bound(ids.begin(), ids.end(), id);
		ids.insert(pos, id);

		return id;
	}

	auto id = (UINT)ids.size();

	objectsData.transformation.push_back({});
	objectsData.dirtyTransformation.emplace_back(true);
	objectsData.bbox.emplace_back();
	objectsData.worldBbox.emplace_back();
	objectsData.worldMatrix.emplace_back();
	objectsData.prevWorldMatrix.emplace_back();
	objectsData.objects.push_back(obj);
	objectsData.flags.emplace_back();

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
		if (auto& dirty = objectsData.dirtyTransformation[id])
		{
			updateTransformation(id, objectsData.transformation[id]);
			dirty = {};
		}
	}
}

void RenderObjectsStorage::updateTransformation(UINT id, ObjectTransformation& transformation)
{
	objectsData.prevWorldMatrix[id] = objectsData.worldMatrix[id];
	objectsData.worldMatrix[id] = transformation.createWorldMatrix();
	objectsData.bbox[id].Transform(objectsData.worldBbox[id], objectsData.worldMatrix[id]);
}

void RenderObjectsStorage::initializeTransformation(UINT id, ObjectTransformation& transformation)
{
	updateTransformation(id, transformation);
	objectsData.prevWorldMatrix[id] = objectsData.worldMatrix[id];
}

void RenderObjectsStorage::updateVisibility(const BoundingFrustum& frustum, RenderObjectsVisibilityState& visible, const std::vector<UINT>& ids) const
{
	for (auto id : ids)
	{
		visible[id] = objectsData.worldBbox[id].Extents.x == 0 || frustum.Intersects(objectsData.worldBbox[id]);
	}
}

void RenderObjectsStorage::updateVisibility(const BoundingOrientedBox& box, RenderObjectsVisibilityState& visible, const std::vector<UINT>& ids) const
{
	for (auto id : ids)
	{
		visible[id] = box.Intersects(objectsData.worldBbox[id]);
	}
}

void RenderObjectsStorage::updateVisibility(const Camera& camera, RenderObjectsVisibilityData& info) const
{
	info.visibility.resize(ids.size() + freeIds.size());

	if (camera.isOrthographic())
		updateVisibility(camera.prepareOrientedBox(), info.visibility, ids);
	else
		updateVisibility(camera.prepareFrustum(), info.visibility, ids);
}

void RenderObjectsStorage::updateVisibility(const Camera& camera, const std::vector<UINT>& filtered, RenderObjectsVisibilityData& info) const
{
	info.visibility.resize(ids.size() + freeIds.size());

	if (camera.isOrthographic())
		updateVisibility(camera.prepareOrientedBox(), info.visibility, filtered);
	else
		updateVisibility(camera.prepareFrustum(), info.visibility, filtered);
}

void RenderObjectsStorage::createFilteredIds(uint8_t flag, std::vector<UINT>& filtered) const
{
	filtered.clear();

	for (auto id : ids)
	{
		if (!(objectsData.flags[id] & flag))
			filtered.push_back(id);
	}
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

RenderObject::RenderObject(RenderObjectsStorage& r, uint16_t g) : source(r)
{
	id = source.createId(this);
	groupId = g;
}

RenderObject::~RenderObject()
{
	source.deleteId(id);
}

void RenderObject::setPosition(Vector3 position)
{
	auto& coords = source.objectsData.transformation[id];
	coords.position = position;
	source.objectsData.dirtyTransformation[id] = true;
}

void RenderObject::setScale(Vector3 scale)
{
	auto& coords = source.objectsData.transformation[id];
	coords.scale = scale;
	source.objectsData.dirtyTransformation[id] = true;
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
	coords.orientation = orientation;
	source.objectsData.dirtyTransformation[id] = true;
}

void RenderObject::yaw(float yaw)
{
	auto& coords = source.objectsData.transformation[id];
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(0, yaw, 0));
	source.objectsData.dirtyTransformation[id] = true;
}

void RenderObject::pitch(float pitch)
{
	auto& coords = source.objectsData.transformation[id];
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(pitch, 0, 0));
	source.objectsData.dirtyTransformation[id] = true;
}

void RenderObject::roll(float roll)
{
	auto& coords = source.objectsData.transformation[id];
	coords.orientation = XMQuaternionMultiply(coords.orientation, XMQuaternionRotationRollPitchYaw(0, 0, roll));
	source.objectsData.dirtyTransformation[id] = true;
}

void RenderObject::resetRotation()
{
	setOrientation(XMQuaternionIdentity());
}

void RenderObject::setPositionOrientation(Vector3 position, Quaternion orientation)
{
	auto& coords = source.objectsData.transformation[id];
	coords.position = position;
	coords.orientation = orientation;
	source.objectsData.dirtyTransformation[id] = true;
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

bool RenderObject::isVisible(const BoundingFrustum& frustum) const
{
	return frustum.Intersects(source.objectsData.worldBbox[id]);
}

bool RenderObject::isVisible(const BoundingOrientedBox& box) const
{
	return box.Intersects(source.objectsData.worldBbox[id]);
}

XMMATRIX RenderObject::getWorldMatrix() const
{
	return source.objectsData.worldMatrix[id];
}

XMMATRIX RenderObject::getPreviousWorldMatrix() const
{
	return source.objectsData.prevWorldMatrix[id];
}

void RenderObject::updateWorldMatrix()
{
	source.initializeTransformation(id, source.objectsData.transformation[id]);
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
	return ObjectId(id, source.order, groupId);
}

RenderObjectsStorage& RenderObject::getStorage() const
{
	return source;
}

RenderObjectFlags RenderObject::getFlags() const
{
	return flags;
}

bool RenderObject::hasFlag(RenderObjectFlag::Value v) const
{
	return flags & v;
}

void RenderObject::setFlag(RenderObjectFlag::Value v)
{
	flags |= v;
	source.objectsData.flags[id] = flags;
}
