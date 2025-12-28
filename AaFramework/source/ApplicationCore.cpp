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
#include <filesystem>

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
	if (!std::filesystem::exists(DATA_DIRECTORY) || !std::filesystem::is_directory(DATA_DIRECTORY))
		FileLogger::logError("Missing directory " + std::filesystem::absolute(DATA_DIRECTORY).string());

	renderSystem.core.initializeSwapChain(window);

	Vector3(-1, -1, -1).Normalize(lights.directionalLight.direction);

	shadowMap = new ShadowMaps(lights.directionalLight, params.sun);
	shadowMap->init(renderSystem, resources);

	compositor = new FrameCompositor(appParams.compositor, { params, renderSystem, resources }, sceneMgr, *shadowMap);
	compositor->registerTask("RenderPhysics", [this](RenderProvider& provider, SceneManager& sceneMgr)
	{
		return std::make_shared<PhysicsRenderTask>(provider, sceneMgr, physicsMgr);
	});
	compositor->reloadPasses();

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

	SceneParser::Result result;
	{
		GlobalQueueMarker marker(renderSystem.core.commandQueue, "SceneParser");

		result = SceneParser::load(scene, { batch, sceneMgr, renderSystem, resources, physicsMgr });

// 		planes.CreatePlanesVertexBuffer(renderSystem, batch, { { {0,-1400,-2000}, 4000 }, { {0,-1400,2000}, 4000 } });
// 		auto e = sceneMgr.createEntity("testPlane", Order::Transparent);
// 		e->material = resources.materials.getMaterial("WaterLake", batch);
// 		planes.AssignToEntity(e);

// 		auto e = sceneMgr.createEntity("pyramid", { {}, {0, -500, 0 }, { 20,20,20 } }, *resources.models.getLoadedModel("pyramid.mesh", ResourceGroup::Core));
// 		e->material = resources.materials.getMaterial("RedVCT", batch);
// 		result.grassTasks.push_back({ e, e->getWorldBoundingBox() });

// 		{
// 			auto material = resources.materials.getMaterial("grassBladeInstanced", batch);
// 			auto model = resources.models.getModel("GrassBladeMedium.mesh", batch, {ResourceGroup::General, SCENE_DIRECTORY + "tmp/" });
// 
// 			auto& instanceDescription = result.instanceDescriptions[material];
// 			instanceDescription.material = material;
// 			instanceDescription.model = model;
// 
// 			for (float x = 0; x < 50; x++)
// 				for (float z = 0; z < 50; z++)
// 				{
// 					float distance = std::sqrt(x * x + z * z); // Distance from origin (0,0)
// 					float densityFactor = 1.0f + distance * 0.04f; // Increase spacing with distance
// 
// 					float GrassOffset = 3.0f * densityFactor;
// 					float GrassRandomOffset = GrassOffset * 0.7f;
// 
// 					auto& tr = instanceDescription.objects.emplace_back();
// 					tr.position = {
// 						x * GrassOffset + getRandomFloat(-GrassRandomOffset, GrassRandomOffset),
// 						0,
// 						z * GrassOffset + getRandomFloat(-GrassRandomOffset, GrassRandomOffset)
// 					};
// 					tr.scale = Vector3(getRandomFloat(0.4f, 1.5f));
// 					tr.orientation = Quaternion::CreateFromAxisAngle(Vector3::UnitY, getRandomFloat(0, XM_2PI));
// 				}
// 
// 			planes.CreatePlanesVertexBuffer(renderSystem, batch, { { {0,0,250}, 500 }, { {500,0,250}, 500 } }, 0.01f, true);
// 			planes.CreateEntity("testPlane", sceneMgr, resources.materials.getMaterial("terrainGrass", batch));
// 		}
	}

	auto commands = renderSystem.core.CreateCommandList(L"loadScene", PixColor::Load);
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

		marker.move("loadSceneVoxels", commands.color);
		VoxelizeSceneTask::Get().clear(commands.commandList);

 		marker.move("loadSceneTerrain", commands.color);
// 		sceneMgr.terrain.createTerrain(commands.commandList, renderSystem, sceneMgr, resources, batch);
		sceneMgr.water.initializeGpuResources(renderSystem, resources, batch, sceneMgr);
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
