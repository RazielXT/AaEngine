#pragma once

#include "AaRenderSystem.h"
#include "AaCamera.h"
#include "SceneLights.h"
#include "ShaderConstantBuffers.h"

class AaShadowMap
{
public:

	AaShadowMap(aa::SceneLights::Light&);

	void init(AaRenderSystem* renderSystem);

	RenderDepthTargetTexture texture[2];
	AaCamera camera[2];

	aa::SceneLights::Light& light;
	void update(UINT frameIndex);
	void clear(ID3D12GraphicsCommandList* commandList, UINT frameIndex);

private:

	CbufferView cbuffer;
};
