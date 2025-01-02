#pragma once

#include "RenderSystem.h"
#include "Camera.h"
#include "SceneLights.h"
#include "ShaderDataBuffers.h"
#include "ShaderResources.h"
#include "GraphicsResources.h"

class AaShadowMap
{
public:

	AaShadowMap(SceneLights::Light&, PssmParameters&);

	void init(RenderSystem& renderSystem, GraphicsResources& resources);

	RenderTargetTexture texture[2];
	Camera camera[2];

	SceneLights::Light& sun;
	void update(UINT frameIndex, Camera& camera);
	void update(UINT frameIndex, Vector3 center);
	void clear(ID3D12GraphicsCommandList* commandList);

private:

	RenderTargetHeap targetHeap;
	PssmParameters& data;
	CbufferView cbuffer;
};
