#include "ApplicationCore.h"
#include "Utils/Logger.h"
#include "App/Directories.h"
#include "SceneParser.h"
#include "Resources/Model/ModelResources.h"
#include "Resources/Shader/ShaderLibrary.h"
#include "Resources/Material/MaterialResources.h"
#include "RenderCore/TextureData.h"
#include "FrameCompositor/FrameCompositor.h"
#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "RenderObject/Terrain/TerrainGridParams.h"
#include "Physics/Render/PhysicsRenderTask.h"
#include "Objects/SplineConstruction.h"
#include <dxgidebug.h>
#include <filesystem>
#include "SceneGraph/Scene.h"

ApplicationCore::ApplicationCore(TargetViewport& viewport) : renderSystem(viewport), resources(renderSystem), renderWorld(resources)
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

	shadowMap = new ShadowMaps(renderSystem, lights.directionalLight, params.sky);
	shadowMap->init();

	{
		ResourceUploadBatch batch(renderSystem.core.device);
		batch.Begin();

		sky.initializeSkyParameters(params.sky, renderSystem.core.device, resources, batch);

		auto uploadResourcesFinished = batch.End(renderSystem.core.commandQueue);
		uploadResourcesFinished.wait();
	}

	compositor = new FrameCompositor(appParams.compositor, { params, renderSystem, resources }, renderWorld, *shadowMap);
	compositor->setColorSpace(colorSpace);
	compositor->registerTask("RenderPhysics", [this](RenderProvider& provider, RenderWorld& renderWorld)
	{
		return std::make_shared<PhysicsRenderTask>(provider, renderWorld, physicsMgr);
	});
	compositor->reloadPasses();

	renderWorld.initialize(renderSystem);

	physicsMgr.init();
	terrainPhysics.init(renderSystem.core.device);

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
	params.frameCounter++;

	terrainPhysics.consumeReadbacks(camera.getPosition(), renderWorld.terrain.params, physicsMgr);
	waterInteraction.update(timeSinceLastFrame, physicsMgr, renderWorld.water);

	renderWorld.update();
	camera.updateMatrix();
	shadowMap->update(renderSystem.core.frameIndex, camera);

	sky.updateSkyParameters(params.sky, renderSystem.core.frameIndex);

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

