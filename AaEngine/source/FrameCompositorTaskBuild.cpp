#include "FrameCompositor.h"
#include <set>
#include <algorithm>
#include <ranges>

void FrameCompositor::initializeTextureStates()
{
	//find last backbuffer write
	for (auto it = passes.rbegin(); it != passes.rend(); ++it)
	{
		if (it->info.targets.size() == 1 && it->info.targets.front().name == "Output")
		{
			it->present = true;
			break;
		}
	}

	using PassStartCb = std::function<void(FrameCompositor::PassData&)>;
	const PassStartCb EmptyPassStartCb = [](FrameCompositor::PassData&) {};
	using TextureStateCb = std::function<void(const std::string& name, Compositor::UsageFlags flags, GpuTextureStates& t, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES stateOut)>;
	const TextureStateCb EmptyTextureStateCb = [](const std::string&, Compositor::UsageFlags, GpuTextureStates&, D3D12_RESOURCE_STATES, D3D12_RESOURCE_STATES) {};

	auto iteratePasses = [this](TextureStateCb setTextureState, PassStartCb passStart)
		{
			for (auto& pass : passes)
			{
				passStart(pass);

				for (UINT idx = 0; auto& [name, flags] : pass.info.inputs)
				{
					if (flags & Compositor::PixelShader)
						setTextureState(name, flags, pass.inputs[idx++], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
					else if (flags & Compositor::ComputeShader && info.textures[name].uav && flags & Compositor::Write)
					{
						if (!(flags & Compositor::Read))
							setTextureState(name, flags, pass.inputs[idx++], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
						else if (flags & Compositor::WriteFirst)
							setTextureState(name, flags, pass.inputs[idx++], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
						else
							setTextureState(name, flags, pass.inputs[idx++], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					}
					else
						setTextureState(name, flags, pass.inputs[idx++], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
				}
				for (UINT idx = 0; auto& [name, flags] : pass.info.targets)
				{
					if (flags & Compositor::ComputeShader)
						setTextureState(name, flags, pass.targets[idx++], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
					else if (flags & Compositor::PixelShader)
					{
						if (name.ends_with(":Depth"))
						{
							if (flags & Compositor::DepthRead)
								setTextureState(name, flags, pass.targets[idx++], D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_COMMON);
							else
								setTextureState(name, flags, pass.targets[idx++], D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON);
						}
						else if (pass.present)
							setTextureState(name, flags, pass.targets[idx++], config.renderToBackbuffer ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
						else
							setTextureState(name, flags, pass.targets[idx++], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
					}
				}
			}
		};
	auto isReadState = [](D3D12_RESOURCE_STATES state)
		{
			const D3D12_RESOURCE_STATES ReadFlags = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ;
			return (state & ~ReadFlags) == 0;
		};
	auto isCompatibleState = [isReadState](D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES state2)
		{
			return isReadState(state) && isReadState(state2);
		};

	//---------------------------------------------------
	{
		// initialize data storage
		for (auto& pass : passes)
		{
			pass.inputs.resize(pass.info.inputs.size());
			pass.targets.resize(pass.info.targets.size());

			if (pass.info.mrt)
				pass.mrt = std::make_unique<RenderTargetTexturesView>();
		}
	}
	//---------------------------------------------------
	{
		struct AccumulatedTextureInfo
		{
			Compositor::UsageFlags lastFlags{};
			std::set<std::string> signals;
		};
		std::map<std::string, AccumulatedTextureInfo> texturesInfo;
		std::set<std::string> waits;

		// validate access across queues, identify missing synchronization
		for (auto& pass : passes)
		{
			for (auto& sync : pass.info.sync)
			{
				if (!sync.signal)
					waits.insert(sync.name);
			}

			auto validateTexture = [&](const std::string& name, Compositor::UsageFlags flags)
				{
					auto& info = texturesInfo[name];

					if (info.lastFlags)
					{
						if ((info.lastFlags & Compositor::Async) != (flags & Compositor::Async))
						{
							bool isSynced = false;
							for (auto& sync : info.signals)
								isSynced |= waits.contains(sync);

							if (!isSynced)
								__debugbreak(); // unsynced cross queue access

							// brand new sync will be needed for next cross queue access
							info.signals.clear();
						}
					}

					info.lastFlags = flags;

					for (auto& sync : pass.info.sync)
					{
						if (sync.signal)
							info.signals.insert(sync.name);
					}
				};

			for (auto& [name, flags] : pass.info.inputs)
				validateTexture(name, flags);
			for (auto& [name, flags] : pass.info.targets)
				validateTexture(name, flags);

			// all previous textures in same queue get behind new signals
			for (auto& sync : pass.info.sync)
			{
				if (sync.signal)
				{
					for (auto& t : texturesInfo)
					{
						if ((t.second.lastFlags & Compositor::Async) == (pass.info.flags & Compositor::Async))
							t.second.signals.insert(sync.name);
					}
				}
			}
		}
	}
	//---------------------------------------------------
	{
		struct TextureUseInfo
		{
			Compositor::UsageFlags flags;
			GpuTextureStates* states;
			FrameCompositor::PassData* pass{};
			bool forceTransition = false;
		};
		std::map<std::string, std::vector<TextureUseInfo>> textureUsage;

		FrameCompositor::PassData* currentPass{};
		auto passStart = [&](FrameCompositor::PassData& pass) { currentPass = &pass; };
		auto setInitialTextureState = [&](const std::string& name, Compositor::UsageFlags f, GpuTextureStates& t, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES stateOut)
			{
				t.state = state;
				t.nextState = stateOut;

				textureUsage[name].emplace_back(f, &t, currentPass);
			};
		iteratePasses(setInitialTextureState, passStart);

		for (auto& [name, usage] : textureUsage)
		{
			std::vector<std::pair<D3D12_RESOURCE_STATES, std::vector<TextureUseInfo>>> mergedTextureStates;

			// flag if async compute transition will be needed, cant allow outside PS states inside async compute then
			for (size_t i = 0; i < usage.size(); i++)
			{
				if (usage[i].flags & Compositor::Async)
				{
					bool transition = false;
					size_t j = i;

					for (; j < usage.size(); j++)
					{
						if (!(usage[j].flags & Compositor::Async))
							break;

						if (!isCompatibleState(usage[i].states->state, usage[j].states->state) || usage[i].states->nextState != D3D12_RESOURCE_STATE_COMMON)
							transition = true;
					}

					usage[i].forceTransition = transition;
					i = j;
				}
			}

			// merge compatible states
			for (auto& t : usage)
			{
				if (!mergedTextureStates.empty() && !t.forceTransition && isCompatibleState(mergedTextureStates.back().first, t.states->state))
				{
					mergedTextureStates.back().first |= t.states->state;
					mergedTextureStates.back().second.push_back(t);
				}
				else
					mergedTextureStates.push_back({ t.states->state, { t } });
			}

			// merge end-begin
			if (mergedTextureStates.size() > 1 && isCompatibleState(mergedTextureStates.front().first, mergedTextureStates.back().first))
			{
				mergedTextureStates.front().first |= mergedTextureStates.back().first;
				mergedTextureStates.back().first = mergedTextureStates.front().first;
			}

			// assign back merged states
			for (auto& [mergedState, usage] : mergedTextureStates)
			{
				for (auto& u : usage)
					u.states->state = mergedState;
			}

			// prev / next
			for (size_t i = 0; i < usage.size(); i++)
			{
				if (i == 0)
					usage[i].states->previousState = usage.back().states->nextState ? usage.back().states->nextState : usage.back().states->state;
				else
					usage[i].states->previousState = usage[i - 1].states->nextState ? usage[i - 1].states->nextState : usage[i - 1].states->state;
			}
			for (size_t i = 0; i < usage.size(); i++)
			{
				if (!usage[i].states->nextState)
				{
					if (i == usage.size() - 1)
						usage[i].states->nextState = usage.front().states->state;
					else
						usage[i].states->nextState = usage[i + 1].states->state;
				}
			}

			// if state is used/shared with async compute, change to this state at the end last state usage (presuming before signal sync)
			// compute also cant transition non compute states so needs to be outside
			if (mergedTextureStates.size() > 1)
				for (size_t i = 0; i < mergedTextureStates.size(); i++)
				{
					auto& usages = mergedTextureStates[i].second;
					bool asyncShared = false;

					for (auto& usage : usages)
						asyncShared |= bool(usage.flags & Compositor::Async);

					if (asyncShared)
					{
						auto& prev = (i == 0) ? mergedTextureStates.back() : mergedTextureStates[i - 1];
						auto& prevStateLastUsage = prev.second.back();
						prevStateLastUsage.pass->postTransition.push_back(prevStateLastUsage.states);

						// presume its already transitioned
						auto& stateFirstUsage = usages.front();
						stateFirstUsage.states->previousState = stateFirstUsage.states->state;
					}
				}
		}

		for (auto& [name, usage] : textureUsage)
		{
			initialTextureStates[name] = usage.front().states->previousState;
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
			{
				provider.renderSystem.core.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&f.fence));
				f.name = name;
			}
		};
	for (auto& pass : passes)
	{
		for (auto& s : pass.info.sync)
			initFence(s.name);
	}

	std::optional<CommandsData> syncCommands{};
	std::optional<CommandsData> syncComputeCommands{};
	std::vector<CompositorPassInfo*> syncPasses;
	AsyncTasksInfo asyncTasks;
	std::vector<CompositorPassInfo*> asyncPasses;

	auto buildLastTasksFences = [&]()
		{
			auto& t = tasks.back();
			for (auto pass : t.passes)
			{
				for (auto& s : pass->sync)
				{
					auto queue = s.compute ? provider.renderSystem.core.computeQueue : provider.renderSystem.core.commandQueue;
					if (s.signal)
						t.syncSignal.emplace_back(queue, &taskFences[s.name]);
					else
						t.syncWait.emplace_back(queue, &taskFences[s.name]);
				}
			}
		};
	auto buildSyncCommands = [&]()
		{
			if (syncCommands || syncComputeCommands)
			{
				auto& taskData = tasks.emplace_back();
				taskData.passes = std::move(syncPasses);

				if (syncCommands)
					taskData.data.push_back(*syncCommands);
				if (syncComputeCommands)
					taskData.data.push_back(*syncComputeCommands);

				buildLastTasksFences();
			}

			syncCommands.reset();
			syncComputeCommands.reset();
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
			group.passes = std::move(asyncPasses);
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
			if (!syncCommands && (!passData.task || passData.task->writesSyncCommands(passData)))
			{
				syncCommands = generalCommandsArray.emplace_back(provider.renderSystem.core.CreateCommandList(L"Compositor", PixColor::Compositor));
				passData.startCommands = true;
			}
			if (!syncComputeCommands && passData.task && passData.task->writesSyncComputeCommands(passData))
			{
				syncComputeCommands = generalCommandsArray.emplace_back(provider.renderSystem.core.CreateCommandList(L"CompositorCompute", PixColor::Compositor, D3D12_COMMAND_LIST_TYPE_COMPUTE));
				passData.startComputeCommands = true;
			}
			passData.syncCommands = syncCommands;
			passData.computeCommands = syncComputeCommands;
			syncPasses.push_back(&passData.info);
		};
	auto dependsOnPreviousPass = [](const CompositorPassInfo& pass, const std::vector<CompositorPassInfo*>& passes, bool forceTargetOrder = false)
		{
			for (auto& s : pass.sync)
			{
				if (!s.signal) //wait
					return true;
			}

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

		for (auto& s : pass.info.sync)
		{
			if (s.signal)
			{
				buildAsyncCommands();
				buildSyncCommands();
			}
		}
	}

	buildAsyncCommands();
	buildSyncCommands();
}
