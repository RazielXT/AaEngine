#pragma once

#include "AaMath.h"

struct WorldCoordinates
{
	XMVECTOR orientation;
	Vector3 position;
	Vector3	scale;
	bool dirty;
};

struct RenderObjectData
{
	std::vector<XMMATRIX> worldMatrix;
	std::vector<BoundingBox> worldBbox;
	std::vector<BoundingBox> bbox;
};

class RenderObject;

using RenderableVisibility = std::vector<bool>;

class Renderables
{
public:

	Renderables();
	~Renderables();

	static Renderables& Get();

	UINT createId(RenderObject*);
	void deleteId(UINT);

	std::vector<WorldCoordinates> coordinates;
	RenderObjectData renderData;

	void updateWorldMatrix();
	void updateWVPMatrix(XMMATRIX viewProjection, const RenderableVisibility&, std::vector<XMFLOAT4X4>&);
	void updateVisibility(const BoundingFrustum&, RenderableVisibility&);
	void updateVisibility(const BoundingOrientedBox&, RenderableVisibility&);

private:

	std::vector<UINT> ids;
	std::vector<UINT> freeIds;

	std::vector<RenderObject*> objects;
};

class RenderObject
{
public:

	RenderObject();
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

protected:

	void setBoundingBox(BoundingBox bbox);

private:

	UINT id;
};
