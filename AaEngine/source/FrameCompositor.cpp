#include "FrameCompositor.h"
#include "CompositorFileParser.h"
#include "MaterialResources.h"
#include "SceneManager.h"
#include "ShadowMaps.h"
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
#include "PrepareFrameTask.h"

FrameCompositor::FrameCompositor(const InitConfig& params, RenderProvider p, SceneManager& scene, ShadowMaps& shadows) : config(params), provider(p), sceneMgr(scene), shadowMaps(shadows)
{
}

FrameCompositor::~FrameCompositor()
{
	for (auto c : generalCommandsArray)
		c.deinit();
}

void FrameCompositor::reloadPasses()
{
	std::set<std::string> defines;
	auto& upscale = provider.renderSystem.upscale;
	if (upscale.dlss.enabled()) defines.insert("DLSS");
	if (upscale.fsr.enabled()) defines.insert("FSR");
	if (upscale.dlss.enabled() || upscale.fsr.enabled()) defines.insert("UPSCALE");

	info = CompositorFileParser::parseFile(FRAME_DIRECTORY, config.file + ".compositor", { defines });

	if (!config.renderToBackbuffer)
	{
		auto& outputTexture = info.textures["Output"];
		outputTexture.name = "Output";
		outputTexture.outputScale = true;
		outputTexture.height = outputTexture.width = 1.0f;
		outputTexture.format = config.outputFormat;
	}

	passes.clear();

	std::map<std::string, std::shared_ptr<CompositorTask>> tasks;

	for (auto& p : info.passes)
	{
		auto& pass = passes.emplace_back(p);
		auto& task = tasks[pass.info.task];

		if (!task)
		{
			if (pass.info.task == "SceneRender")
			{
				pass.task = std::make_shared<SceneRenderTask>(provider, sceneMgr);
			}
			else if (pass.info.task == "PrepareFrame")
			{
				pass.task = std::make_shared<PrepareFrameTask>(provider, sceneMgr);
			}
			else if (pass.info.task == "Shadows")
			{
				pass.task = std::make_shared<ShadowsRenderTask>(provider, sceneMgr, shadowMaps);
			}
			else if (pass.info.task == "VoxelScene")
			{
				pass.task = std::make_shared<VoxelizeSceneTask>(provider, sceneMgr, shadowMaps);
			}
			else if (pass.info.task == "DebugOverlay")
			{
				pass.task = std::make_shared<DebugOverlayTask>(provider, sceneMgr);
			}
			else if (pass.info.task == "Imgui")
			{
				pass.task = std::make_shared<ImguiDebugWindowTask>(provider, sceneMgr);
			}
			else if (pass.info.task == "SSAO")
			{
				pass.task = std::make_shared<SsaoComputeTask>(provider, sceneMgr);
			}
			else if (pass.info.task == "Upscale")
			{
				pass.task = std::make_shared<UpscaleTask>(provider, sceneMgr);
			}
			else if (pass.info.task == "Test")
			{
				pass.task = std::make_shared<SceneTestTask>(provider, sceneMgr);
			}
			else if (pass.info.task == "HiZDepthDownsample")
			{
				pass.task = std::make_shared<DownsampleDepthTask>(provider, sceneMgr);
			}
			else if (auto it = externTasks.find(pass.info.task); it != externTasks.end())
			{
				pass.task = it->second(provider, sceneMgr);
			}

			task = pass.task;
		}
		else
		{
			pass.task = task;
		}
	}

	initializeTextureStates();

	reloadTextures();

	for (auto& pass : passes)
	{
		if (!pass.info.material.empty() && pass.targets.size() == 1)
			pass.material = provider.resources.materials.getMaterial(pass.info.material)->Assign({}, { pass.targets.front().texture->format });
	}

	initializeCommands();
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

		rtvHeap.InitRtv(provider.renderSystem.core.device, textureCount, L"CompositorRTV");
		rtvHeap.InitDsv(provider.renderSystem.core.device, depthCount, L"CompositorDSV");
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
				auto sz = provider.renderSystem.getRenderSize();
				w = sz.x * t.width;
				h = sz.y * t.height;
			}
			else if (t.outputScale)
			{
				auto sz = provider.renderSystem.getOutputSize();
				w = sz.x * t.width;
				h = sz.y * t.height;
			}
			if (t.arraySize > 1)
			{
				auto sqr = sqrt(t.arraySize);
				w = std::ceil(w / sqr);
				h = std::ceil(h / sqr);
			}
			w = max(w, 1);
			h = max(h, 1);


			{
				auto lastState = initialTextureStates[name];

				GpuTexture2D& tex = textures[name];
				tex.depthOrArraySize = t.arraySize;

				if (name.ends_with(":Depth"))
					tex.InitDepth(provider.renderSystem.core.device, w, h, rtvHeap, lastState);
				else
				{
					if (t.uav)
						tex.InitUAV(provider.renderSystem.core.device, w, h, t.format, lastState);
					else
						tex.InitRenderTarget(provider.renderSystem.core.device, w, h, rtvHeap, t.format, lastState);
				}

				tex.SetName(name);

				DescriptorManager::get().createTextureView(tex);
				if (t.uav)
					DescriptorManager::get().createUAVView(tex);

				provider.resources.textures.setNamedTexture(info.name + ":" + name, tex.view);
			}
		};

	//ensure mrt handles are contiguous
	for (auto& [_, names] : info.mrt)
	{
		for (auto& name : names)
		{
			initializeTexture(name, info.textures[name]);
		}
	}

	for (auto& [name, t] : info.textures)
	{
		initializeTexture(name, t);
	}

	for (auto& p : passes)
	{
		for (UINT i = 0; auto& [name,_] : p.info.inputs)
		{
			p.inputs[i++].texture = &textures[name];
		}

		for (UINT i = 0; auto& [name, _] : p.info.targets)
		{
			auto& target = p.targets[i++];

			if (config.renderToBackbuffer && name == "Output")
			{
				target.texture = &provider.renderSystem.core.backbuffer[0];
				p.backbuffer = true;
			}
			else
			{
				target.texture = &textures[name];
			}
		}

		if (p.mrt)
		{
			std::vector<GpuTexture2D*> mrtTex;
			GpuTexture2D* mrtDepth{};

			for (int c = 0; auto& [name, _] : p.info.targets)
			{
				auto t = &textures[name];

				if (name.ends_with(":Depth"))
					mrtDepth = t;
				else
					mrtTex.push_back(t);
			}

			p.mrt->Init(provider.renderSystem.core.device, mrtTex, mrtDepth);
		}

		if (p.task)
		{
			p.task->resize(p);
		}
	}
}

