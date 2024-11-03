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

MyListener::MyListener(AaRenderSystem* render)
{
	sceneMgr = new AaSceneManager(render);

 	renderSystem = render;
 	renderSystem->getWindow()->listeners.push_back(this);

	cameraMan = new FreeCamera();
	cameraMan->camera.setPosition(XMFLOAT3(0, 0, -14.f));
	auto window = renderSystem->getWindow();
	cameraMan->reload(window->getWidth() / (float)window->getHeight());

	shadowMap = new AaShadowMap(lights.directionalLight);
	shadowMap->init(renderSystem);

	AaShaderLibrary::get().loadShaderReferences(SHADER_DIRECTORY, false);
 	AaMaterialResources::get().loadMaterials(MATERIAL_DIRECTORY, false);

	compositor = new FrameCompositor({ gpuParams, render }, sceneMgr, shadowMap);
	compositor->load(DATA_DIRECTORY + "frame.compositor");

	Vector3(-1, -1, -1).Normalize(lights.directionalLight.direction);

 	SceneParser::load("test", sceneMgr, renderSystem);

	{
		tmpQueue = sceneMgr->createManualQueue();
		tmpQueue.update({ { EntityChange::Add, sceneMgr->getEntity("Plane001") } });
		tmpQueue.update({ { EntityChange::Add, sceneMgr->getEntity("Torus001") } });
		heap.Init(renderSystem->device, tmpQueue.targets.size(), 1, L"tempHeap");
		tmp.Init(renderSystem->device, 512, 512, 1, heap, tmpQueue.targets, true);
		tmp.SetName(L"tmpTex");

		ResourcesManager::get().createShaderResourceView(tmp);

		AaCamera tmpCamera;
		tmpCamera.setOrthographicCamera(100, 100, 1, 300);
		tmpCamera.setPosition({ 81, 100, -72 });
		tmpCamera.setDirection({ 0, -1, 0 });

		sceneMgr->renderables.updateTransformation();

		RenderInformation objInfo;
		sceneMgr->renderables.updateRenderInformation(tmpCamera, objInfo);

		auto commands = renderSystem->CreateCommandList(L"tempCmd");

		renderSystem->StartCommandList(commands);

		tmp.PrepareAsTarget(commands.commandList, 0);

		tmpQueue.renderObjects(tmpCamera, objInfo, gpuParams, commands.commandList, 0);

		tmp.PrepareAsView(commands.commandList, 0);

		renderSystem->ExecuteCommandList(commands);
	}

	debugWindow.init(renderSystem);
}

MyListener::~MyListener()
{
	debugWindow.deinit();

	renderSystem->WaitForAllFrames();

	delete compositor;
 	delete shadowMap;
 	delete cameraMan;
	delete sceneMgr;
}

bool MyListener::frameStarted(float timeSinceLastFrame)
{
	elapsedTime += timeSinceLastFrame;

	InputHandler::consumeInput(*this);

 	cameraMan->update(timeSinceLastFrame);

	if (debugWindow.state.changeScene)
	{
		renderSystem->WaitForAllFrames();
		sceneMgr->clear();
		AaModelResources::get().clear();
		SceneParser::load(debugWindow.state.changeScene, sceneMgr, renderSystem);
		debugWindow.state.changeScene = {};
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
		ent->setPosition({ cos(elapsedTime / 2.f) * 5, ent->getPosition().y, ent->getPosition().z });
	}

	//Vector3(cos(elapsedTime), -1, sin(elapsedTime)).Normalize(sceneMgr->lights.directionalLight.direction);

	if (debugWindow.state.reloadShaders)
	{
		renderSystem->WaitForAllFrames();
		AaMaterialResources::get().ReloadShaders();
		debugWindow.state.reloadShaders = false;
	}

	sceneMgr->updateQueues();
	sceneMgr->renderables.updateTransformation();

	shadowMap->update(renderSystem->frameIndex);

	gpuParams.time = elapsedTime;
	gpuParams.timeDelta = timeSinceLastFrame;
	gpuParams.sunDirection = lights.directionalLight.direction;
	XMStoreFloat4x4(&gpuParams.shadowMapViewProjectionTransposed, XMMatrixTranspose(shadowMap->camera[0].getViewProjectionMatrix()));

	RenderContext ctx = { &cameraMan->camera, &sceneMgr->renderables };
	compositor->render(ctx);

	Sleep(10);

	return continue_rendering;
}

bool MyListener::keyPressed(int key)
{
	switch (key)
	{
	case VK_ESCAPE:
		continue_rendering = false;
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
	cameraMan->reload(window->getWidth() / (float)window->getHeight());

	compositor->reloadTextures();
}

bool MyListener::mousePressed(MouseButton button)
{
	return cameraMan->mousePressed(button);
}

bool MyListener::mouseReleased(MouseButton button)
{
	return cameraMan->mouseReleased(button);
}
