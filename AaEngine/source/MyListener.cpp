#include "MyListener.h"
#include "AaLogger.h"
#include "Directories.h"
#include "../../DirectX-Headers/include/directx/d3dx12.h"
#include "FreeCamera.h"
#include "SceneParser.h"
#include "AaModelResources.h"
#include "AaShaderResources.h"
#include "AaMaterialResources.h"
#include "Compositor.h"

bool reloadShaders = false;

MyListener::MyListener(AaRenderSystem* render)
{
	sceneMgr = new AaSceneManager(render);

 	renderSystem = render;
 	renderSystem->getWindow()->listeners.push_back(this);

	cameraMan = new FreeCamera();
	cameraMan->camera.setPosition(XMFLOAT3(0, 0, -14.f));
	auto window = renderSystem->getWindow();
	cameraMan->reload(window->getWidth() / (float)window->getHeight());

	shadowMap = new AaShadowMap(sceneMgr->lights.directionalLight);
	shadowMap->init(renderSystem);

// 	voxelScene = new AaVoxelScene(mSceneMgr);
// 	voxelScene->initScene(128);

	AaShaderResources::get().loadShaderReferences(SHADER_DIRECTORY, false);
 	AaMaterialResources::get().loadMaterials(MATERIAL_DIRECTORY, false);

	compositor = new SceneCompositor(render);
	compositor->load(DATA_DIRECTORY + "postprocess.compositor");

	Vector3(-1, -1, -1).Normalize(sceneMgr->lights.directionalLight.direction);

 	SceneParser::load("test.scene", sceneMgr);

	debugWindow.init(renderSystem, &reloadShaders);
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
int count = 0;

bool MyListener::frameStarted(float timeSinceLastFrame)
{
	InputHandler::consumeInput(*this);

 	cameraMan->update(timeSinceLastFrame);

 	if (auto ent = sceneMgr->getEntity("Torus001"))
 	{
 		ent->roll(timeSinceLastFrame);
		ent->yaw(timeSinceLastFrame / 2.f);
		ent->pitch(timeSinceLastFrame / 3.f);
 	}

 	elapsedTime += timeSinceLastFrame;

	Vector3(cos(elapsedTime), -1, sin(elapsedTime)).Normalize(sceneMgr->lights.directionalLight.direction);

	if (reloadShaders)
	{
		renderSystem->WaitForAllFrames();
		AaMaterialResources::get().ReloadShaders();
		reloadShaders = false;
	}

	FrameGpuParameters params;
	params.time = elapsedTime;

	compositor->render(params, sceneMgr, cameraMan->camera, *shadowMap, debugWindow);

// 	//if (count % 2 == 0)
// 	voxelScene->voxelizeScene(XMFLOAT3(30, 30, 30), XMFLOAT3(0, 0, 0));
// 	voxelScene->endFrame(XMFLOAT3(30, 30, 30), XMFLOAT3(0, 0, 0));
// 	count++;

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

	compositor->reload();
}

bool MyListener::mousePressed(MouseButton button)
{
	return cameraMan->mousePressed(button);
}

bool MyListener::mouseReleased(MouseButton button)
{
	return cameraMan->mouseReleased(button);
}