void FrameCompositor::renderQuad(PassData& pass, RenderContext& ctx, ID3D12GraphicsCommandList* commandList)
{
	auto& target = pass.targets.front();
	target.texture->PrepareAsRenderTarget(commandList, target.previousState);
	GpuTextureStates::Transition(commandList, pass.inputs);

	for (UINT i = 0; auto& input : pass.inputs)
	{
		pass.material->SetTexture(input.texture->view, i++);
	}

	ShaderConstantsProvider constants(provider.params, {}, *ctx.camera, *target.texture);
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
		auto& syncCommands = pass.syncCommands;
		if (pass.startCommands)
			provider.renderSystem.core.StartCommandListNoMarker(*syncCommands);
		if (pass.startComputeCommands)
			provider.renderSystem.core.StartCommandListNoMarker(*pass.computeCommands);
		if (pass.backbuffer)
			pass.targets.front().texture = &provider.renderSystem.core.backbuffer[provider.renderSystem.core.frameIndex];

		if (pass.material)
		{
			CommandsMarker marker(syncCommands->commandList, pass.info.name.c_str(), PixColor::Compositor);
 			renderQuad(pass, ctx, syncCommands->commandList);
		}
		else if (pass.task)
		{
			if (syncCommands)
				pass.task->run(ctx, *syncCommands, pass);
			else if (pass.computeCommands)
				pass.task->runCompute(ctx, *pass.computeCommands, pass);
			else
				pass.task->run(ctx, pass);
		}

		if (pass.present)
		{
			auto& target = pass.targets.front();

			if (config.renderToBackbuffer)
				target.texture->PrepareToPresent(syncCommands->commandList, target.previousState);
			else
				target.texture->PrepareAsView(syncCommands->commandList, target.previousState);
		}

		if (!pass.postTransition.empty() && syncCommands)
		{
			TextureTransitions<5> tr;
			for (auto& pt : pass.postTransition)
				tr.add(pt->texture, pt->state, pt->nextState);
			tr.push(syncCommands->commandList);
		}
	}
	executeCommands();
}

const GpuTexture2D* FrameCompositor::getTexture(const std::string& name) const
{
	auto it = textures.find(name);
	if (it == textures.end())
		return nullptr;

	return &it->second;
}

void FrameCompositor::registerTask(const std::string& name, CreateTaskFunc f)
{
	externTasks[name] = f;
}

CompositorTask* FrameCompositor::getTask(const std::string& name)
{
	for (auto& p : passes)
	{
		if (p.info.task == name && p.task)
			return p.task.get();
	}

	return nullptr;
}

void FrameCompositor::executeCommands()
{
	auto& renderer = provider.renderSystem.core;

	for (auto& t : tasks)
	{
		for (auto& f : t.syncWait)
			f.first->Wait(f.second->fence.Get(), f.second->value++);

		if (t.finishEvents.empty())
		{
			for (auto& c : t.data)
			{
				renderer.ExecuteCommandList(c);
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
					renderer.ExecuteCommandList(t.data[index]);
				}
			}
		}

		for (auto& f : t.syncSignal)
			f.first->Signal(f.second->fence.Get(), f.second->value);
	}
}
