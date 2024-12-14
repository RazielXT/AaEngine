#include "FrameCompositor.h"
#include "CompositorFileParser.h"
#include "AaMaterialResources.h"
#include "SceneManager.h"
#include "ShadowMap.h"
#include "DebugWindow.h"
#include "ShadowsRenderTask.h"
#include "SceneRenderTask.h"
#include "VoxelizeSceneTask.h"
#include "DebugOverlayTask.h"
#include "SceneTestTask.h"
#include "ImguiDebugWindowTask.h"
#include "Directories.h"
#include "SsaoComputeTask.h"
#include "UpscaleTask.h"
#include "DownsampleTask.h"

FrameCompositor::FrameCompositor(RenderProvider p, SceneManager& scene, AaShadowMap& shadows) : provider(p), sceneMgr(scene), shadowMaps(shadows)
{
}

FrameCompositor::~FrameCompositor()
{
	for (auto c : generalCommandsArray)
		c.deinit();
}

void FrameCompositor::load(std::string path)
{
	load(CompositorFileParser::parseFile(FRAME_DIRECTORY, path));
}

void FrameCompositor::load(CompositorInfo i)
{
	info = i;
	std::vector<CompositorPassCondition> conditions;
	auto& upscale = provider.renderSystem->upscale;
	conditions.push_back({ upscale.dlss.enabled() , "DLSS" });
	conditions.push_back({ upscale.fsr.enabled() , "FSR" });
	conditions.push_back({ upscale.dlss.enabled() || upscale.fsr.enabled() , "UPSCALE" });

	passes.clear();
	for (auto& p : info.passes)
	{
		if (p.condition)
		{
			bool pass = false;
			for (auto& c : conditions)
				pass = pass || c == p.condition;

			if (!pass)
				continue;
		}

		auto& pass = passes.emplace_back(p);

		if (pass.info.task == "Scene")
		{
			pass.task = std::make_unique<SceneRenderTask>(provider, sceneMgr);
		}
		else if (pass.info.task == "SceneTransparent")
		{
			pass.task = std::make_unique<SceneRenderTransparentTask>(provider, sceneMgr);
		}
		else if (pass.info.task == "Shadows")
		{
			pass.task = std::make_unique<ShadowsRenderTask>(provider, sceneMgr, shadowMaps);
		}
		else if (pass.info.task == "VoxelScene")
		{
			pass.task = std::make_unique<VoxelizeSceneTask>(provider, sceneMgr, shadowMaps);
		}
		else if (pass.info.task == "DebugOverlay")
		{
			pass.task = std::make_unique<DebugOverlayTask>(provider, sceneMgr);
		}
		else if (pass.info.task == "Imgui")
		{
			pass.task = std::make_unique<ImguiDebugWindowTask>(provider, sceneMgr);
		}
		else if (pass.info.task == "SSAO")
		{
			pass.task = std::make_unique<SsaoComputeTask>(provider, sceneMgr);
		}
		else if (pass.info.task == "Upscale")
		{
			pass.task = std::make_unique<UpscaleTask>(provider, sceneMgr);
		}
		else if (pass.info.task == "Test")
		{
			pass.task = std::make_unique<SceneTestTask>(provider, sceneMgr);
		}
		else if (pass.info.task == "HiZDepthDownscale")
		{
			pass.task = std::make_unique<DownsampleDepthTask>(provider, sceneMgr);
		}
	}

	initializeTextureStates();

	reloadTextures();

	for (auto& pass : passes)
	{
		if (!pass.info.material.empty() && pass.target.texture)
			pass.material = AaMaterialResources::get().getMaterial(pass.info.material)->Assign({}, { pass.target.texture->format });
	}

	initializeCommands();
}

void FrameCompositor::reloadPasses()
{
	load(info);
}

