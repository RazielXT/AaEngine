#pragma once

#include "AaRenderSystem.h"
#include "AaCamera.h"
#include "SceneLights.h"

class AaShadowMap
{
public:

	AaShadowMap(aa::SceneLights::Light&);

	void init(AaRenderSystem* renderSystem);

	RenderDepthTargetTexture texture;

	AaCamera camera;
	aa::SceneLights::Light& light;
	void update();
};
