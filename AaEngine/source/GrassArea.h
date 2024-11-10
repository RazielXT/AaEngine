#pragma once

#include "ShaderConstantBuffers.h"
#include "GrassInitComputeShader.h"
#include "AaMath.h"
#include <vector>

class AaEntity;

struct GrassArea
{
	void initialize(BoundingBoxVolume extends);

	ComPtr<ID3D12Resource> gpuBuffer;
	ComPtr<ID3D12Resource> vertexCounter;
	ComPtr<ID3D12Resource> vertexCounterRead;

	DirectX::BoundingBox bbox;

	UINT count{};
	XMUINT2 areaCount;
	float width = 2;
	UINT vertexCount{};
};

class GrassManager
{
public:

	GrassManager(AaRenderSystem* rs);
	~GrassManager();

	GrassArea* addGrass(BoundingBoxVolume extends);
	void bakeTerrain(GrassArea& grass, AaEntity* terrain, XMMATRIX invVP, ID3D12GraphicsCommandList* commandList, UINT frameIndex);

	void clear();

private:

	std::vector<GrassArea*> grasses;

	GrassInitComputeShader grassCS;
};