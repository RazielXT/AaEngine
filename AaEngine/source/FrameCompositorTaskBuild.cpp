#include "FrameCompositor.h"
#include <set>
#include <algorithm>

void FrameCompositor::initializeTextureStates()
{
	//find last backbuffer write
	for (auto it = passes.rbegin(); it != passes.rend(); ++it)
	{
		if (it->info.target == "Backbuffer")
		{
			it->target.present = true;
			break;
		}
	}

	std::map<std::string, D3D12_RESOURCE_STATES> lastTextureStates;
	//iterate to get last states first
	for (auto& pass : passes)
	{
		lastTextureStates[pass.info.target] = pass.target.present ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_RENDER_TARGET;
		for (auto& i : pass.info.inputs)
			lastTextureStates[i.first] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	for (auto& pass : passes)
	{
		pass.target.previousState = lastTextureStates[pass.info.target];
		lastTextureStates[pass.info.target] = D3D12_RESOURCE_STATE_RENDER_TARGET;

		pass.inputs.resize(pass.info.inputs.size());
		for (UINT idx = 0; auto & i : pass.info.inputs)
		{
			pass.inputs[idx++].previousState = lastTextureStates[i.first];
			lastTextureStates[i.first] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
	}
}

D3D12_RESOURCE_STATES FrameCompositor::getInitialTextureState(const std::string& name) const
{
	for (auto& pass : passes)
	{
		if (pass.info.target == name)
			return pass.target.previousState;
	}

	return D3D12_RESOURCE_STATE_COMMON;
}

void FrameCompositor::initializeCommands()
{
	auto needsPassResources = [](const std::set<std::string>& pushedResources, const CompositorPassInfo& pass)
		{
			return pushedResources.contains(pass.after)
				|| pushedResources.contains(pass.target)
				|| std::any_of(pass.inputs.begin(), pass.inputs.end(), [&pushedResources](const std::pair<std::string, UINT>& s) { return pushedResources.find(s.first) != pushedResources.end(); });
		};

	std::vector<std::string> asyncNames;
	AsyncTasksInfo pushedAsyncTasks;
	std::set<std::string> pushedAsyncResources;

	auto prepareAsyncTasks = [&]()
		{
			if (pushedAsyncTasks.empty())
				return;

			TasksGroup& group = tasks.emplace_back();
			for (auto& t : pushedAsyncTasks)
			{
				group.data.push_back(t.commands);
				group.finishEvents.push_back(t.finishEvent);
			}
			group.pass = std::move(asyncNames);

			pushedAsyncTasks.clear();
			pushedAsyncResources.clear();
		};

	auto pushAsyncTasks = [&](const CompositorPassInfo& pass, const AsyncTasksInfo& tasks, bool forceOrder)
		{
			if (needsPassResources(pushedAsyncResources, pass))
				prepareAsyncTasks();

			if (!forceOrder)
			{
				asyncNames.push_back(pass.name);
				pushedAsyncTasks.insert(pushedAsyncTasks.end(), tasks.begin(), tasks.end());
			}
			else
			{
				for (int i = 0; auto& t : tasks)
				{
					if (i++)
						prepareAsyncTasks();

					asyncNames.push_back(pass.name);
					pushedAsyncTasks.push_back(t);
				}
			}

			if (!pass.name.empty())
				pushedAsyncResources.insert(pass.name);

			if (!pass.target.empty())
				pushedAsyncResources.insert(pass.target);
		};

	CommandsData generalCommands{};
	auto buildSyncCommands = [&]()
		{
			if (generalCommands.commandList)
				tasks.push_back({ {}, { generalCommands } });
		};
	auto refreshSyncCommands = [&](FrameCompositor::PassData& passData, bool syncPass)
		{
			static uint32_t syncCount = 0;

			if (!generalCommands.commandList || (!syncPass && syncCount && needsPassResources(pushedAsyncResources, passData.info)))
			{
				buildSyncCommands();
				generalCommands = generalCommandsArray.emplace_back(provider.renderSystem->CreateCommandList(L"Compositor"));
				passData.startCommands = true;
				syncCount = 0;
			}
			syncCount += syncPass;
			return generalCommands;
		};

	for (auto& pass : passes)
	{
		if (pass.task)
			pushAsyncTasks(pass.info, pass.task->initialize(pass), pass.task->forceTaskOrder());

		pass.generalCommands = refreshSyncCommands(pass, !pass.task || pass.task->writesSyncCommands());
	}

	prepareAsyncTasks();
	buildSyncCommands();
}
