#include "FrameCompositor.h"
#include "CompositorFileParser.h"
#include "AaMaterialResources.h"
#include "AaSceneManager.h"
#include "ShadowMap.h"
#include "DebugWindow.h"

FrameCompositor::FrameCompositor(AaRenderSystem* rs, AaSceneManager* scene, AaShadowMap* shadows) : shadowRender(*shadows)
{
	renderSystem = rs;
	sceneMgr = scene;
}

FrameCompositor::~FrameCompositor()
{
	for (auto c : generalCommandsArray)
		c.deinit();
}

void FrameCompositor::load(std::string path)
{
	load(CompositorFileParser::parseFile(path));
}

void FrameCompositor::load(CompositorInfo i)
{
	info = i;

	for (auto& p : info.passes)
	{
		passes.emplace_back(p);
	}

	UINT textureCount = 0;
	for (auto& t : info.textures)
		textureCount += t.formats.size();

	rtvHeap.Init(renderSystem->device, renderSystem->FrameCount * textureCount, L"CompositorRTV");
	reloadTextures();

	for (size_t i = 0; i < passes.size(); i++)
	{
		const auto& passInfo = info.passes[i];
		auto& pass = passes[i];

		if (!passInfo.material.empty() && pass.target)
			pass.material = AaMaterialResources::get().getMaterial(passInfo.material)->Assign({}, pass.target->formats);
	}

	if (!passes.empty() && passes.back().target == &renderSystem->backbuffer)
	{
		passes.back().present = true;
	}

	initializeCommands();
}

void FrameCompositor::reloadTextures()
{
	rtvHeap.Reset();
	textures.clear();
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
		tex.Init(renderSystem->device, w, h, renderSystem->FrameCount, rtvHeap, t.formats, t.depthBuffer);
		tex.SetName(std::wstring(t.name.begin(), t.name.end()).c_str());

		AaMaterialResources::get().PrepareShaderResourceView(tex);
	}

	for (auto& p : passes)
	{
		p.inputs.clear();
		for (auto& [name, idx] : p.info.inputs)
		{
			p.inputs.push_back(&textures[name].textures[idx].textureView);
		}

		if (!p.info.target.empty())
		{
			if (p.info.target == "Backbuffer")
				p.target = &renderSystem->backbuffer;
			else
				p.target = &textures[p.info.target];
		}
	}
}

static void RenderQuad(AaMaterial* material, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	MaterialConstantBuffers constants;

	material->GetBase()->BindSignature(commandList, frameIndex);

	material->LoadMaterialConstants(constants);
	material->UpdatePerFrame(constants, params, {});
	material->BindPipeline(commandList);
	material->BindTextures(commandList, frameIndex);
	material->BindConstants(commandList, frameIndex, constants);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}

