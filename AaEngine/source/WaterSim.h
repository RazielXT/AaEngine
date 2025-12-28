#pragma once

#include "ShaderDataBuffers.h"
#include "WaterSimCS.h"
#include "GraphicsResources.h"
#include "TextureResources.h"
#include "TextureToMeshCS.h"
#include "CopyTexturesCS.h"
#include "GenerateMipsComputeShader.h"

class SceneManager;

class WaterSim
{
public:

	WaterSim();
	~WaterSim();

	void initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, SceneManager& s);
	void update(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx, Vector3 cameraPos);
	void updateCompute(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx);
	void clear();

	void prepareForRendering(ID3D12GraphicsCommandList* commandList);
	void prepareAfterRendering(ID3D12GraphicsCommandList* commandList);

private:

	WaterSimContinuityComputeShader continuityComputeShader;
	WaterSimMomentumComputeShader momentumComputeShader;

	GenerateHeightmapNormalsCS heightmapToNormalCS;
	WaterTextureToTextureCS waterToTextureCS;

	CopyTexturesCS copyTexturesCS;
	WaterMeshTextureCS meshTextureCS;

	FileTexture* terrainTexture;
	FileTexture* terrainTexture2;

	GpuTexture2D terrainHeight;
	GpuTexture2D terrainNormal;
	std::vector<ShaderUAV> terrainNormalMips;
	GenerateNormalMips4xCS generateNormalMipsCS;

	GpuTexture2D waterHeight[FrameCount];
	GpuTexture2D waterVelocity[FrameCount];

	GpuTexture2D waterNormalTexture;
	std::vector<ShaderUAV> waterNormalTextureMips;

	ComPtr<ID3D12Resource> srcWater;
	ComPtr<ID3D12Resource> srcVelocity;

	VertexBufferModel waterModel;
	VertexBufferModel terrainModel;
};
