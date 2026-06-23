#pragma once

#include "Resources/Shader/ShaderDataBuffers.h"
#include "Resources/Compute/WaterSimCS.h"
#include "Resources/GraphicsResources.h"
#include "Resources/Textures/TextureResources.h"
#include "Resources/Compute/TextureToMeshCS.h"
#include "Resources/Compute/CopyTexturesCS.h"
#include "Resources/Compute/GenerateMipsComputeShader.h"
#include "Resources/Compute/TerrainGenerationCS.h"
#include "Resources/Compute/CameraWaterStateCS.h"
#include "RenderObject/WaterSim/WaterSimInteraction.h"
#include "RenderObject/Terrain/GridMesh.h"
#include <mutex>

class RenderWorld;

class WaterSim
{
public:

	WaterSim();
	~WaterSim();

	void initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);
	void initializeTarget(const GpuTexture2D& texture, RenderWorld& w, Vector2 size, Vector3 center);

	void update(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx, Vector3 cameraPos);
	void updateCompute(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx);
	void clear();

	void enableLodUpdating(bool enable);
	void enableWaterUpdating(bool enable);

	void prepareForRendering(ID3D12GraphicsCommandList* commandList);
	void prepareAfterRendering(ID3D12GraphicsCommandList* commandList);

	struct WaterAdjustment
	{
		Vector3 worldPosition;
		float radius;
		float heightDelta;
	};
	void addAdjustment(const WaterAdjustment& adjustment);

	struct WaterBounds
	{
		Vector3 center;
		Vector2 size;
	};
	WaterBounds getWaterBounds() const { return { worldCenter, worldSize }; }

	// Water interaction query system
	using InteractionQuery = WaterSimInteraction::Query;
	using InteractionResult = WaterSimInteraction::Result;
	static constexpr UINT MaxInteractionQueries = WaterSimInteraction::MaxQueries;

	void submitInteractionQueries(const std::vector<InteractionQuery>& queries);
	bool consumeInteractionResults(std::vector<InteractionResult>& outResults);

private:

	WaterSimContinuityComputeShader continuityComputeShader;
	WaterSimMomentumComputeShader momentumComputeShader;

	WaterTextureToTextureCS waterToTextureCS;

	WaterAdjustCS waterAdjustCS;
	WaterSmoothCS waterSmoothCS;

	const GpuTexture2D* terrainHeight{};
	GpuTexture2D waterHeight[FrameCount];
	GpuTexture2D waterVelocity[FrameCount];

	GpuTexture2D waterHeightMeshTexture;

	GpuTexture2D waterNormalTexture;
	std::vector<ShaderTextureViewUAV> waterNormalTextureMips;

	GpuTexture2D waterFlowTexture;
	std::vector<ShaderTextureViewUAV> waterFlowTextureMips;

	GenerateXYMips4xCS generateXYMips4xCS;
	CameraWaterStateCS cameraWaterStateCS;

	ComPtr<ID3D12Resource> srcWater;

	VertexBufferModel waterModel;
	MaterialInstance* waterMaterial{};

	bool updateWater = true;
	bool updateLod = true;

	Vector2 worldSize{};
	Vector3 worldCenter{};

	std::mutex adjustmentsMutex;
	std::vector<WaterAdjustment> pendingAdjustments;

	RenderEntity* entity{};
	GridInstanceMesh waterGridMesh;
	GridLODSystem waterGridTiles;

	WaterSimInteraction interaction;

	ComPtr<ID3D12Resource> sceneRenderingStateBuffer;
	bool sceneRenderingStateNeedsReset = true;
};