void FrameCompositor::render(RenderContext& ctx, imgui::DebugWindow& gui)
{
	Renderables::Get().updateWorldMatrix();

	for (auto& pass : passes)
	{
		auto& generalCommands = pass.generalCommands;
		if (pass.startCommands)
			renderSystem->StartCommandList(generalCommands);

		ctx.target = pass.target;

		if (pass.target)
		{
			ctx.params.inverseViewportSize = { 1.f / pass.target->width, 1.f / pass.target->height };
		}

		if (pass.material)
		{
			pass.target->PrepareAsTarget(generalCommands.commandList, renderSystem->frameIndex, false, false);

			for (UINT i = 0; i < pass.inputs.size(); i++)
				pass.material->SetTexture(*pass.inputs[i], i);

			RenderQuad(pass.material, ctx.params, generalCommands.commandList, renderSystem->frameIndex);

			if (pass.present)
				pass.target->PrepareToPresent(generalCommands.commandList, renderSystem->frameIndex);
			else
				pass.target->PrepareAsView(generalCommands.commandList, renderSystem->frameIndex);
		}
		else if (pass.info.render == "Shadows")
		{
			shadowRender.prepare(ctx, generalCommands);
		}
		else if (pass.info.render == "Scene")
		{
			sceneRender.prepare(ctx, generalCommands);
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

	executeCommands();

	renderSystem->Present();
	renderSystem->EndFrame();
}

void FrameCompositor::initializeCommands()
{
	auto containsPassResources = [](const std::set<std::string>& pushedResources, const CompositorPass& pass)
		{
			return pushedResources.contains(pass.after)
				|| pushedResources.contains(pass.target)
				|| std::any_of(pass.inputs.begin(), pass.inputs.end(), [&pushedResources](const std::pair<std::string, UINT>& s) { return pushedResources.find(s.first) != pushedResources.end(); });
		};

	AsyncTasksInfo pushedAsyncTasks;
	std::set<std::string> pushedAsyncResources;

	auto commitAsyncTasks = [&]()
		{
			if (pushedAsyncTasks.empty())
				return;

			TasksGroup& group = tasks.emplace_back();
			for (auto& t : pushedAsyncTasks)
			{
				group.data.push_back(t.commands);
				group.finishEvents.push_back(t.finishEvent);
			}

			pushedAsyncTasks.clear();
			pushedAsyncResources.clear();
		};

	auto pushAsyncTasks = [&](const CompositorPass& pass, const AsyncTasksInfo& info)
		{
			if (containsPassResources(pushedAsyncResources, pass))
				commitAsyncTasks();

			pushedAsyncTasks.insert(pushedAsyncTasks.end(), info.begin(), info.end());

			if (!pass.name.empty())
				pushedAsyncResources.insert(pass.name);

			if (!pass.target.empty())
				pushedAsyncResources.insert(pass.target);
		};

	CommandsData generalCommands{};
	std::set<std::string> pushedCommandsResources;
	uint32_t syncCount = 0;
	auto commitSyncCommands = [&]()
		{
			if (generalCommands.commandList)
				tasks.push_back({ {}, { generalCommands } });
		};
	auto refreshSyncCommands = [&](FrameCompositor::PassData& passData, bool syncPass)
		{
			if (!generalCommands.commandList || (!syncPass && syncCount && containsPassResources(pushedAsyncResources, passData.info)))
			{
				commitSyncCommands();
				generalCommands = generalCommandsArray.emplace_back(renderSystem->CreateCommandList(L"Compositor"));
				passData.startCommands = true;
				syncCount = 0;
			}
			syncCount += syncPass;
			return generalCommands;
		};

	for (auto& pass : passes)
	{
		bool syncPass = false;

		if (pass.info.render == "Shadows")
		{
			pushAsyncTasks(pass.info, shadowRender.initialize(renderSystem, sceneMgr->createQueue({}, true)));
		}
		else if (pass.info.render == "Scene")
		{
			pushAsyncTasks(pass.info, sceneRender.initialize(renderSystem, sceneMgr->createQueue(pass.target->formats)));
		}
		else if (pass.info.render == "SceneEarlyZ")
		{
			pushAsyncTasks(pass.info, sceneRender.initializeEarlyZ(renderSystem, sceneMgr->createQueue({}, true)));
		}
		else
		{
			syncPass = true;
		}

		pass.generalCommands = refreshSyncCommands(pass, syncPass);
	}

	commitAsyncTasks();
	commitSyncCommands();
}

void FrameCompositor::executeCommands()
{
	for (auto& t : tasks)
	{
		if (t.finishEvents.empty())
		{
			for (auto& c : t.data)
			{
				renderSystem->ExecuteCommandList(c);
			}
		}
		else
		{
			for (size_t i = 0; i < t.finishEvents.size(); i++)
			{
				DWORD dwWaitResult = WaitForMultipleObjects(t.finishEvents.size(), t.finishEvents.data(), FALSE, INFINITE);
				if (dwWaitResult >= WAIT_OBJECT_0 && dwWaitResult < WAIT_OBJECT_0 + t.finishEvents.size())
				{
					int index = dwWaitResult - WAIT_OBJECT_0;
					renderSystem->ExecuteCommandList(t.data[index]);
				}
			}
		}
	}
}
