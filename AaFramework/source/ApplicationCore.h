#pragma once

#include "RenderCore/RenderSystem.h"
#include "RenderCore/ShadowMaps.h"
#include "Scene/RenderWorld.h"
#include "RenderObject/DrawPrimitives.h"
#include "FrameCompositor/FrameCompositor.h"
#include "Resources/GraphicsResources.h"
#include "Physics/PhysicsManager.h"
#include "Physics/TerrainPhysics.h"
#include "Physics/WaterSimInteractionUpdater.h"
#include "RenderObject/Sky/SkyRendering.h"

#include <memory>
#include <vector>

class SplineConstruction;

namespace DirectX
{
	class ResourceUploadBatch;
}

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

	void loadScene();

	RenderSystem renderSystem;
	GraphicsResources resources;
	RenderWorld renderWorld;
	FrameParameters params;
	PhysicsManager physicsMgr;
	TerrainPhysics terrainPhysics;
	WaterSimInteractionUpdater waterInteraction;
	FrameCompositor* compositor;
	ShadowMaps* shadowMap;
	SceneLights lights;

	PlanesModel planes;
	SkyRendering sky;
private:
	void initializeSplineConstructionPreview(DirectX::ResourceUploadBatch& batch);

	std::vector<std::unique_ptr<SplineConstruction>> splineConstructionPreviews;

	void onViewportResize(UINT, UINT) override;
	void onScreenResize(UINT, UINT) override;
	float timeSinceLastFrame{};
};
