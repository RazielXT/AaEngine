#pragma once

#include "ShaderDataBuffers.h"
#include "WaterSimCS.h"
#include "GraphicsResources.h"
#include "TextureResources.h"

class WaterSim
{
public:

	WaterSim();
	~WaterSim();

	void initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources);
	void update(ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx);
	void clear();

private:

	WaterSimComputeShader waterSimCS;

	FileTexture* terrainTexture;
	RenderTargetTexture terrainHeight;
	RenderTargetTexture waterHeight[FrameCount];
	RenderTargetTexture waterVelocity[FrameCount];

	ComPtr<ID3D12Resource> srcWater;
};
