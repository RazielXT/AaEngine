#pragma once

#include "Resources/Compute/ComputeShader.h"
#include "Resources/Shader/ShaderFileParser.h"
#include "Utils/MathUtils.h"
#include <DirectXCollision.h>

class MaterialInstance;
class VertexBufferModel;

struct HeightmapPlacementShaderDescription
{
	std::string name;
	ShaderRef shader;
	std::vector<UINT> constants;
};

struct SlopePlacementParams
{
	UINT samplesPerAxis = 32;
	float density = 0.35f;
	float minNormalY = 0.8f;
	float maxNormalY = 1.0f;
	float minScale = 0.8f;
	float maxScale = 1.2f;
	float heightOffset{};
	UINT padding{};
};

HeightmapPlacementShaderDescription CreateSlopePlacementShader(const SlopePlacementParams& params = {});

struct HeightmapInstanceGroupDescription
{
	MaterialInstance* material{};
	VertexBufferModel* model{};
	XMUINT2 sectorCount{ 1, 1 };
	float sectorWorldSize{};
	UINT maxInstancesPerSector{};
	BoundingBox instanceBounds{};
	HeightmapPlacementShaderDescription placement;
};

class HeightmapPlacementComputeShader : public ComputeShader
{
public:
	struct Input
	{
		UINT terrainTexture;
		float terrainHeight;
		Vector2 terrainWorldMin;
		Vector2 sectorWorldMin;
		float sectorWorldSize;
		UINT maxInstances;
		Vector2 heightmapUvMin;
		Vector2 heightmapUvSize;
		UINT seed;
		UINT indexCount;
	};

	void dispatch(
		ID3D12GraphicsCommandList* commandList,
		const Input& input,
		const std::vector<UINT>& customConstants,
		D3D12_GPU_VIRTUAL_ADDRESS instanceBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS commandBuffer);

private:
	ComPtr<ID3D12Resource> customConstantsBuffer;
};
