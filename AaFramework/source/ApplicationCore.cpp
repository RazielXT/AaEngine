#include "ApplicationCore.h"
#include "FileLogger.h"
#include "Directories.h"
#include "FreeCamera.h"
#include "SceneParser.h"
#include "ModelResources.h"
#include "ShaderLibrary.h"
#include "MaterialResources.h"
#include "FrameCompositor.h"
#include "GrassArea.h"
#include "TextureData.h"
#include "VoxelizeSceneTask.h"
#include <dxgidebug.h>
#include "PhysicsRenderTask.h"

ApplicationCore::ApplicationCore(TargetViewport& viewport) : renderSystem(viewport), resources(renderSystem), sceneMgr(resources)
{
	viewport.listeners.push_back(this);
}

ApplicationCore::~ApplicationCore()
{
	renderSystem.core.WaitForAllFrames();

	delete compositor;
	delete shadowMap;
}

void ApplicationCore::initialize(const TargetWindow& window, const InitParams& appParams)
{
	renderSystem.core.initializeSwapChain(window);

	Vector3(-1, -1, -1).Normalize(lights.directionalLight.direction);

	shadowMap = new ShadowMaps(lights.directionalLight, params.sun);
	shadowMap->init(renderSystem, resources);

	compositor = new FrameCompositor({ params, renderSystem, resources }, sceneMgr, *shadowMap);
	compositor->registerTask("RenderPhysics", [this](RenderProvider& provider, SceneManager& sceneMgr)
	{
		return std::make_shared<PhysicsRenderTask>(provider, sceneMgr, physicsMgr);
	});
	compositor->load(appParams.defaultCompositor, appParams.defaultCompositorParams);

	sceneMgr.initialize(renderSystem);

	physicsMgr.init();

	FileLogger::log("ApplicationCore Initialized");
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

			if (timeSinceLastFrame > 1.f)
				timeSinceLastFrame = 1.f;

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

	auto result = SceneParser::load(scene, { sceneMgr, renderSystem, resources, physicsMgr });

	auto commands = renderSystem.core.CreateCommandList(L"loadSceneCmd");
	{
		auto marker = renderSystem.core.StartCommandList(commands);

		//initialize gpu resources
		{
			shadowMap->clear(commands.commandList);
		}

		for (const auto& i : result.instanceDescriptions)
		{
			sceneMgr.instancing.build(sceneMgr, i.second);
		}

		for (const auto& g : result.grassTasks)
		{
			sceneMgr.grass.scheduleGrassCreation(g, commands.commandList, params, resources, sceneMgr);
		}
	}

	VoxelizeSceneTask::Get().clear(commands.commandList);

	sceneMgr.terrain.createTerrain(commands.commandList, sceneMgr, {});

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
