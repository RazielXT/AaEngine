#include "Compositor.h"
#include "CompositorFileParser.h"
#include "AaMaterialResources.h"
#include "AaSceneManager.h"
#include "ShadowMap.h"
#include "DebugWindow.h"

Compositor::Compositor(AaRenderSystem* rs)
{
	renderSystem = rs;
}

void Compositor::load(std::string path)
{
	load(CompositorFileParser::parseFile(path));

	initializeTasks();
}

void Compositor::load(CompositorInfo i)
{
	info = i;

	for (auto& t : info.textures)
	{
		UINT w = t.width;
		UINT h = t.height;

		if (t.targetScale)
		{
			w = renderSystem->getWindow()->getWidth() * t.width;
			h = renderSystem->getWindow()->getHeight() * t.height;
		}

		RenderTargetTexture& tex = textures[t.name];
		tex.Init(renderSystem->device, w, h, renderSystem->FrameCount, t.format);

		AaMaterialResources::get().PrepareShaderResourceView(tex);
	}

	for (auto& p : info.passes)
	{
		PassData data{ p };
		
		for (auto& t : p.inputs)
		{
			data.inputs.push_back(&textures[t]);
		}

		if (!p.material.empty())
			data.material = AaMaterialResources::get().getMaterial(p.material)->Assign({});

		if (!p.target.empty())
		{
			if (p.target == "Backbuffer")
				data.target = &renderSystem->backbuffer;
			else
				data.target = &textures[p.target];
		}

		passes.emplace_back(std::move(data));
	}

	if (!passes.empty() && passes.back().target == &renderSystem->backbuffer)
	{
		passes.back().present = true;
	}
}

void Compositor::reload()
{
	passes.clear();
	textures.clear();

	load(info);
}

// Compositor::SceneCompositor(AaRenderSystem* rs) : Compositor(rs)
// {
// 	generalCommands = rs->CreateCommandList();
// 	sceneCommands = rs->CreateCommandList();
// 	earlyZCommands = rs->CreateCommandList();
// 	shadowsCommands = rs->CreateCommandList();
// 
// 	for (int i = 0; i < 3; i++)
// 	{
// 		workerBeginRenderFrame[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
// 		workerFinishPass[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
// 	}
// 	workers[Shadows] = std::thread([this]
// 		{
// 			while (running)
// 			{
// 				WaitForSingleObject(workerBeginRenderFrame[Shadows], INFINITE);
// 
// 				prepareShadowsCommandList();
// 
// 				SetEvent(workerFinishPass[Shadows]);
// 			}
// 		});
// 	workers[Scene] = std::thread([this]
// 		{
// 			while (running)
// 			{
// 				WaitForSingleObject(workerBeginRenderFrame[Scene], INFINITE);
// 
// 				prepareSceneCommandList();
// 
// 				SetEvent(workerFinishPass[Scene]);
// 			}
// 		});
// 	workers[EarlyZ] = std::thread([this]
// 		{
// 			while (running)
// 			{
// 				WaitForSingleObject(workerBeginRenderFrame[EarlyZ], INFINITE);
// 
// 				prepareEarlyZCommandList();
// 
// 				SetEvent(workerFinishPass[EarlyZ]);
// 			}
// 		});
// }

// SceneCompositor::~SceneCompositor()
// {
// 	running = false;
// 	SetEvent(workerBeginRenderFrame[Shadows]);
// 	SetEvent(workerBeginRenderFrame[EarlyZ]);
// 	SetEvent(workerBeginRenderFrame[Scene]);
// 	for (auto& t : workers)
// 	{
// 		t.join();
// 	}
// 
// 	auto releaseCommands = [](AaRenderSystem::CommandsData& data)
// 		{
// 			data.commandList->Release();
// 			for (auto a : data.commandAllocators)
// 			{
// 				a->Release();
// 			}
// 		};
// 
// 	releaseCommands(generalCommands);
// 	releaseCommands(sceneCommands);
// 	releaseCommands(earlyZCommands);
// 	releaseCommands(shadowsCommands);
// }

struct RenderContext
{
	AaSceneManager* sceneMgr;
	AaCamera* camera;
	FrameGpuParameters* params;
	AaShadowMap* shadowMap;
}
ctx;

static RenderInformation sceneInfo;

