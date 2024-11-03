#include "FrameCompositor.h"

void FrameCompositor::initializeCommands()
{
	auto needsPassResources = [](const std::set<std::string>& pushedResources, const CompositorPass& pass)
		{
			return pushedResources.contains(pass.after)
				|| pushedResources.contains(pass.target)
				|| std::any_of(pass.inputs.begin(), pass.inputs.end(), [&pushedResources](const std::pair<std::string, UINT>& s) { return pushedResources.find(s.first) != pushedResources.end(); });
		};

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

			pushedAsyncTasks.clear();
			pushedAsyncResources.clear();
		};

	auto pushAsyncTasks = [&](const CompositorPass& pass, const AsyncTasksInfo& info)
		{
			if (needsPassResources(pushedAsyncResources, pass))
				prepareAsyncTasks();

			pushedAsyncTasks.insert(pushedAsyncTasks.end(), info.begin(), info.end());

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

	auto sz = passes.size();
	for (auto& pass : passes)
	{
		bool syncPass = false;

		if (pass.info.render == "Shadows")
		{
			pushAsyncTasks(pass.info, shadowRender.initialize(provider.renderSystem, sceneMgr));
		}
		else if (pass.info.render == "VoxelScene")
		{
			pushAsyncTasks(pass.info, sceneVoxelize.initialize(sceneMgr, pass.target));
		}
		else if (pass.info.render == "DebugOverlay")
		{
			debugOverlay.initialize(pass.target);
			syncPass = true;
		}
		else if (pass.info.render == "Test")
		{
			testTask.initialize(sceneMgr, pass.target);
			syncPass = true;
		}
		else if (pass.info.render == "Scene")
		{
			pushAsyncTasks(pass.info, sceneRender.initialize(sceneMgr, pass.target));
		}
		else if (pass.info.render == "SceneEarlyZ")
		{
			pushAsyncTasks(pass.info, sceneRender.initializeEarlyZ(sceneMgr));
		}
		else
		{
			syncPass = true;
		}

		pass.generalCommands = refreshSyncCommands(pass, syncPass);
	}

	prepareAsyncTasks();
	buildSyncCommands();
}
