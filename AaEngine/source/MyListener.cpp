#include "MyListener.h"
#include "AaLogger.h"
#include "Directories.h"
#include "../../DirectX-Headers/include/directx/d3dx12.h"
#include "FreeCamera.h"
#include "SceneParser.h"
#include "AaModelResources.h"
#include "AaShaderLibrary.h"
#include "AaMaterialResources.h"
#include "FrameCompositor.h"
#include "GrassArea.h"
#include "TextureData.h"
#include "VoxelizeSceneTask.h"

MyListener::MyListener(RenderSystem* render)
{
	AaShaderLibrary::get().loadShaderReferences(SHADER_DIRECTORY);
	AaMaterialResources::get().loadMaterials(MATERIAL_DIRECTORY);

	sceneMgr = new SceneManager(render);
	grass = new GrassAreaGenerator();

 	renderSystem = render;
 	renderSystem->getWindow()->listeners.push_back(this);

	cameraMan = new FreeCamera();
	cameraMan->camera.setPosition(XMFLOAT3(0, 0, -14.f));
	auto window = renderSystem->getWindow();
	cameraMan->init(window->getWidth() / (float)window->getHeight());

	Vector3(-1, -1, -1).Normalize(lights.directionalLight.direction);

	shadowMap = new AaShadowMap(lights.directionalLight, params.sun);
	shadowMap->init(renderSystem);
	shadowMap->update(renderSystem->frameIndex);

	compositor = new FrameCompositor({ params, renderSystem }, *sceneMgr, *shadowMap);
	compositor->load("frame.compositor");

	grass->initializeGpuResources(renderSystem, sceneMgr->getQueueTargetFormats());

	debugWindow.init(renderSystem);
	debugWindow.state.DlssMode = (int)renderSystem->upscale.dlss.selectedMode();
	debugWindow.state.FsrMode = (int)renderSystem->upscale.fsr.selectedMode();

	loadScene("test");
}

MyListener::~MyListener()
{
	debugWindow.deinit();

	renderSystem->WaitForAllFrames();

	delete compositor;
 	delete shadowMap;
 	delete cameraMan;
	delete grass;
	delete sceneMgr;
}

size_t GetGpuMemoryUsage()
{
	static IDXGIFactory4* pFactory{};
	if (!pFactory)
		CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&pFactory);

	static IDXGIAdapter3* adapter{};
	if (!adapter)
		pFactory->EnumAdapters(0, reinterpret_cast<IDXGIAdapter**>(&adapter));

	DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
	adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);

	return videoMemoryInfo.CurrentUsage / 1024 / 1024;
}

