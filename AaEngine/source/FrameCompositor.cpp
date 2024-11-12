#include "FrameCompositor.h"
#include "CompositorFileParser.h"
#include "AaMaterialResources.h"
#include "AaSceneManager.h"
#include "ShadowMap.h"
#include "DebugWindow.h"
#include "ShadowsRenderTask.h"
#include "SceneRenderTask.h"
#include "VoxelizeSceneTask.h"
#include "DebugOverlayTask.h"
#include "SceneTestTask.h"
#include "ImguiDebugWindowTask.h"

FrameCompositor::FrameCompositor(RenderProvider p, AaSceneManager& scene, AaShadowMap& shadows) : provider(p), sceneMgr(scene), shadowMaps(shadows)
{
}

FrameCompositor::~FrameCompositor()
{
	for (auto c : generalCommandsArray)
		c.deinit();

	info.passes.clear();
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
		auto& pass = passes.emplace_back(p);

		if (pass.info.render == "Scene")
		{
			pass.task = std::make_unique<SceneRenderTask>(provider, sceneMgr);
		}
		else if (pass.info.render == "SceneTransparent")
		{
			pass.task = std::make_unique<SceneRenderTransparentTask>(provider, sceneMgr);
		}
		else if (pass.info.render == "Shadows")
		{
			pass.task = std::make_unique<ShadowsRenderTask>(provider, sceneMgr, shadowMaps);
		}
		else if (pass.info.render == "VoxelScene")
		{
			pass.task = std::make_unique<VoxelizeSceneTask>(provider, sceneMgr, shadowMaps);
		}
		else if (pass.info.render == "DebugOverlay")
		{
			pass.task = std::make_unique<DebugOverlayTask>(provider, sceneMgr);
		}
		else if (pass.info.render == "Imgui")
		{
			pass.task = std::make_unique<ImguiDebugWindowTask>(provider, sceneMgr);
		}
		else if (pass.info.render == "Test")
		{
			pass.task = std::make_unique<SceneTestTask>(provider, sceneMgr);
		}
	}

	initializeTextureStates();

	UINT textureCount = 0;
	for (auto& t : info.textures)
		textureCount += t.formats.size();

	rtvHeap.Init(provider.renderSystem->device, textureCount, provider.renderSystem->FrameCount, L"CompositorRTV");
	reloadTextures();

	for (size_t i = 0; i < passes.size(); i++)
	{
		const auto& passInfo = info.passes[i];
		auto& pass = passes[i];

		if (!passInfo.material.empty() && pass.target.texture)
			pass.material = AaMaterialResources::get().getMaterial(passInfo.material)->Assign({}, pass.target.texture->formats);
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

		auto lastState = getInitialTextureState(t.name);

		RenderTargetTexture& tex = textures[t.name];
		tex.Init(provider.renderSystem->device, w, h, provider.renderSystem->FrameCount, rtvHeap, t.formats, lastState, t.depthBuffer);
		tex.SetName(std::wstring(t.name.begin(), t.name.end()).c_str());

		if (reusedHeapDescriptors.empty())
			DescriptorManager::get().createTextureView(tex);
		else
			DescriptorManager::get().createTextureView(tex, reusedHeapDescriptors[i++]);
	}

	for (auto& p : passes)
	{
		for (UINT i = 0; auto& [name, idx] : p.info.inputs)
		{
			p.inputs[i].view = &textures[name].textures[idx].textureView;
			p.inputs[i++].texture = &textures[name];
		}

		if (!p.info.target.empty())
		{
			if (p.info.target == "Backbuffer")
				p.target.texture = &provider.renderSystem->backbuffer;
			else
				p.target.texture = &textures[p.info.target];
		}

		if (p.task)
		{
			p.task->resize(p);
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
		auto& syncCommands = pass.generalCommands;
		if (pass.startCommands)
			provider.renderSystem->StartCommandList(syncCommands);

		if (pass.target.texture)
		{
			provider.params.inverseViewportSize = { 1.f / pass.target.texture->width, 1.f / pass.target.texture->height };
		}

		if (pass.material)
		{
 			pass.target.texture->PrepareAsTarget(syncCommands.commandList, provider.renderSystem->frameIndex, pass.target.previousState, false, false);

			for (UINT i = 0; auto & input : pass.inputs)
			{
				input.texture->PrepareAsView(syncCommands.commandList, provider.renderSystem->frameIndex, input.previousState);
				pass.material->SetTexture(*input.view, i++);
			}

 			RenderQuad(pass.material, provider, ctx, syncCommands.commandList, provider.renderSystem->frameIndex);
		}
		else if (pass.task)
		{
			pass.task->run(ctx, syncCommands, pass);
		}

		if (pass.target.present)
			pass.target.texture->PrepareToPresent(syncCommands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
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
