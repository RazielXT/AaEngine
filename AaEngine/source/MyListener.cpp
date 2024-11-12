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
	AaShaderLibrary::get().loadShaderReferences(SHADER_DIRECTORY, false);
	AaMaterialResources::get().loadMaterials(MATERIAL_DIRECTORY, false);

	sceneMgr = new AaSceneManager(render);

 	renderSystem = render;
 	renderSystem->getWindow()->listeners.push_back(this);

	cameraMan = new FreeCamera();
	cameraMan->camera.setPosition(XMFLOAT3(0, 0, -14.f));
	auto window = renderSystem->getWindow();
	cameraMan->reload(window->getWidth() / (float)window->getHeight());

	shadowMap = new AaShadowMap(lights.directionalLight);
	shadowMap->init(renderSystem);

	compositor = new FrameCompositor({ gpuParams, render }, *sceneMgr, *shadowMap);
	compositor->load(DATA_DIRECTORY + "frame.compositor");

	Vector3(-1, -1, -1).Normalize(lights.directionalLight.direction);

 	SceneParser::load("test", sceneMgr, renderSystem);

	if (true)
	{
		tmpQueue = sceneMgr->createManualQueue();
		auto terrainTarget = sceneMgr->getEntity("Plane001");
		tmpQueue.update({ { EntityChange::Add, terrainTarget } });

		heap.Init(renderSystem->device, tmpQueue.targets.size(), 1, L"tempHeap");
		tmp.Init(renderSystem->device, 512, 512, 1, heap, tmpQueue.targets, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		tmp.SetName(L"tmpTex");

		ResourcesManager::get().createShaderResourceView(tmp);
		ResourcesManager::get().createDepthShaderResourceView(tmp);

		auto bbox = terrainTarget->getBoundingBox();

		AaCamera tmpCamera;
		tmpCamera.setOrthographicCamera(bbox.Extents.x * 2, bbox.Extents.z * 2, 1, bbox.Extents.y * 2 + 1);
		tmpCamera.setPosition(bbox.Center + terrainTarget->getPosition() + Vector3(0, bbox.Extents.y + 1 ,0));
		tmpCamera.setDirection({ 0, -1, 0 });
		tmpCamera.updateMatrix();

		RenderInformation objInfo{ sceneMgr->getRenderables(Order::Normal) };
		objInfo.source->updateTransformation();
		objInfo.updateVisibility(tmpCamera);

		auto commands = renderSystem->CreateCommandList(L"tempCmd");
		renderSystem->StartCommandList(commands);

		shadowMap->update(renderSystem->frameIndex);
		shadowMap->clear(commands.commandList, renderSystem->frameIndex);

		tmp.PrepareAsTarget(commands.commandList, 0, D3D12_RESOURCE_STATE_RENDER_TARGET);

		tmpQueue.renderObjects(tmpCamera, objInfo, gpuParams, commands.commandList, 0);

		tmp.PrepareAsView(commands.commandList, 0, D3D12_RESOURCE_STATE_RENDER_TARGET);
		tmp.TransitionDepth(commands.commandList, 0, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		auto grassEntity = sceneMgr->getEntity("grass_Plane001");
		{
			sceneMgr->grass.bakeTerrain(*grassEntity->grass, terrainTarget, XMMatrixInverse(nullptr, tmpCamera.getProjectionMatrix()), commands.commandList, 0);

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(grassEntity->grass->vertexCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
			commands.commandList->ResourceBarrier(1, &barrier);
			commands.commandList->CopyResource(grassEntity->grass->vertexCounterRead.Get(), grassEntity->grass->vertexCounter.Get());
		}

		renderSystem->ExecuteCommandList(commands);

		renderSystem->WaitForCurrentFrame();
		
		{
			uint32_t* pCounterValue = nullptr;
			CD3DX12_RANGE readbackRange(0, sizeof(uint32_t));
			grassEntity->grass->vertexCounterRead->Map(0, &readbackRange, reinterpret_cast<void**>(&pCounterValue));
			grassEntity->grass->count = *pCounterValue;
			grassEntity->grass->vertexCount = grassEntity->grass->count * 3;
			grassEntity->geometry.fromGrass(*grassEntity->grass);
			grassEntity->grass->vertexCounterRead->Unmap(0, nullptr);
		}
		commands.deinit();

		std::vector<UCHAR> textureData;
		SaveTextureToMemory(renderSystem->commandQueue, tmp.textures.front().texture[0].Get(), textureData, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		SaveBMP(textureData, 512, 512, "Test.bmp");
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
	sceneMgr->updateTransformations();
	cameraMan->camera.updateMatrix();
	shadowMap->update(renderSystem->frameIndex);

	gpuParams.time = elapsedTime;
	gpuParams.timeDelta = timeSinceLastFrame;
	gpuParams.sun = shadowMap->data;

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
	cameraMan->reload(window->getWidth() / (float)window->getHeight());

	compositor->reloadTextures();
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
