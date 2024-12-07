#pragma once

#include "AaMath.h"

struct ObjectTransformation
{
	Quaternion orientation;
	Vector3 position;
	Vector3	scale{1, 1, 1};
	bool dirty = true;

	XMMATRIX createWorldMatrix() const;
};

using RenderObjectsVisibilityState = std::vector<bool>;
using RenderObjectsFilter = std::vector<UINT>;

struct RenderObjectsVisibilityData
{
	std::vector<bool> visibility;
	std::vector<XMFLOAT4X4> wvpMatrix;
};

class RenderObject;
class AaCamera;

class RenderObjectsStorage
{
public:

	RenderObjectsStorage();
	~RenderObjectsStorage();

	UINT createId(RenderObject*);
	void deleteId(UINT);

	void updateTransformation();
	void updateTransformation(UINT id, ObjectTransformation& transformation);
	void initializeTransformation(UINT id, ObjectTransformation& transformation);

	void updateVisibility(const AaCamera& camera, RenderObjectsVisibilityData&) const;
	void updateVisibility(const AaCamera& camera, RenderObjectsVisibilityData&, const RenderObjectsFilter&) const;

	struct
	{
		std::vector<ObjectTransformation> transformation;
		std::vector<XMMATRIX> worldMatrix;
		std::vector<XMMATRIX> prevWorldMatrix;
		std::vector<BoundingBox> worldBbox;
		std::vector<BoundingBox> bbox;
	}
	objectsData;

private:

	void updateWVPMatrix(const XMMATRIX& viewProjection, RenderObjectsVisibilityData&) const;
	void updateWVPMatrix(const XMMATRIX& viewProjection, RenderObjectsVisibilityData&, const RenderObjectsFilter&) const;

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
	XMFLOAT4X4 getWvpMatrix(const std::vector<XMFLOAT4X4>&) const;

	void setBoundingBox(BoundingBox bbox);
	BoundingBox getBoundingBox() const;
	BoundingBox getWorldBoundingBox() const;

	UINT getId() const;

private:

	UINT id;
	RenderObjectsStorage& source;
};
