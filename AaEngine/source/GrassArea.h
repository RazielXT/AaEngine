#pragma once

#include "ShaderConstantBuffers.h"
#include "GrassInitComputeShader.h"
#include "AaMath.h"
#include <vector>

class AaEntity;

struct GrassAreaPlacementTask
{
	AaEntity* terrain{};
	BoundingBox bbox;
};

struct GrassAreaDescription
{
	void initialize(BoundingBoxVolume extends);

	DirectX::BoundingBox bbox;

	UINT count{};
	XMUINT2 areaCount{};
	UINT vertexCount{};

	float width = 2;
	float spacing = 2;
};
struct GrassArea
{
	UINT vertexCount{};
	ComPtr<ID3D12Resource> gpuBuffer;
	ComPtr<ID3D12Resource> vertexCounter;
	ComPtr<ID3D12Resource> vertexCounterRead;
};

class GrassManager
{
public:

	GrassManager(AaRenderSystem* rs);
	~GrassManager();

	GrassArea* createGrass(const GrassAreaDescription& desc);
	void initializeGrassBuffer(GrassArea& grass, GrassAreaDescription& desc, XMMATRIX invView, UINT colorTex, UINT depthTex, ID3D12GraphicsCommandList* commandList, UINT frameIndex);

	void clear();

private:

	std::vector<GrassArea*> grasses;

	GrassInitComputeShader grassCS;
};