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

struct RenderObjectData
{
	std::vector<ObjectTransformation> transformation;
	std::vector<XMMATRIX> worldMatrix;
	std::vector<BoundingBox> worldBbox;
	std::vector<BoundingBox> bbox;
};

class RenderObject;
class AaCamera;
class Renderables;

using RenderableVisibility = std::vector<bool>;

using RenderableFilter = std::vector<UINT>;

struct RenderInformation
{
	RenderableVisibility visibility;
	std::vector<XMFLOAT4X4> wvpMatrix;
};

class Renderables
{
public:

	Renderables();
	~Renderables();

	UINT createId(RenderObject*);
	void deleteId(UINT);

	RenderObjectData objectData;

	void updateTransformation();
	void updateTransformation(UINT id, ObjectTransformation& transformation);

	void updateRenderInformation(const AaCamera& camera, RenderInformation&) const;
	void updateRenderInformation(const AaCamera& camera, RenderInformation&, const RenderableFilter&) const;

private:

	void updateWVPMatrix(XMMATRIX viewProjection, const RenderableVisibility&, std::vector<XMFLOAT4X4>&) const;
	void updateWVPMatrix(XMMATRIX viewProjection, const RenderableVisibility&, std::vector<XMFLOAT4X4>&, const RenderableFilter&) const;

	void updateVisibility(const BoundingFrustum&, RenderableVisibility&) const;
	void updateVisibility(const BoundingFrustum&, RenderableVisibility&, const RenderableFilter&) const;
	void updateVisibility(const BoundingOrientedBox&, RenderableVisibility&) const;
	void updateVisibility(const BoundingOrientedBox&, RenderableVisibility&, const RenderableFilter&) const;

	void reset();

	std::vector<UINT> ids;
	std::vector<UINT> freeIds;
};

class RenderObject
{
public:

	RenderObject(Renderables&);
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

	void setTransformation(const ObjectTransformation& transformation, bool forceUpdate);
	const ObjectTransformation& getTransformation() const;

	bool isVisible(const RenderableVisibility&) const;

	XMMATRIX getWorldMatrix() const;
	XMFLOAT4X4 getWvpMatrix(const std::vector<XMFLOAT4X4>&) const;

	void setBoundingBox(BoundingBox bbox);
	BoundingBox getBoundingBox() const;
	BoundingBox getWorldBoundingBox() const;

	UINT getId() const;

private:

	UINT id;
	Renderables& source;
};
