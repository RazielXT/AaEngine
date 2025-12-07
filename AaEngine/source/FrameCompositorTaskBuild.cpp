#include "FrameCompositor.h"
#include <set>
#include <algorithm>

void FrameCompositor::initializeTextureStates()
{
	//find last backbuffer write
	for (auto it = passes.rbegin(); it != passes.rend(); ++it)
	{
		if (!it->info.targets.empty() && it->info.targets.front().name == "Output")
		{
			it->target.present = true;
			break;
		}
	}

	//iterate to get last states first
	for (auto& pass : passes)
	{
		for (auto& [name, flags] : pass.info.inputs)
		{
			auto& state = lastTextureStates[name];
			if (flags & Compositor::PixelShader)
				state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			else if (flags & Compositor::ComputeShader && info.textures[name].uav && !(flags & Compositor::Read))
				state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			else
				state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}
		for (auto& [name, flags] : pass.info.targets)
		{
			auto& state = lastTextureStates[name];
			if (pass.target.present)
				state = config.renderToBackbuffer ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			else if (name.ends_with(":Depth"))
			{
				if (flags & Compositor::DepthRead)
					state = D3D12_RESOURCE_STATE_DEPTH_READ;
				else
					state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			}
			else
				state = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
	}

	for (auto& pass : passes)
	{
		pass.inputs.resize(pass.info.inputs.size());
		for (UINT idx = 0; auto & [name, flags] : pass.info.inputs)
		{
			auto& tState = lastTextureStates[name];
			pass.inputs[idx++].previousState = tState;

			if (flags & Compositor::PixelShader)
				tState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			else if (flags & Compositor::ComputeShader && info.textures[name].uav && !(flags & Compositor::Read))
				tState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			else
				tState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		if (!pass.info.targets.empty())
		{
			auto& [targetName, flags] = pass.info.targets[0];
			auto& lastState = lastTextureStates[targetName];
			pass.target.previousState = lastState;

			if (pass.info.targets.size() == 1)
			{
				if (pass.target.present)
					lastState = config.renderToBackbuffer ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				else if (targetName.ends_with(":Depth"))
				{
					if (flags & Compositor::DepthRead)
						lastState = D3D12_RESOURCE_STATE_DEPTH_READ;
					else
						lastState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				}
				else
					lastState = D3D12_RESOURCE_STATE_RENDER_TARGET;
			}
			else
			{
				pass.target.textureSet = std::make_unique<RenderTargetTexturesView>();
				auto textureSet = pass.target.textureSet.get();

				for (auto& [targetName, flags] : pass.info.targets)
				{
					auto& lastState = lastTextureStates[targetName];

					if (targetName.ends_with(":Depth"))
					{
						textureSet->depthState.previousState = lastState;

						if (flags & Compositor::DepthRead)
							lastState = D3D12_RESOURCE_STATE_DEPTH_READ;
						else
							lastState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
					}
					else
					{
						textureSet->texturesState.push_back({ nullptr, lastState });
						lastState = D3D12_RESOURCE_STATE_RENDER_TARGET;
					}
				}
			}
		}
	}
}

void FrameCompositor::initializeCommands()
{
	taskFences.clear();
	tasks.clear();
	for (auto& c : generalCommandsArray)
	{
		c.deinit();
	}
	generalCommandsArray.clear();

	auto initFence = [this](const std::string& name)
		{
			if (auto& f = taskFences[name]; !f.fence)
				 provider.renderSystem.core.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&f.fence));
		};
	for (auto& pass : passes)
	{
		if (!pass.info.syncSignal.empty())
			initFence(pass.info.syncSignal);
		if (!pass.info.syncWait.empty())
			initFence(pass.info.syncWait);
	}

	CommandsData syncCommands{};
	std::optional<CommandsData> syncComputeCommands{};
	std::vector<CompositorPassInfo*> syncPasses;
	AsyncTasksInfo asyncTasks;
	std::vector<CompositorPassInfo*> asyncPasses;

	auto buildLastTasksFences = [&]()
		{
			auto& t = tasks.back();
			for (auto pass : t.pass)
			{
				if (!pass->syncSignal.empty())
					t.syncSignal.push_back(&taskFences[pass->syncSignal]);
				if (!pass->syncWait.empty())
					t.syncWait.push_back(&taskFences[pass->syncWait]);
			}
		};
	auto buildSyncCommands = [&]()
		{
			if (syncCommands.commandList)
			{
				tasks.push_back({ {}, { syncCommands }, std::move(syncPasses) });
				
				if (syncComputeCommands)
					tasks.back().data.push_back(*syncComputeCommands);
			}

			syncCommands = {};
			syncComputeCommands.reset();
			buildLastTasksFences();
		};
	auto buildAsyncCommands = [&]()
		{
			if (asyncTasks.empty())
				return;

			TasksGroup& group = tasks.emplace_back();
			for (auto& t : asyncTasks)
			{
				group.data.push_back(t.commands);
				group.finishEvents.push_back(t.finishEvent);
			}
			group.pass = std::move(asyncPasses);
			asyncTasks.clear();
			buildLastTasksFences();
		};
	auto pushAsyncTasks = [&](CompositorPassInfo& pass, const AsyncTasksInfo& tasks, bool forceOrder)
		{
			if (tasks.empty())
				return;

			if (!forceOrder)
			{
				asyncPasses.push_back(&pass);
				asyncTasks.insert(asyncTasks.end(), tasks.begin(), tasks.end());
			}
			else
			{
				for (int i = 0; auto& t : tasks)
				{
					if (i++)
						buildAsyncCommands();

					asyncPasses.push_back(&pass);
					asyncTasks.push_back(t);
				}
			}
		};
	auto pushSyncCommands = [&](FrameCompositor::PassData& passData)
		{
			if (!syncCommands.commandList)
			{
				syncCommands = generalCommandsArray.emplace_back(provider.renderSystem.core.CreateCommandList(L"Compositor", PixColor::Compositor));
				passData.startCommands = true;
			}
			if (passData.task && passData.task->writesSyncComputeCommands(passData))
			{
				syncComputeCommands = generalCommandsArray.emplace_back(provider.renderSystem.core.CreateCommandList(L"CompositorCompute", PixColor::Compositor, D3D12_COMMAND_LIST_TYPE_COMPUTE));
			}
			passData.generalCommands = syncCommands;
			passData.computeCommands = syncComputeCommands;
			syncPasses.push_back(&passData.info);
		};
	auto dependsOnPreviousPass = [](const CompositorPassInfo& pass, const std::vector<CompositorPassInfo*>& passes, bool forceTargetOrder = false)
		{
			for (auto& target : passes)
			{
				if (pass.after == target->name)
					return true;

				for (auto& input : target->inputs)
				{
					for (auto& myInput : pass.inputs)
					{
						if (myInput.name == input.name && myInput.flags != input.flags)
							return true;
					}
				}
				for (auto& target : target->targets)
				{
					for (auto& myTarget : pass.targets)
					{
						if (myTarget.name == target.name && myTarget.flags != target.flags)
							return true;
					}

					if (forceTargetOrder)
					{
						for (auto& myInput : pass.inputs)
						{
							if (myInput.name == target.name)
								return true;
						}
						for (auto& myTarget : pass.targets)
						{
							if (myTarget.name == target.name)
								return true;
						}
					}
				}
			}
			return false;
		};

	for (auto& pass : passes)
	{
		auto isSync = !pass.task || pass.task->writesSyncCommands(pass) || pass.task->writesSyncComputeCommands(pass);

		if (dependsOnPreviousPass(pass.info, asyncPasses, true))
			buildAsyncCommands();
		if (!isSync && dependsOnPreviousPass(pass.info, syncPasses, !isSync))
			buildSyncCommands();

		if (pass.task)
 			pushAsyncTasks(pass.info, pass.task->initialize(pass), pass.task->forceTaskOrder());

		if (isSync)
			pushSyncCommands(pass);
	}

	buildAsyncCommands();
	buildSyncCommands();
}