void FrameCompositor::reloadTextures()
{
	if (textures.empty())
	{
		UINT textureCount = 0;
		UINT depthCount = 0;
		for (auto& [name, t] : info.textures)
			if (name.ends_with(":Depth"))
				depthCount++;
			else
				textureCount++;

		rtvHeap.InitRtv(provider.renderSystem->device, textureCount, L"CompositorRTV");
		rtvHeap.InitDsv(provider.renderSystem->device, depthCount, L"CompositorDSV");
	}
	rtvHeap.Reset();
	textures.clear();

	auto initializeTexture = [this](const std::string& name, const CompositorTextureInfo& t)
		{
			if (textures.contains(name))
				return;

			UINT w = t.width;
			UINT h = t.height;

			if (t.targetScale)
			{
				auto sz = provider.renderSystem->getRenderSize();
				w = sz.x * t.width;
				h = sz.y * t.height;
			}
			else if (t.outputScale)
			{
				auto sz = provider.renderSystem->getOutputSize();
				w = sz.x * t.width;
				h = sz.y * t.height;
			}
			if (t.arraySize > 1)
			{
				auto sqr = sqrt(t.arraySize);
				w /= sqr;
				h /= sqr;
			}

			{
				auto lastState = lastTextureStates[name];

				RenderTargetTexture& tex = textures[name];
				tex.arraySize = t.arraySize;

				if (name.ends_with(":Depth"))
					tex.InitDepth(provider.renderSystem->device, w, h, rtvHeap, lastState);
				else
				{

					UINT flags = t.uav ? RenderTargetTexture::AllowUAV : RenderTargetTexture::AllowRenderTarget;
					tex.Init(provider.renderSystem->device, w, h, rtvHeap, t.format, lastState, flags);
				}

				tex.SetName(name);

				DescriptorManager::get().createTextureView(tex);
				if (t.uav)
					DescriptorManager::get().createUAVView(tex);

				AaTextureResources::get().setNamedTexture("c_" + name, tex.view);
			}
		};

	//ensure mrt handles are ordered
	for (auto& [_, names] : info.mrt)
	{
		for (auto& name : names)
		{
			initializeTexture(name, info.textures[name]);
		}
	}

	for (auto& [name,t] : info.textures)
	{
		initializeTexture(name, t);
	}

	for (auto& p : passes)
	{
		for (UINT i = 0; auto& [name,_] : p.info.inputs)
		{
			p.inputs[i++].texture = &textures[name];
		}

		if (!p.info.targets.empty())
		{
			if (p.info.targets.front().name == "Backbuffer")
			{
				p.target.texture = &provider.renderSystem->backbuffer[0];
				p.target.backbuffer = true;
			}
			else
			{
				p.target.texture = &textures[p.info.targets.front().name];
			}
			
			if (p.target.textureSet)
			{
				auto textureSet = p.target.textureSet.get();

				for (int c = 0; auto& [targetName,_] : p.info.targets)
				{
					auto t = &textures[targetName];

					if (targetName.ends_with(":Depth"))
						textureSet->depthState.texture = t;
					else
						textureSet->texturesState[c++].texture = t;
				}

				textureSet->Init();
			}
		}

		if (p.task)
		{
			p.task->resize(p);
		}
	}
}

void FrameCompositor::renderQuad(PassData& pass, RenderContext& ctx, ID3D12GraphicsCommandList* commandList)
{
	pass.target.texture->PrepareAsTarget(commandList, pass.target.previousState);

	for (UINT i = 0; auto & input : pass.inputs)
	{
		input.texture->PrepareAsView(commandList, input.previousState);
		pass.material->SetTexture(input.texture->view, i++);
	}

	ShaderConstantsProvider constants(provider.params, {}, *ctx.camera, *pass.target.texture);
	MaterialDataStorage storage;

	auto material = pass.material;
	material->GetBase()->BindSignature(commandList);

	material->LoadMaterialConstants(storage);
	material->UpdatePerFrame(storage, constants);
	material->BindPipeline(commandList);
	material->BindTextures(commandList);
	material->BindConstants(commandList, storage, constants);

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
		if (pass.target.backbuffer)
			pass.target.texture = &provider.renderSystem->backbuffer[provider.renderSystem->frameIndex];

		if (pass.material)
		{
			CommandsMarker marker(syncCommands.commandList, pass.info.name.c_str());
 			renderQuad(pass, ctx, syncCommands.commandList);
		}
		else if (pass.task)
		{
			pass.task->run(ctx, syncCommands, pass);
		}

		if (pass.target.present)
			pass.target.texture->PrepareToPresent(syncCommands.commandList, pass.target.previousState);
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
