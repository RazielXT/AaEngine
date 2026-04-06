#pragma once

#include "RenderSystem.h"
#include "Camera.h"
#include "SceneLights.h"
#include "ShaderDataBuffers.h"
#include "ShaderResources.h"
#include "GraphicsResources.h"
#include "CascadedShadowMaps.h"

struct ShadowMap
{
	GpuTexture2D texture;
	Camera camera;
};

class ShadowMaps
{
public:

	ShadowMaps(RenderSystem& renderSystem, SceneLights::Light&, PssmParameters&);

	void init(GraphicsResources& resources);

	struct ShadowData : public ShadowMap
	{
		bool update{};
	};

	ShadowData cascades[4];

	SceneLights::Light& sun;
	void update(UINT frameIndex, Camera& camera);
	void clear(ID3D12GraphicsCommandList* commandList);

	void createShadowMap(ShadowMap& map, UINT size, D3D12_RESOURCE_STATES, const std::string& name);
	void removeShadowMap(ShadowMap& map);
	void updateShadowMap(ShadowMap& map, Vector3 position, float extends) const;

private:

	RenderTargetHeap targetHeap;
	PssmParameters& data;
	CbufferView cbuffer;

	ShadowMapCascade cascadeInfo;

	RenderSystem& renderSystem;
};
