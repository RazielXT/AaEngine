#pragma once

#include "ShaderConstantBuffers.h"
#include <vector>
#include "AaMath.h"

struct GrassArea
{
	void initialize(BoundingBoxVolume extends);
	UINT getVertexCount() const;

	ComPtr<ID3D12Resource> gpuBuffer;

	DirectX::BoundingBox bbox;
	UINT count{};
};

class GrassManager
{
public:

	GrassManager() = default;
	~GrassManager();

	GrassArea* addGrass(BoundingBoxVolume extends);

	void clear();

private:

	std::vector<GrassArea*> grasses;
};