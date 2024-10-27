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

	compositor = new FrameCompositor(render, sceneMgr, shadowMap);
	compositor->load(DATA_DIRECTORY + "frame.compositor");

	Vector3(-1, -1, -1).Normalize(lights.directionalLight.direction);

 	SceneParser::load("test", sceneMgr, renderSystem);

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

float elapsedTime = 0;

bool MyListener::frameStarted(float timeSinceLastFrame)
{
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

 	elapsedTime += timeSinceLastFrame;

	//Vector3(cos(elapsedTime), -1, sin(elapsedTime)).Normalize(sceneMgr->lights.directionalLight.direction);

	if (debugWindow.state.reloadShaders)
	{
		renderSystem->WaitForAllFrames();
		AaMaterialResources::get().ReloadShaders();
		debugWindow.state.reloadShaders = false;
	}

	sceneMgr->updateQueues();
	shadowMap->update(renderSystem->frameIndex);

	RenderContext ctx = { &cameraMan->camera, renderSystem, &sceneMgr->renderables };

	ctx.params.time = elapsedTime;
	ctx.params.timeDelta = timeSinceLastFrame;
	ctx.params.sunDirection = lights.directionalLight.direction;
	XMStoreFloat4x4(&ctx.params.shadowMapViewProjectionTransposed, XMMatrixTranspose(shadowMap->camera[0].getViewProjectionMatrix()));

	compositor->render(ctx, debugWindow);

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
