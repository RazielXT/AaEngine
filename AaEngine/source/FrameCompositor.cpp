#include "FrameCompositor.h"
#include "CompositorFileParser.h"
#include "MaterialResources.h"
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
#include "RenderWireframeTask.h"

FrameCompositor::FrameCompositor(RenderProvider p, SceneManager& scene, AaShadowMap& shadows) : provider(p), sceneMgr(scene), shadowMaps(shadows)
{
}

FrameCompositor::~FrameCompositor()
{
	for (auto c : generalCommandsArray)
		c.deinit();
}

void FrameCompositor::load(std::string path, const InitParams& params)
{
	renderToBackbuffer = params.renderToBackbuffer;
	info = CompositorFileParser::parseFile(FRAME_DIRECTORY, path + ".compositor");

	if (!renderToBackbuffer)
	{
		auto& outputTexture = info.textures["Output"];
		outputTexture.name = "Output";
		outputTexture.outputScale = true;
		outputTexture.height = outputTexture.width = 1.0f;
		outputTexture.format = params.outputFormat;
	}

	load(info);
}

void FrameCompositor::load(CompositorInfo i)
{
	std::vector<CompositorPassCondition> conditions;
	auto& upscale = provider.renderSystem.upscale;
	conditions.push_back({ upscale.dlss.enabled() , "DLSS" });
	conditions.push_back({ upscale.fsr.enabled() , "FSR" });
	conditions.push_back({ upscale.dlss.enabled() || upscale.fsr.enabled() , "UPSCALE" });

	passes.clear();

	std::map<std::string, std::shared_ptr<CompositorTask>> tasks;

	for (auto& p : info.passes)
	{
		bool passedCondition = true;

		for (auto& passCondition : p.conditions)
		{
			bool found = false;

			for (auto& c : conditions)
				if (c.param == passCondition.param)
				{
					found = true;
					if (c.accept != passCondition.accept)
						passedCondition = false;
				}

			if (!found && passCondition.accept)
				passedCondition = false;
		}
		if (!passedCondition)
			continue;

		auto& pass = passes.emplace_back(p);
		auto& task = tasks[pass.info.task];

		if (!task)
		{
			if (pass.info.task == "SceneRender")
			{
				pass.task = std::make_shared<SceneRenderTask>(provider, sceneMgr);
			}
			else if (pass.info.task == "Shadows")
			{
				pass.task = std::make_shared<ShadowsRenderTask>(provider, sceneMgr, shadowMaps);
			}
			else if (pass.info.task == "VoxelScene")
			{
				pass.task = std::make_shared<VoxelizeSceneTask>(provider, sceneMgr, shadowMaps);
			}
			else if (pass.info.task == "RenderWireframe")
			{
				pass.task = std::make_shared<RenderWireframeTask>(provider, sceneMgr);
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
			else if (pass.info.task == "UpscalePrepare")
			{
				pass.task = std::make_shared<UpscalePrepareTask>(provider, sceneMgr);
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
		if (!pass.info.material.empty() && pass.target.texture)
			pass.material = provider.resources.materials.getMaterial(pass.info.material)->Assign({}, { pass.target.texture->format });
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
				auto lastState = lastTextureStates[name];

				RenderTargetTexture& tex = textures[name];
				tex.arraySize = t.arraySize;

				if (name.ends_with(":Depth"))
					tex.InitDepth(provider.renderSystem.core.device, w, h, rtvHeap, lastState);
				else
				{

					UINT flags = t.uav ? RenderTargetTexture::AllowUAV : RenderTargetTexture::AllowRenderTarget;
					tex.Init(provider.renderSystem.core.device, w, h, rtvHeap, t.format, lastState, flags);
				}

				tex.SetName(name);

				DescriptorManager::get().createTextureView(tex);
				if (t.uav)
					DescriptorManager::get().createUAVView(tex);

				provider.resources.textures.setNamedTexture(info.name + ":" + name, tex.view);
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

		if (!p.info.targets.empty())
		{
			if (renderToBackbuffer && p.info.targets.front().name == "Output")
			{
				p.target.texture = &provider.renderSystem.core.backbuffer[0];
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
			provider.renderSystem.core.StartCommandListNoMarker(syncCommands);
		if (pass.target.backbuffer)
			pass.target.texture = &provider.renderSystem.core.backbuffer[provider.renderSystem.core.frameIndex];

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
		{
			if (renderToBackbuffer)
				pass.target.texture->PrepareToPresent(syncCommands.commandList, pass.target.previousState);
			else
				pass.target.texture->PrepareAsView(syncCommands.commandList, pass.target.previousState);
		}
	}
	executeCommands();
}

const RenderTargetTexture* FrameCompositor::getTexture(const std::string& name) const
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
		if (p.info.name == name && p.task)
			return p.task.get();
	}

	return nullptr;
}

void FrameCompositor::executeCommands()
{
	for (auto& t : tasks)
	{
		if (t.finishEvents.empty())
		{
			for (auto& c : t.data)
			{
				provider.renderSystem.core.ExecuteCommandList(c);
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
					provider.renderSystem.core.ExecuteCommandList(t.data[index]);
				}
			}
		}
	}
}
