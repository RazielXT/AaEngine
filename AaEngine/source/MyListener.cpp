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

MyListener::MyListener(AaRenderSystem* render)
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

	shadowMap = new AaShadowMap(lights.directionalLight);
	shadowMap->init(renderSystem);
	shadowMap->update(renderSystem->frameIndex);

	compositor = new FrameCompositor({ gpuParams, renderSystem }, *sceneMgr, *shadowMap);
	compositor->load(DATA_DIRECTORY + "frame.compositor");

	grass->initializeGpuResources(renderSystem, sceneMgr->getQueueTargetFormats());

	debugWindow.init(renderSystem);

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

bool MyListener::frameStarted(float timeSinceLastFrame)
{
	elapsedTime += timeSinceLastFrame;

	InputHandler::consumeInput(*this);

 	cameraMan->update(timeSinceLastFrame);

	if (debugWindow.state.changeScene)
	{
		loadScene(debugWindow.state.changeScene);
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

	sceneMgr->update();
	cameraMan->camera.updateMatrix();
	shadowMap->update(renderSystem->frameIndex);
	updateFrameParams(timeSinceLastFrame);

	RenderContext ctx = { &cameraMan->camera };
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
	cameraMan->init(window->getWidth() / (float)window->getHeight());

	compositor->reloadTextures();
}

void MyListener::updateFrameParams(float tslf)
{
	gpuParams.time = elapsedTime;
	gpuParams.timeDelta = tslf;
	gpuParams.sun = shadowMap->data;
}

void MyListener::loadScene(const char* scene)
{
	renderSystem->WaitForAllFrames();
	grass->clear();
	instancing.clear();
	sceneMgr->clear();
	AaModelResources::get().clear();

	auto result = SceneParser::load(scene, sceneMgr, renderSystem);
	updateFrameParams(0);

	auto commands = renderSystem->CreateCommandList(L"initCmd");
	renderSystem->StartCommandList(commands);

	{
		shadowMap->clear(commands.commandList, renderSystem->frameIndex);
	}

	for (const auto& i : result.instanceDescriptions)
	{
		instancing.build(sceneMgr, i.second);
	}

	for (const auto& g : result.grassTasks)
	{
		grass->scheduleGrassCreation(g, commands.commandList, gpuParams, sceneMgr);
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
