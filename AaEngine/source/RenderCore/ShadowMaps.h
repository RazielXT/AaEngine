#pragma once

#include "RenderCore/RenderSystem.h"
#include "Scene/Camera.h"
#include "Scene/SceneLights.h"
#include "Scene/FrameParameters.h"
#include "RenderCore/CascadedShadowMaps.h"

struct ShadowMap
{
	GpuTexture2D texture;
	Camera camera;
};

class ShadowMaps
{
public:

	ShadowMaps(RenderSystem& renderSystem, SceneLights::Light&, SkyParameters&);

	void init();

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
	SkyParameters& data;

	ShadowMapCascade cascadeInfo;

	RenderSystem& renderSystem;
};
