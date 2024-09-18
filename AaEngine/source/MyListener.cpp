#include "MyListener.h"
#include "AaLogger.h"
#include "Directories.h"
#include "AaSceneNode.h"
#include "AaSceneParser.h"
#include "imgui.h"
#include "OgreMeshFileParser.h"
#include "Math.h"

bool zp = false, zm = false, xp = false, xm = false;

MyListener::MyListener(AaSceneManager* mSceneMgr)
{
	this->mSceneMgr = mSceneMgr;

	cameraMan = new FreeCamera();
	cameraMan->camera.setPosition(XMFLOAT3(0, 0, -14.f));

	mRS = mSceneMgr->getRenderSystem();
	mRS->getWindow()->listeners.push_back(this);

	voxelScene = new AaVoxelScene(mSceneMgr);
	voxelScene->initScene(128);

	mShadowMapping = new AaShadowMapping(mSceneMgr);

	AaShaderManager::get().loadShaderReferences(SHADER_DIRECTORY, false);
	AaMaterialResources::get().loadMaterials(MATERIAL_DIRECTORY, false);

	pp = new AaBloomPostProcess(mSceneMgr);

	mLights.directionalLight.type = LightType_Directional;
	mLights.directionalLight.color = XMFLOAT3(1, 1, 1);
	Vector3(0.5f, -1, 1.6).Normalize(mLights.directionalLight.direction);

	SceneParser::load("test.scene", mSceneMgr);

	debugWindow.init(mSceneMgr, mRS);
}

MyListener::~MyListener()
{
	debugWindow.deinit();

	delete voxelScene;
	delete pp;
	delete mShadowMapping;
	delete cameraMan;
}

float elapsedTime = 0;
int count = 0;

bool MyListener::frameStarted(float timeSinceLastFrame)
{
	InputHandler::consumeInput(*this);

	if (auto ent = mSceneMgr->getEntity("Suzanne"))
	{
		ent->yaw(timeSinceLastFrame);
		ent->setPosition(cos(elapsedTime) * 5, ent->getPosition().y, ent->getPosition().z);
	}

	cameraMan->update(timeSinceLastFrame);

	elapsedTime += timeSinceLastFrame * 0.3f;

	//Vector3(cos(elapsedTime) / 2.f, -1, sin(elapsedTime) / 2.f).Normalize(mLights.directionalLight.direction);

	mShadowMapping->updateShadowCamera(mLights.directionalLight.direction);
	mShadowMapping->renderShadowMaps();

	//if (count % 2 == 0)
	voxelScene->voxelizeScene(XMFLOAT3(30, 30, 30), XMFLOAT3(0, 0, 0));
	voxelScene->endFrame(XMFLOAT3(30, 30, 30), XMFLOAT3(0, 0, 0));
	count++;

	AaShaderManager::get().buffers.updatePerFrameConstants(timeSinceLastFrame, cameraMan->camera, mShadowMapping->shadowCamera, mLights);

	mRS->setBackbufferAsRenderTarget();
	mRS->clearViews();

	pp->render(cameraMan->camera);

	debugWindow.draw();

	mRS->swapChain_->Present(1, 0);

	return continue_rendering;
}

bool MyListener::keyPressed(int key)
{
	switch (key)
	{
	case VK_ESCAPE:
		continue_rendering = false;
		break;

	case 'I':
		xp = true;
		break;

	case 'K':
		xm = true;
		break;

	case 'J':
		zp = true;
		break;

	case 'L':
		zm = true;
		break;

	case 'M':
	{
		auto ent = mSceneMgr->getEntity("Box03");
		ent->visible = !ent->visible;
		break;
	}
	default:
		break;
	}

	return cameraMan->keyPressed(key);
}

bool MyListener::keyReleased(int key)
{
	switch (key)
	{
	case 'I':
		xp = false;
		break;

	case 'K':
		xm = false;
		break;

	case 'J':
		zp = false;
		break;

	case 'L':
		zm = false;
		break;
	}

	return cameraMan->keyReleased(key);
}

bool MyListener::mouseMoved(int x, int y)
{
	return cameraMan->mouseMoved(x, y);
}

void MyListener::onScreenResize()
{
	delete pp;
	pp = new AaBloomPostProcess(mSceneMgr);

	auto window = mRS->getWindow();
	cameraMan->camera.setPerspectiveCamera(70, window->getWidth() / (float)window->getHeight(), 0.01, 1000);
}

bool MyListener::mousePressed(MouseButton button)
{
	return cameraMan->mousePressed(button);
}

bool MyListener::mouseReleased(MouseButton button)
{
	return cameraMan->mouseReleased(button);
}