void ApplicationCore::loadScene()
{
	renderSystem.core.WaitForAllFrames();
	renderWorld.clear();
	terrainPhysics.clear(physicsMgr);
	physicsMgr.init();
	terrainPhysics.init(renderSystem.core.device);
	resources.models.clear();

	ResourceUploadBatch batch(renderSystem.core.device);
	batch.Begin();

	sky.createSky(renderWorld, resources.materials, batch);
	sky.createClouds(renderWorld, resources.materials, renderSystem.core.device, batch);
	sky.createMoon(renderWorld, resources.materials, batch);

	auto commands = renderSystem.core.CreateCommandList(L"loadScene", PixColor::Load);
	{
		auto marker = renderSystem.core.StartCommandList(commands);

		//initialize gpu resources
		{
			shadowMap->clear(commands.commandList);
		}

		marker.move("loadSceneVoxels", commands.color);
		VoxelizeSceneTask::Get().clear(commands.commandList);

 		marker.move("loadSceneTerrain", commands.color);
		renderWorld.water.initializeGpuResources(renderSystem, resources, batch);
		renderWorld.terrain.initialize(renderSystem, resources, batch, renderWorld);
		renderWorld.vegetation.createChunks(renderWorld, renderSystem, resources, batch);
		renderWorld.grass.createChunks(renderWorld, renderSystem, resources, batch);
		renderWorld.water.initializeTarget(renderWorld.terrain.getHeightmap({ 0,0 }), renderWorld, { renderWorld.terrain.params.tileSize, renderWorld.terrain.params.tileSize }, renderWorld.terrain.getHeightmapPosition({ 0,0 }));
		initializeSplineConstructionPreview(batch);
	}
	renderWorld.terrain.postUpdateCallback = [this](ID3D12GraphicsCommandList* commandList, ProgressiveTerrain& terrain)
	{
		terrainPhysics.requestReadbacks(commandList, terrain);
	};

	auto uploadResourcesFinished = batch.End(renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();

	renderSystem.core.ExecuteCommandList(commands);
	renderSystem.core.WaitForCurrentFrame();
	commands.deinit();

	struct MeshRendererComponent
	{
		int idx = 0;
	};

	class RenderSystem
	{
	public:

		RenderSystem(Scene& scene)
		{
			scene.Events<MeshRendererComponent>().onAdded = 
				[&](Entity e, MeshRendererComponent& c)
				{
					c.idx++;
				};
			scene.Events<MeshRendererComponent>().onRemoved =
				[&](Entity e, MeshRendererComponent& c)
				{
					c.idx--;
				};
		}
	};

	Scene scene;
	RenderSystem rs(scene);

	auto entity = scene.CreateEntity();
	scene.AddComponent(entity, MeshRendererComponent{ -10 });
	scene.DestroyEntity(entity);
}

void ApplicationCore::initializeSplineConstructionPreview(DirectX::ResourceUploadBatch& batch)
{
	splineConstructionPreviews.clear();

	auto addPreview = [this, &batch](ShapeProfile2D profile, const std::vector<SplinePoint>& points, Vector3 color, bool physics = true)
	{
		auto construction = std::make_unique<SplineConstruction>();
		construction->spline.setTessellationSegments(8);
		construction->spline.setAdaptiveTessellation(6.0f, 24);
		for (const SplinePoint& point : points)
			construction->spline.addPoint(point.position, point.roll);

		construction->profile = std::move(profile);
		construction->sweepSettings.pathUvScale = 0.25f;
		construction->sweepSettings.profileUvScale = 0.25f;

		SplineConstructionCreateParams params;
		params.materialName = "General";
		params.createMeshPhysics = physics;
		construction->regenerate(renderSystem, resources, renderWorld, &physicsMgr, batch, params);

		color *= 0.8f;

		if (construction->entity)
			construction->entity->Material().setParam("MaterialColor", color * color);

		splineConstructionPreviews.push_back(std::move(construction));
	};

	addPreview(ShapeProfile2D::createRoad(5.0f),
	{
		{ { -44.0f, 6.0f, -24.0f }, 0.0f },
		{ { -28.0f, 6.5f, -12.0f }, 0.15f },
		{ { -16.0f, 6.0f, 6.0f }, -0.2f },
		{ { -4.0f, 7.0f, 20.0f }, 0.25f },
	}, { 0.35f, 0.35f, 0.35f });

	addPreview(ShapeProfile2D::createRectangle(4.0f, 1.0f),
	{
		{ { -38.0f, 11.0f, 26.0f }, 0.0f },
		{ { -22.0f, 12.0f, 34.0f }, 0.35f },
		{ { -6.0f, 12.0f, 30.0f }, -0.25f },
	}, { 0.8f, 0.55f, 0.25f });

	addPreview(ShapeProfile2D::createCircle(2.0f, 32),
	{
		{ { 4.0f, 9.0f, -26.0f }, 0.0f },
		{ { 16.0f, 11.0f, -16.0f }, 0.4f },
		{ { 28.0f, 9.0f, -26.0f }, -0.35f },
	}, { 0.7f, 0.8f, 0.95f });

	addPreview(ShapeProfile2D::createTube(3.0f, 2.35f, 32),
	{
		{ { 2.0f, 10.0f, 8.0f }, 0.0f },
		{ { 16.0f, 13.0f, 20.0f }, 0.45f },
		{ { 32.0f, 10.0f, 10.0f }, -0.35f },
		{ { 44.0f, 12.0f, 26.0f }, 0.7f },
	}, { 0.55f, 0.75f, 1.0f });

	addPreview(ShapeProfile2D::createArc(3.0f, 3.14159265f, 24),
	{
		{ { -8.0f, 14.0f, -38.0f }, 0.0f },
		{ { 10.0f, 17.0f, -44.0f }, 0.5f },
		{ { 30.0f, 15.0f, -38.0f }, -0.45f },
	}, { 0.65f, 0.9f, 0.55f });

	addPreview(ShapeProfile2D::createFilledArc(3.0f, 3.14159265f, 24),
	{
		{ { -8.0f, 20.0f, -54.0f }, 0.0f },
		{ { 10.0f, 22.0f, -60.0f }, -0.35f },
		{ { 30.0f, 20.0f, -54.0f }, 0.4f },
	}, { 0.9f, 0.85f, 0.45f });

	addPreview(ShapeProfile2D::createTube(3.5f, 2.75f, 40),
	{
		{ { -62.0f, 14.0f, -8.0f }, 0.0f },
		{ { -48.0f, 28.0f, 14.0f }, 1.1f },
		{ { -34.0f, 46.0f, -4.0f }, 2.5f },
		{ { -20.0f, 28.0f, -22.0f }, 4.2f },
		{ { -6.0f, 12.0f, 0.0f }, 6.0f },
		{ { 18.0f, 20.0f, 18.0f }, 6.7f },
		{ { 44.0f, 16.0f, -4.0f }, 7.4f },
	}, { 0.95f, 0.45f, 0.65f });
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
