#pragma once

#include "Resources/Shader/ShaderDataBuffers.h"
#include "Resources/Compute/WaterSimCS.h"
#include "Resources/GraphicsResources.h"
#include "Resources/Textures/TextureResources.h"
#include "Resources/Compute/TextureToMeshCS.h"
#include "Resources/Compute/CopyTexturesCS.h"
#include "Resources/Compute/GenerateMipsComputeShader.h"
#include "Resources/Compute/TerrainGenerationCS.h"
#include "RenderObject/Terrain/GridMesh.h"

class SceneManager;

class WaterSim
{
public:

	WaterSim();
	~WaterSim();

	void initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);
	void initializeTarget(const GpuTexture2D& texture, SceneManager& s, Vector2 size, Vector3 center);

	void update(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx, Vector3 cameraPos);
	void updateCompute(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx);
	void clear();

	void enableLodUpdating(bool enable);
	void enableWaterUpdating(bool enable);

	void prepareForRendering(ID3D12GraphicsCommandList* commandList);
	void prepareAfterRendering(ID3D12GraphicsCommandList* commandList);

private:

	WaterSimContinuityComputeShader continuityComputeShader;
	WaterSimMomentumComputeShader momentumComputeShader;

	WaterTextureToTextureCS waterToTextureCS;

	const GpuTexture2D* terrainHeight{};
	GpuTexture2D waterHeight[FrameCount];
	GpuTexture2D waterVelocity[FrameCount];

	GpuTexture2D waterHeightMeshTexture;

	GpuTexture2D waterNormalTexture;
	std::vector<ShaderTextureViewUAV> waterNormalTextureMips;

	GpuTexture2D waterFlowTexture;
	std::vector<ShaderTextureViewUAV> waterFlowTextureMips;

	GenerateXYMips4xCS generateXYMips4xCS;

	ComPtr<ID3D12Resource> srcWater;

	VertexBufferModel waterModel;
	MaterialInstance* waterMaterial{};

	bool updateWater = true;
	bool updateLod = true;

	GridInstanceMesh waterGridMesh;
	GridLODSystem waterGridTiles;
};