void Compositor::render(FrameGpuParameters& params, AaSceneManager* sceneMgr, AaCamera& camera, AaShadowMap& shadowMap, imgui::DebugWindow& gui)
{
	ctx = { sceneMgr, &camera, &params, &shadowMap };

	Renderables::Get().updateWorldMatrix();

	params.sunDirection = sceneMgr->lights.directionalLight.direction;
	XMStoreFloat4x4(&params.shadowMapViewProjectionTransposed, XMMatrixTranspose(shadowMap.camera.getViewProjectionMatrix()));

	int idx = 0;
	for (auto& pass : passes)
	{
		auto& commands = passCommands[idx++];
		auto& generalCommands = generalCommandsData[commands.generalCommandsIdx];

		if (pass.target)
		{
			params.inverseViewportSize = { 1.f / pass.target->width, 1.f / pass.target->height };
		}

		if (pass.material)
		{
			pass.target->PrepareAsTarget(generalCommands.commandList, renderSystem->frameIndex);

			for (UINT i = 0; i < pass.inputs.size(); i++)
				pass.material->SetTexture(pass.inputs[i]->textureView, i);

			sceneMgr->renderQuad(pass.material, params, generalCommands.commandList, renderSystem->frameIndex);

			if (pass.present)
				pass.target->PrepareToPresent(generalCommands.commandList, renderSystem->frameIndex);
			else
				pass.target->PrepareAsView(generalCommands.commandList, renderSystem->frameIndex);
		}
		else if (pass.info.render == "Shadows")
		{
			shadowMap.update();
			SetEvent(commands.tasks);
		}
		else if (pass.info.render == "Scene")
		{
			camera.updateMatrix();
			Renderables::Get().updateVisibility(camera.prepareFrustum(), sceneInfo.visibility);
			Renderables::Get().updateWVPMatrix(camera.getViewProjectionMatrix(), sceneInfo.visibility, sceneInfo.wvpMatrix);

			if (pass.info.earlyZ)
			{
				SetEvent(workerBeginRenderFrame[EarlyZ]);
			}

			SetEvent(workerBeginRenderFrame[Scene]);
		}
		else if (pass.info.render == "Imgui")
		{
			pass.target->PrepareAsTarget(generalCommands.commandList, renderSystem->frameIndex, false);

			gui.draw(generalCommands.commandList);

			if (pass.present)
				pass.target->PrepareToPresent(generalCommands.commandList, renderSystem->frameIndex);
			else
				pass.target->PrepareAsView(generalCommands.commandList, renderSystem->frameIndex);
		}
	}

	WaitForSingleObject(workerFinishPass[Shadows], INFINITE);
	renderSystem->ExecuteCommandList(shadowsCommands);
	WaitForSingleObject(workerFinishPass[EarlyZ], INFINITE);
	renderSystem->ExecuteCommandList(earlyZCommands);
	WaitForSingleObject(workerFinishPass[Scene], INFINITE);
	renderSystem->ExecuteCommandList(sceneCommands);

	renderSystem->ExecuteCommandList(generalCommands);

	renderSystem->Present();
	renderSystem->EndFrame();
}

void Compositor::initializeRenderTasks()
{
	generalCommandsData.emplace_back(renderSystem->CreateCommandList());

	for (auto& pass : passes)
	{
		auto& commands = passCommands.emplace_back();

		if (pass.info.render == "Scene")
		{
			if (pass.info.earlyZ)
			{
				commands.tasks.emplace_back(1, renderSystem);
				workers.emplace_back([this, &task = commands.tasks.back(), target = pass.target]
					{
						while (running)
						{
							WaitForSingleObject(task.workerBegin[0], INFINITE);

							prepareEarlyZCommandList(task.commands[0], target);

							SetEvent(task.workerFinish[0]);
						}
					});
			}

			commands.tasks.emplace_back(1, renderSystem);
			workers.emplace_back([this, &task = commands.tasks.back(), target = pass.target]
				{
					while (running)
					{
						WaitForSingleObject(task.workerBegin[0], INFINITE);

						prepareSceneCommandList(task.commands[0], target);

						SetEvent(task.workerFinish[0]);
					}
				});
		}
		else if (pass.info.render == "Shadows")
		{
			commands.tasks.emplace_back(1, renderSystem);
			workers.emplace_back([this, &task = commands.tasks.back()]
				{
					while (running)
					{
						WaitForSingleObject(task.workerBegin[0], INFINITE);

						prepareShadowsCommandList(task.commands[0]);

						SetEvent(task.workerFinish[0]);
					}
				});
		}
	}
}

void Compositor::prepareEarlyZCommandList(AaRenderSystem::CommandsData& commands, RenderTargetTexture* target)
{
	renderSystem->StartCommandList(commands);

	target->PrepareAsDepthTarget(commands.commandList, renderSystem->frameIndex);
	ctx.sceneMgr->renderObjectsDepth(*ctx.camera, sceneInfo, *ctx.params, commands.commandList, renderSystem->frameIndex);
}

void Compositor::prepareSceneCommandList(AaRenderSystem::CommandsData& commands, RenderTargetTexture* target)
{
	renderSystem->StartCommandList(commands);

	target->PrepareAsTarget(commands.commandList, renderSystem->frameIndex);
	ctx.sceneMgr->renderObjects(*ctx.camera, sceneInfo, *ctx.params, commands.commandList, renderSystem->frameIndex);

	target->PrepareAsView(commands.commandList, renderSystem->frameIndex);
}

void Compositor::prepareShadowsCommandList(AaRenderSystem::CommandsData& commands)
{
	static RenderInformation info;
	Renderables::Get().updateVisibility(ctx.shadowMap->camera.prepareOrientedBox(), info.visibility);
	Renderables::Get().updateWVPMatrix(ctx.shadowMap->camera.getViewProjectionMatrix(), info.visibility, info.wvpMatrix);

	renderSystem->StartCommandList(commands);

	ctx.shadowMap->texture.PrepareAsDepthTarget(commands.commandList, renderSystem->frameIndex);

	ctx.sceneMgr->renderObjectsDepth(ctx.shadowMap->camera, info, *ctx.params, commands.commandList, renderSystem->frameIndex);

	ctx.shadowMap->texture.PrepareAsDepthView(commands.commandList, renderSystem->frameIndex);
}

Compositor::RenderTask::RenderTask(int count, AaRenderSystem* rs)
{
	for (int i = 0; i < count; i++)
	{
		commands.emplace_back(rs->CreateCommandList());
		workerBegin.push_back(CreateEvent(NULL, FALSE, FALSE, NULL));
		workerFinish.push_back(CreateEvent(NULL, FALSE, FALSE, NULL));
	}
}
