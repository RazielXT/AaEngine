#include "ApplicationCore.h"
#include "Utils/Logger.h"
#include "App/Directories.h"
#include "FreeCamera.h"
#include "SceneParser.h"
#include "Resources/Model/ModelResources.h"
#include "Resources/Shader/ShaderLibrary.h"
#include "Resources/Material/MaterialResources.h"
#include "RenderObject/GrassArea.h"
#include "RenderCore/TextureData.h"
#include "FrameCompositor/FrameCompositor.h"
#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "RenderObject/Terrain/TerrainGridParams.h"
#include "PhysicsRenderTask.h"
#include <dxgidebug.h>
#include <filesystem>

ApplicationCore::ApplicationCore(TargetViewport& viewport) : renderSystem(viewport), resources(renderSystem), sceneMgr(resources)
{
	viewport.listeners.push_back(this);
	Vector3(-1, -1, -1).Normalize(lights.directionalLight.direction);
}

ApplicationCore::~ApplicationCore()
{
	renderSystem.core.WaitForAllFrames();

	delete compositor;
	delete shadowMap;
}

void ApplicationCore::initialize(const TargetWindow& window, const InitParams& appParams)
{
	if (!std::filesystem::exists(DATA_DIRECTORY) || !std::filesystem::is_directory(DATA_DIRECTORY))
		Logger::logError("Missing directory " + std::filesystem::absolute(DATA_DIRECTORY).string());

	resources.shaderDefines.setDefine("CFG_SHADOW_HIGH", true);

	ColorSpace colorSpace{};
	if (renderSystem.core.IsDisplayHDR())
		colorSpace = { .outputFormat = DXGI_FORMAT_R10G10B10A2_UNORM, .type = ColorSpace::HDR10 };
	renderSystem.core.initializeSwapChain(window, colorSpace);

	shadowMap = new ShadowMaps(renderSystem, lights.directionalLight, params.sun);
	shadowMap->init(resources);

	compositor = new FrameCompositor(appParams.compositor, { params, renderSystem, resources }, sceneMgr, *shadowMap);
	compositor->setColorSpace(colorSpace);
	compositor->registerTask("RenderPhysics", [this](RenderProvider& provider, SceneManager& sceneMgr)
	{
		return std::make_shared<PhysicsRenderTask>(provider, sceneMgr, physicsMgr);
	});
	compositor->reloadPasses();

	sceneMgr.initialize(renderSystem);

	physicsMgr.init();

	Logger::log("ApplicationCore Initialized");
}

void ApplicationCore::beginRendering(std::function<bool(float)> onUpdate)
{
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	auto frequency = (float)freq.QuadPart;
	LARGE_INTEGER lastTime{};
	QueryPerformanceCounter(&lastTime);

	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// Update time
			LARGE_INTEGER thisTime;
			QueryPerformanceCounter(&thisTime);
			timeSinceLastFrame = (thisTime.QuadPart - lastTime.QuadPart) / frequency;
			lastTime = thisTime;

			if (timeSinceLastFrame > 0.1f)
				timeSinceLastFrame = 0.1f;

			if (!onUpdate(timeSinceLastFrame))
				return;
		}
	}
}

void ApplicationCore::renderFrame(Camera& camera)
{
	params.time += timeSinceLastFrame;
	params.timeDelta = timeSinceLastFrame;
	params.frameIndex = renderSystem.core.frameIndex;

	sceneMgr.update();
	camera.updateMatrix();
	shadowMap->update(renderSystem.core.frameIndex, camera);

	RenderContext ctx = { &camera };
	compositor->render(ctx);
}

HRESULT ApplicationCore::present()
{
	HRESULT r = renderSystem.core.Present();
	renderSystem.core.EndFrame();

	return r;
}

void ApplicationCore::onViewportResize(UINT, UINT)
{
	compositor->reloadTextures();
}

void ApplicationCore::onScreenResize(UINT, UINT)
{
}

void ApplicationCore::loadScene(const char* scene)
{
	renderSystem.core.WaitForAllFrames();
	sceneMgr.clear();
	physicsMgr.init();
	resources.models.clear();

	ResourceUploadBatch batch(renderSystem.core.device);
	batch.Begin();

	sceneMgr.skybox.setMaterial("Sky", sceneMgr.getQueue(MaterialTechnique::Default, Order::Post)->targetFormats);

	sky.createClouds(sceneMgr, resources.materials, renderSystem.core.device, batch);
	sky.createMoon(sceneMgr, resources.materials, batch);

	auto commands = renderSystem.core.CreateCommandList(L"loadScene", PixColor::Load);
	{
		auto marker = renderSystem.core.StartCommandList(commands);

		//initialize gpu resources
		{
			shadowMap->clear(commands.commandList);
		}

// 		for (const auto& i : result.instanceDescriptions)
// 		{
// 			sceneMgr.instancing.build(sceneMgr, i.second);
// 		}
// 
// 		for (const auto& g : result.grassTasks)
// 		{
// 			sceneMgr.grass.scheduleGrassCreation(g, commands.commandList, params, resources, sceneMgr);
// 		}

		marker.move("loadSceneVoxels", commands.color);
		VoxelizeSceneTask::Get().clear(commands.commandList);

 		marker.move("loadSceneTerrain", commands.color);
		sceneMgr.water.initializeGpuResources(renderSystem, resources, batch);
		sceneMgr.terrain.initialize(renderSystem, resources, batch, sceneMgr);
		sceneMgr.vegetation.createChunks(sceneMgr, renderSystem, resources, batch);
		sceneMgr.water.initializeTarget(sceneMgr.terrain.getHeightmap({ 0,0 }), sceneMgr, { sceneMgr.terrain.params.tileSize, sceneMgr.terrain.params.tileSize }, {});
	}

	auto uploadResourcesFinished = batch.End(renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();

	renderSystem.core.ExecuteCommandList(commands);
	renderSystem.core.WaitForCurrentFrame();
	commands.deinit();

	sceneMgr.grass.finishGrassCreation();
}

DebugReporter::DebugReporter()
{
#ifndef NDEBUG
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
#endif
}

DebugReporter::~DebugReporter()
{
#ifndef NDEBUG
	ComPtr<IDXGIDebug> dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
	{
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	}
#endif
}
