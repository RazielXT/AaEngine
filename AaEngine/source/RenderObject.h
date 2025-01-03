#pragma once

#include "MathUtils.h"
#include "ObjectId.h"

struct ObjectTransformation
{
	Quaternion orientation;
	Vector3 position;
	Vector3	scale{1, 1, 1};
	bool dirty = true;

	XMMATRIX createWorldMatrix() const;
	ObjectTransformation operator+(const ObjectTransformation& rhs) const
	{
		ObjectTransformation obj{};
		obj.position = position + rhs.position;
		obj.orientation = orientation * rhs.orientation;
		obj.scale = scale * rhs.scale;

		return obj;
	}
};

using RenderObjectsVisibilityState = std::vector<bool>;
using RenderObjectsFilter = std::vector<UINT>;

struct RenderObjectsVisibilityData
{
	std::vector<bool> visibility;
};

class RenderObject;
class Camera;

class RenderObjectsStorage
{
public:

	RenderObjectsStorage(Order o = Order::Normal);
	~RenderObjectsStorage();

	UINT createId(RenderObject*);
	void deleteId(UINT);

	void updateTransformation();
	void updateTransformation(UINT id, ObjectTransformation& transformation);
	void initializeTransformation(UINT id, ObjectTransformation& transformation);

	void updateVisibility(const Camera& camera, RenderObjectsVisibilityData&) const;
	void updateVisibility(const Camera& camera, RenderObjectsVisibilityData&, const RenderObjectsFilter&) const;

	struct
	{
		std::vector<ObjectTransformation> transformation;
		std::vector<XMMATRIX> worldMatrix;
		std::vector<XMMATRIX> prevWorldMatrix;
		std::vector<BoundingBox> worldBbox;
		std::vector<BoundingBox> bbox;
		std::vector<RenderObject*> objects;
	}
	objectsData;

	RenderObject* getObject(UINT id) const;
	void iterateObjects(std::function<void(RenderObject&)>) const;

	Order order;

private:

	void updateVisibility(const BoundingFrustum&, RenderObjectsVisibilityState&) const;
	void updateVisibility(const BoundingFrustum&, RenderObjectsVisibilityState&, const RenderObjectsFilter&) const;
	void updateVisibility(const BoundingOrientedBox&, RenderObjectsVisibilityState&) const;
	void updateVisibility(const BoundingOrientedBox&, RenderObjectsVisibilityState&, const RenderObjectsFilter&) const;

	void reset();

	std::vector<UINT> ids;
	std::vector<UINT> freeIds;
};

class RenderObject
{
public:

	RenderObject(RenderObjectsStorage&);
	~RenderObject();

	void setPosition(Vector3 position);
	void setScale(Vector3 scale);

	Vector3 getPosition() const;
	Vector3 getScale() const;
	Quaternion getOrientation() const;

	void setOrientation(Quaternion orientation);
	void yaw(float yaw);
	void pitch(float pitch);
	void roll(float roll);
	void resetRotation();

	void setTransformation(const ObjectTransformation& transformation, bool initialize);
	const ObjectTransformation& getTransformation() const;

	bool isVisible(const RenderObjectsVisibilityState&) const;

	XMMATRIX getWorldMatrix() const;
	XMMATRIX getPreviousWorldMatrix() const;

	void setBoundingBox(const BoundingBox& bbox);
	const BoundingBox& getBoundingBox() const;
	const BoundingBox& getWorldBoundingBox() const;

	UINT getId() const;
	ObjectId getGlobalId() const;

	RenderObjectsStorage& getStorage() const;

private:

	UINT id;
	RenderObjectsStorage& source;
};
