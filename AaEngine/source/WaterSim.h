#pragma once

#include "ShaderDataBuffers.h"
#include "WaterSimCS.h"
#include "GraphicsResources.h"
#include "TextureResources.h"
#include "TextureToMeshCS.h"
#include "CopyTexturesCS.h"

class SceneManager;

class WaterSim
{
public:

	WaterSim();
	~WaterSim();

	void initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, SceneManager& s);
	void update(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx);
	void updateCompute(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx);
	void clear();

private:

	WaterSimContinuityComputeShader continuityComputeShader;
	WaterSimMomentumComputeShader momentumComputeShader;

	TextureToMeshCS textureToMeshCS;
	WaterTextureToMeshCS waterToMeshCS;

	CopyTexturesCS copyTexturesCS;
	WaterMeshTextureCS meshTextureCS;

	FileTexture* terrainTexture;
	FileTexture* terrainTexture2;
	RenderTargetTexture terrainHeight;
	RenderTargetTexture waterHeight[FrameCount];
	RenderTargetTexture waterVelocity[FrameCount];

	RenderTargetTexture meshColorTexture;

	ComPtr<ID3D12Resource> srcWater;
	ComPtr<ID3D12Resource> srcVelocity;

	VertexBufferModel waterModel;
	VertexBufferModel terrainModel;
};
