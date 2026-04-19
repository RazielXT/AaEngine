#pragma once

#include "RenderCore/RenderSystem.h"
#include "RenderCore/ShadowMaps.h"
#include "Scene/SceneManager.h"
#include "Scene/DrawPrimitives.h"
#include "FrameCompositor/FrameCompositor.h"
#include "Resources/GraphicsResources.h"
#include "PhysicsManager.h"

struct DebugReporter
{
	DebugReporter();
	~DebugReporter();
};

class ApplicationCore : public ViewportListener
{
	DebugReporter reporter; //initialized first

public:

	ApplicationCore(TargetViewport& viewport);
	~ApplicationCore();

	struct InitParams
	{
		FrameCompositor::InitConfig compositor;
	};
	void initialize(const TargetWindow& window, const InitParams& appParams = InitParams{});

	void beginRendering(std::function<bool(float)>);
	void renderFrame(Camera& camera);
	HRESULT present();

	void loadScene(const char* scene);

	RenderSystem renderSystem;
	GraphicsResources resources;
	SceneManager sceneMgr;
	FrameParameters params;
	PhysicsManager physicsMgr;
	FrameCompositor* compositor;
	ShadowMaps* shadowMap;
	SceneLights lights;

	PlanesModel planes;
private:

	void onViewportResize(UINT, UINT) override;
	void onScreenResize(UINT, UINT) override;
	float timeSinceLastFrame{};
};
