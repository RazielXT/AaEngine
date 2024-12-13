#include "FrameCompositor.h"
#include <set>
#include <algorithm>

void FrameCompositor::initializeTextureStates()
{
	//find last backbuffer write
	for (auto it = passes.rbegin(); it != passes.rend(); ++it)
	{
		if (!it->info.targets.empty() && it->info.targets.front().name == "Backbuffer")
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
				state = D3D12_RESOURCE_STATE_PRESENT;
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
				if (targetName.ends_with(":Depth"))
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
	tasks.clear();
	for (auto& c : generalCommandsArray)
	{
		c.deinit();
	}
	generalCommandsArray.clear();

	CommandsData syncCommands{};
	std::vector<CompositorPassInfo*> syncPasses;
	AsyncTasksInfo asyncTasks;
	std::vector<CompositorPassInfo*> asyncPasses;

	auto buildSyncCommands = [&]()
		{
			if (syncCommands.commandList)
				tasks.push_back({ {}, { syncCommands }, std::move(syncPasses) });

			syncCommands = {};
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
				syncCommands = generalCommandsArray.emplace_back(provider.renderSystem->CreateCommandList(L"Compositor"));
				passData.startCommands = true;
			}
			passData.generalCommands = syncCommands;
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
		auto isSync = !pass.task || pass.task->writesSyncCommands();

		if (dependsOnPreviousPass(pass.info, asyncPasses, true))
			buildAsyncCommands();
		if (dependsOnPreviousPass(pass.info, syncPasses, !isSync))
			buildSyncCommands();

		if (pass.task)
 			pushAsyncTasks(pass.info, pass.task->initialize(pass), pass.task->forceTaskOrder());

		if (isSync)
			pushSyncCommands(pass);
	}

	buildAsyncCommands();
	buildSyncCommands();
}
