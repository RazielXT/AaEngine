#pragma once

#include "AaMath.h"

struct WorldCoordinates
{
	Quaternion orientation;
	Vector3 position;
	Vector3	scale{1, 1, 1};
	bool dirty = true;

	XMMATRIX createWorldMatrix() const;
};

struct RenderObjectData
{
	std::vector<WorldCoordinates> coordinates;
	std::vector<XMMATRIX> worldMatrix;
	std::vector<BoundingBox> worldBbox;
	std::vector<BoundingBox> bbox;
};

class RenderObject;
class AaCamera;
class Renderables;

using RenderableVisibility = std::vector<bool>;

struct RenderInformation
{
	Renderables* source{};

	RenderableVisibility visibility;
	std::vector<XMFLOAT4X4> wvpMatrix;

	void updateVisibility(AaCamera& camera);
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

	void updateRenderInformation(AaCamera& camera, RenderInformation&) const;

private:

	void updateWVPMatrix(XMMATRIX viewProjection, const RenderableVisibility&, std::vector<XMFLOAT4X4>&) const;
	void updateVisibility(const BoundingFrustum&, RenderableVisibility&) const;
	void updateVisibility(const BoundingOrientedBox&, RenderableVisibility&) const;

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

	bool isVisible(const RenderableVisibility&) const;

	XMMATRIX getWorldMatrix() const;
	XMFLOAT4X4 getWvpMatrix(const std::vector<XMFLOAT4X4>&) const;

	void setBoundingBox(BoundingBox bbox);
	BoundingBox getBoundingBox() const;
	BoundingBox getWorldBoundingBox() const;

private:

	UINT id;
	Renderables& source;
};
