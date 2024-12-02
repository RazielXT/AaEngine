#pragma once

#include "RenderSystem.h"
#include "AaCamera.h"
#include "SceneLights.h"
#include "ShaderConstantBuffers.h"
#include "ShaderResources.h"

class AaShadowMap
{
public:

	AaShadowMap(aa::SceneLights::Light&, PssmParameters&);

	void init(RenderSystem* renderSystem);

	RenderTargetTexture texture[2];
	AaCamera camera[2];

	aa::SceneLights::Light& sun;
	void update(UINT frameIndex);
	void clear(ID3D12GraphicsCommandList* commandList);

private:

	RenderTargetHeap targetHeap;
	PssmParameters& data;
	CbufferView cbuffer;
};