bool MyListener::frameStarted(float timeSinceLastFrame)
{
	params.time += timeSinceLastFrame;
	params.timeDelta = timeSinceLastFrame;
	params.frameIndex = renderSystem->frameIndex;

	InputHandler::consumeInput(*this);

 	cameraMan->update(timeSinceLastFrame);

	if (debugWindow.state.changeScene)
	{
		loadScene(debugWindow.state.changeScene);
		debugWindow.state.changeScene = {};
	}
	if (debugWindow.state.reloadShaders)
	{
		renderSystem->WaitForAllFrames();
		AaMaterialResources::get().ReloadShaders();
		debugWindow.state.reloadShaders = false;
	}
	if (debugWindow.state.DlssMode != (int)renderSystem->upscale.dlss.selectedMode())
	{
		renderSystem->WaitForAllFrames();
		renderSystem->upscale.dlss.selectMode((UpscaleMode)debugWindow.state.DlssMode);
		compositor->reloadPasses();
		DescriptorManager::get().initializeSamplers(renderSystem->upscale.getMipLodBias());
	}
	if (debugWindow.state.FsrMode != (int)renderSystem->upscale.fsr.selectedMode())
	{
		renderSystem->WaitForAllFrames();
		renderSystem->upscale.fsr.selectMode((UpscaleMode)debugWindow.state.FsrMode);
		compositor->reloadPasses();
		DescriptorManager::get().initializeSamplers(renderSystem->upscale.getMipLodBias());
	}
	{
		debugWindow.state.vramUsage = GetGpuMemoryUsage();
	}
 	if (auto ent = sceneMgr->getEntity("Torus001"))
 	{
 		ent->roll(timeSinceLastFrame);
		ent->yaw(timeSinceLastFrame / 2.f);
		ent->pitch(timeSinceLastFrame / 3.f);
 	}
	if (auto ent = sceneMgr->getEntity("Suzanne"))
	{
		ent->yaw(timeSinceLastFrame);
		ent->setPosition({ cos(params.time / 2.f) * 5, ent->getPosition().y, ent->getPosition().z });
	}
	if (auto ent = sceneMgr->getEntity("MovingCube"))
	{
		ent->setPosition({ ent->getPosition().x, ent->getPosition().y, -130.f - cos(params.time * 3) * 20 });
	}
	//Vector3(cos(params.time), -1, sin(params.time)).Normalize(lights.directionalLight.direction);

	sceneMgr->update();
	cameraMan->camera.updateMatrix();
	shadowMap->update(renderSystem->frameIndex);

	if (renderSystem->upscale.enabled())
	{
		// Assuming view and viewPrevious are pointers to View instances
		XMMATRIX viewMatrixCurrent = cameraMan->camera.getViewMatrix();
		static XMMATRIX viewMatrixPrevious = viewMatrixCurrent;
		XMMATRIX projMatrixCurrent = cameraMan->camera.getProjectionMatrixNoOffset();
		static XMMATRIX projMatrixPrevious = projMatrixCurrent;

		// Calculate the view reprojection matrix
		XMMATRIX viewReprojectionMatrix = XMMatrixMultiply(XMMatrixInverse(nullptr, viewMatrixCurrent), viewMatrixPrevious);

		// Calculate the reprojection matrix for TAA
		XMMATRIX reprojectionMatrix = XMMatrixMultiply(XMMatrixMultiply(XMMatrixInverse(nullptr, projMatrixCurrent), viewReprojectionMatrix), projMatrixPrevious);

		viewMatrixPrevious = viewMatrixCurrent;
		projMatrixPrevious = projMatrixCurrent;

		XMFLOAT4X4 reprojectionData;
		XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&reprojectionData, XMMatrixTranspose(reprojectionMatrix));
		AaMaterialResources::get().getMaterial("MotionVectors")->SetParameter("reprojectionMatrix", &reprojectionData._11, 16);
	}

	RenderContext ctx = { &cameraMan->camera };
	compositor->render(ctx);

	Sleep(10);

	return continueRendering;
}

bool MyListener::keyPressed(int key)
{
	switch (key)
	{
	case VK_ESCAPE:
		continueRendering = false;
		break;
	default:
		break;
	}

	return cameraMan->keyPressed(key);
}

bool MyListener::keyReleased(int key)
{
	return cameraMan->keyReleased(key);
}

bool MyListener::mouseMoved(int x, int y)
{
	return cameraMan->mouseMoved(x, y);
}

void MyListener::onScreenResize()
{
	auto window = renderSystem->getWindow();
	cameraMan->init(window->getWidth() / (float)window->getHeight());

	compositor->reloadTextures();
}

void MyListener::loadScene(const char* scene)
{
	if (scene == std::string_view("voxelRoom"))
		VoxelSceneSettings::get() = { Vector3(0, 0, 0), Vector3(30, 30, 30) };
	else
		VoxelSceneSettings::get() = {};

	renderSystem->WaitForAllFrames();
	grass->clear();
	instancing.clear();
	sceneMgr->clear();
	AaModelResources::get().clear();

	auto result = SceneParser::load(scene, sceneMgr, renderSystem);

	auto commands = renderSystem->CreateCommandList(L"initCmd");
	{
		auto marker = renderSystem->StartCommandList(commands);

		//initialize gpu resources
		{
			shadowMap->clear(commands.commandList);
		}

		for (const auto& i : result.instanceDescriptions)
		{
			instancing.build(sceneMgr, i.second);
		}

		for (const auto& g : result.grassTasks)
		{
			grass->scheduleGrassCreation(g, commands.commandList, params, sceneMgr);
		}
	}
	renderSystem->ExecuteCommandList(commands);
	renderSystem->WaitForCurrentFrame();
	commands.deinit();

	grass->finishGrassCreation();
}

bool MyListener::mouseWheel(float change)
{
	return cameraMan->mouseWheel(change);
}

bool MyListener::mousePressed(MouseButton button)
{
	return cameraMan->mousePressed(button);
}

bool MyListener::mouseReleased(MouseButton button)
{
	return cameraMan->mouseReleased(button);
}
