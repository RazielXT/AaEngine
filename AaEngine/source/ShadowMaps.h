#pragma once

#include "RenderSystem.h"
#include "Camera.h"
#include "SceneLights.h"
#include "ShaderDataBuffers.h"
#include "ShaderResources.h"
#include "GraphicsResources.h"
#include "CascadedShadowMaps.h"

class ShadowMaps
{
public:

	ShadowMaps(SceneLights::Light&, PssmParameters&);

	void init(RenderSystem& renderSystem, GraphicsResources& resources);

	struct ShadowData
	{
		GpuTexture2D texture;
		Camera camera;
		bool update = true;
	};

	ShadowData cascades[4];
	ShadowData& maxShadow;

	SceneLights::Light& sun;
	void update(UINT frameIndex, Camera& camera);
	void clear(ID3D12GraphicsCommandList* commandList);

private:

	RenderTargetHeap targetHeap;
	PssmParameters& data;
	CbufferView cbuffer;

	ShadowMapCascade cascadeInfo;

	int counter = 0;
};
