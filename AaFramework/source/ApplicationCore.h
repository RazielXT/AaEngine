#pragma once

#include "RenderSystem.h"
#include "SceneManager.h"
#include "FrameCompositor.h"
#include "ShadowMaps.h"
#include "GraphicsResources.h"
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
		std::string defaultCompositor = "frame";
		FrameCompositor::InitParams defaultCompositorParams;
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

private:

	void onViewportResize(UINT, UINT) override;
	void onScreenResize(UINT, UINT) override;
	float timeSinceLastFrame{};
};
