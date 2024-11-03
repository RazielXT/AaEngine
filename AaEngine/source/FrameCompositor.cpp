#include "FrameCompositor.h"
#include "CompositorFileParser.h"
#include "AaMaterialResources.h"
#include "AaSceneManager.h"
#include "ShadowMap.h"
#include "DebugWindow.h"

FrameCompositor::FrameCompositor(RenderProvider p, AaSceneManager* scene, AaShadowMap* shadows) : shadowRender(p, *shadows), sceneVoxelize(p, *shadows), sceneRender(p), testTask(p), debugOverlay(p), provider(p)
{
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

	rtvHeap.Init(provider.renderSystem->device, textureCount, provider.renderSystem->FrameCount, L"CompositorRTV");
	reloadTextures();

	for (size_t i = 0; i < passes.size(); i++)
	{
		const auto& passInfo = info.passes[i];
		auto& pass = passes[i];

		if (!passInfo.material.empty() && pass.target)
			pass.material = AaMaterialResources::get().getMaterial(passInfo.material)->Assign({}, pass.target->formats);
	}

	if (!passes.empty() && passes.back().target == &provider.renderSystem->backbuffer)
	{
		passes.back().present = true;
	}

	initializeCommands();
}

void FrameCompositor::reloadTextures()
{
	rtvHeap.Reset();

	std::vector<UINT> reusedHeapDescriptors;
	if (!textures.empty())
	{
		for (auto& t : info.textures)
		{
			auto& tex = textures[t.name];
			reusedHeapDescriptors.push_back(tex.textures.front().textureView.srvHeapIndex);
		}
		textures.clear();
	}

	for (int i = 0; auto& t : info.textures)
	{
		UINT w = t.width;
		UINT h = t.height;

		if (t.targetScale)
		{
			w = provider.renderSystem->getWindow()->getWidth() * t.width;
			h = provider.renderSystem->getWindow()->getHeight() * t.height;
		}

		RenderTargetTexture& tex = textures[t.name];
		tex.Init(provider.renderSystem->device, w, h, provider.renderSystem->FrameCount, rtvHeap, t.formats, t.depthBuffer);
		tex.SetName(std::wstring(t.name.begin(), t.name.end()).c_str());

		if (reusedHeapDescriptors.empty())
			ResourcesManager::get().createShaderResourceView(tex);
		else
			ResourcesManager::get().createShaderResourceView(tex, reusedHeapDescriptors[i++]);
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
				p.target = &provider.renderSystem->backbuffer;
			else
				p.target = &textures[p.info.target];
		}

		if (p.info.render == "DebugOverlay")
		{
			debugOverlay.resize(p.target);
		}
	}
}

static void RenderQuad(AaMaterial* material, RenderProvider& provider, RenderContext& ctx, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	ShaderConstantsProvider constants({}, *ctx.camera);

	material->GetBase()->BindSignature(commandList, frameIndex);

	material->LoadMaterialConstants(constants);
	material->UpdatePerFrame(constants, provider.params);
	material->BindPipeline(commandList);
	material->BindTextures(commandList, frameIndex);
	material->BindConstants(commandList, frameIndex, constants);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}

void FrameCompositor::render(RenderContext& ctx)
{
	for (auto& pass : passes)
	{
		auto& generalCommands = pass.generalCommands;
		if (pass.startCommands)
			provider.renderSystem->StartCommandList(generalCommands);

		ctx.target = pass.target;

		if (pass.target)
		{
			provider.params.inverseViewportSize = { 1.f / pass.target->width, 1.f / pass.target->height };
		}

		if (pass.material)
		{
 			pass.target->PrepareAsTarget(generalCommands.commandList, provider.renderSystem->frameIndex, false, false);

			for (UINT i = 0; i < pass.inputs.size(); i++)
				pass.material->SetTexture(*pass.inputs[i], i);

 			RenderQuad(pass.material, provider, ctx, generalCommands.commandList, provider.renderSystem->frameIndex);

			if (pass.present)
				pass.target->PrepareToPresent(generalCommands.commandList, provider.renderSystem->frameIndex);
			else
				pass.target->PrepareAsView(generalCommands.commandList, provider.renderSystem->frameIndex);
		}
		else if (pass.info.render == "Shadows")
		{
			shadowRender.prepare(ctx, generalCommands);
		}
		else if (pass.info.render == "Scene")
		{
			sceneRender.prepare(ctx, generalCommands);
		}
		else if (pass.info.render == "VoxelScene")
		{
			sceneVoxelize.prepare(ctx, generalCommands);
		}
		else if (pass.info.render == "Test")
		{
			testTask.prepare(ctx, generalCommands);
		}
		else if (pass.info.render == "DebugOverlay")
		{
			pass.target->PrepareAsTarget(generalCommands.commandList, provider.renderSystem->frameIndex, false, false);

			debugOverlay.prepare(ctx, generalCommands);

			if (pass.present)
				pass.target->PrepareToPresent(generalCommands.commandList, provider.renderSystem->frameIndex);
			else
				pass.target->PrepareAsView(generalCommands.commandList, provider.renderSystem->frameIndex);
		}
		else if (pass.info.render == "Imgui")
		{
			pass.target->PrepareAsTarget(generalCommands.commandList, provider.renderSystem->frameIndex, false);

			imgui::DebugWindow::Get().draw(generalCommands.commandList);

			if (pass.present)
				pass.target->PrepareToPresent(generalCommands.commandList, provider.renderSystem->frameIndex);
			else
				pass.target->PrepareAsView(generalCommands.commandList, provider.renderSystem->frameIndex);
		}
	}
	testTask.finish();
	executeCommands();

	provider.renderSystem->Present();
	provider.renderSystem->EndFrame();
}

void FrameCompositor::executeCommands()
{
	for (auto& t : tasks)
	{
		if (t.finishEvents.empty())
		{
			for (auto& c : t.data)
			{
				provider.renderSystem->ExecuteCommandList(c);
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
					provider.renderSystem->ExecuteCommandList(t.data[index]);
				}
			}
		}
	}
}
