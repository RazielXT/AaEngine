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

	Vector3(-1, -1, -1).Normalize(lights.directionalLight.direction);

	shadowMap = new AaShadowMap(lights.directionalLight);
	shadowMap->init(renderSystem);
	shadowMap->update(renderSystem->frameIndex);

	compositor = new FrameCompositor({ gpuParams, render }, *sceneMgr, *shadowMap);
	compositor->load(DATA_DIRECTORY + "frame.compositor");

	debugWindow.init(renderSystem);

	loadScene("test");

	std::vector<GrassAreaPlacementTask> grassTasks = {
		{
			sceneMgr->getEntity("Plane001"), sceneMgr->getEntity("Plane001")->getWorldBoundingBox()
		},
		{
			sceneMgr->getEntity("Box012"), sceneMgr->getEntity("Box012")->getWorldBoundingBox()
		}
	};

	std::vector<std::pair<AaEntity*, GrassArea*>> output;

	auto queue = sceneMgr->createManualQueue();

	RenderTargetHeap heap;
	heap.Init(renderSystem->device, queue.targets.size(), 1, L"tempRttHeap");
	RenderTargetTexture rtt;
	rtt.Init(renderSystem->device, 512, 512, 1, heap, queue.targets, D3D12_RESOURCE_STATE_RENDER_TARGET);
	rtt.SetName(L"tmpRttTex");

	DescriptorManager::get().createTextureView(rtt);
	DescriptorManager::get().createDepthView(rtt);

	auto commands = renderSystem->CreateCommandList(L"tempCmd");
	renderSystem->StartCommandList(commands);

	[&output, &queue, &rtt](const std::vector<GrassAreaPlacementTask>& grassTasks, ID3D12GraphicsCommandList* commandList, const FrameParameters& params, AaSceneManager* sceneMgr, AaRenderSystem* renderSystem)
		{
			auto initGrass = [](GrassAreaPlacementTask grassTask, RenderQueue& queue, RenderTargetTexture& rtt, ID3D12GraphicsCommandList* commandList, const FrameParameters& params, AaSceneManager* sceneMgr)
				-> std::pair<AaEntity*, GrassArea*>
				{
					auto bbox = grassTask.bbox;

					AaCamera tmpCamera;
					const UINT NearClipDistance = 1;
					tmpCamera.setOrthographicCamera(bbox.Extents.x * 2, bbox.Extents.z * 2, 1, bbox.Extents.y * 2 + NearClipDistance);
					tmpCamera.setPosition(bbox.Center + Vector3(0, bbox.Extents.y + NearClipDistance, 0));
					tmpCamera.setDirection({ 0, -1, 0 });
					tmpCamera.updateMatrix();

					Renderables tmpRenderables;
					AaEntity entity(tmpRenderables, *grassTask.terrain);

					RenderInformation sceneInfo;
					tmpRenderables.updateRenderInformation(tmpCamera, sceneInfo);

					rtt.PrepareAsTarget(commandList, 0, D3D12_RESOURCE_STATE_RENDER_TARGET);

					ShaderConstantsProvider constants(sceneInfo, tmpCamera, rtt);
					queue.update({ { EntityChange::Add, Order::Normal, &entity } });
					queue.renderObjects(constants, params, commandList, 0);
					queue.update({ { EntityChange::DeleteAll } });

					rtt.PrepareAsView(commandList, 0, D3D12_RESOURCE_STATE_RENDER_TARGET);
					rtt.TransitionDepth(commandList, 0, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

					GrassAreaDescription desc;
					desc.initialize(bbox);
					
					auto createGrassEntity = [sceneMgr](const std::string& name, const GrassAreaDescription& desc)
						{
							auto material = AaMaterialResources::get().getMaterial("GrassLeaves");

							Order renderQueue = Order::Normal;
							if (material->IsTransparent())
								renderQueue = Order::Transparent;

							auto grassEntity = sceneMgr->createEntity(name, renderQueue);
							grassEntity->setBoundingBox(desc.bbox);
							grassEntity->material = material;

							return grassEntity;
						};
					auto grassEntity = createGrassEntity("grass_" + std::string(entity.name), desc);
					auto grassArea = sceneMgr->grass.createGrass(desc);

					{
						auto colorTex = rtt.textures[0].textureView.srvHeapIndex;
						auto depthTex = rtt.depthView.srvHeapIndex;

						sceneMgr->grass.initializeGrassBuffer(*grassArea, desc, XMMatrixInverse(nullptr, tmpCamera.getProjectionMatrix()), colorTex, depthTex, commandList, 0);

						CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(grassArea->vertexCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
						commandList->ResourceBarrier(1, &barrier);
						commandList->CopyResource(grassArea->vertexCounterRead.Get(), grassArea->vertexCounter.Get());
					}

					return { grassEntity, grassArea };
				};

			for (auto& grassTask : grassTasks)
			{
				output.push_back(initGrass(grassTask, queue, rtt, commandList, params, sceneMgr));
			}
		}
	(grassTasks, commands.commandList, gpuParams, sceneMgr, renderSystem);

	renderSystem->ExecuteCommandList(commands);
	renderSystem->WaitForCurrentFrame();
	commands.deinit();

	[&output]()
		{
			for (auto [entity, grass] : output)
			{
				uint32_t* pCounterValue = nullptr;
				CD3DX12_RANGE readbackRange(0, sizeof(uint32_t));
				grass->vertexCounterRead->Map(0, &readbackRange, reinterpret_cast<void**>(&pCounterValue));
				grass->vertexCount = *pCounterValue * 3;
				grass->vertexCounterRead->Unmap(0, nullptr);

				entity->geometry.fromGrass(*grass);

// 			std::vector<UCHAR> textureData;
// 			SaveTextureToMemory(renderSystem->commandQueue, rtt.textures.front().texture[0].Get(), textureData, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
// 			SaveBMP(textureData, 512, 512, "Test.bmp");
			}
		}();

	DescriptorManager::get().removeDepthView(rtt);
	DescriptorManager::get().removeTextureView(rtt);
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
	cameraMan->reload(window->getWidth() / (float)window->getHeight());

	compositor->reloadTextures();
}

void MyListener::initializeGpuResources()
{
	auto commands = renderSystem->CreateCommandList(L"initCmd");
	renderSystem->StartCommandList(commands);

	shadowMap->clear(commands.commandList, renderSystem->frameIndex);

	renderSystem->ExecuteCommandList(commands);
	renderSystem->WaitForCurrentFrame();

	commands.deinit();
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
	sceneMgr->clear();
	AaModelResources::get().clear();

	auto result = SceneParser::load(scene, sceneMgr, renderSystem);
	updateFrameParams(0);
	initializeGpuResources();

	for (const auto& i : result.instanceDescriptions)
	{
		sceneMgr->instancing.build(sceneMgr, i.second);
	}

	for (const auto& g : result.grassTask)
	{
	}
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
