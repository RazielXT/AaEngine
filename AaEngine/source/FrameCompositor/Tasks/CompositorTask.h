#pragma once

#include "FrameCompositor/RenderContext.h"
#include "FrameCompositor/CompositorFileParser.h"

class RenderWorld;

struct CompositorPass
{
	CompositorPassInfo& info;

	std::vector<GpuTextureStates> inputs;
	std::vector<GpuTextureStates> targets;

	std::unique_ptr<RenderTargetTexturesView> mrt;
	bool present = false;
	bool backbuffer = false;
};

class CompositorTask
{
public:

	CompositorTask(RenderProvider& p, RenderWorld& w) : provider(p), renderWorld(w) {};
	virtual ~CompositorTask() = default;

	// One-time setup (resources, PSOs, render queues). No command recording here.
	virtual void initialize(CompositorPass& pass) {};
	// Declares command lists recorded on the task's own worker thread, collected by the compositor.
	// Only meaningful for tasks whose execution RecordMode is Threaded.
	virtual AsyncTasksInfo buildAsyncTasks(CompositorPass& pass) { return {}; };
	virtual void resize(CompositorPass& pass) {};

	// How the task records its work for a given pass.
	enum class RecordMode
	{
		None,     // records nothing this pass (data preparation only)
		Threaded, // records on its own worker thread into its own command list
		Inline,   // records inline into the compositor's shared command list
	};
	// Which GPU queue the recorded commands target.
	enum class Queue
	{
		Graphics,
		Compute,  // async compute queue (cross-queue synchronized)
	};
	struct Execution
	{
		RecordMode record = RecordMode::Inline;
		Queue queue = Queue::Graphics;

		bool isInline() const { return record == RecordMode::Inline; }
		bool isInlineGraphics() const { return record == RecordMode::Inline && queue == Queue::Graphics; }
		bool isInlineCompute() const { return record == RecordMode::Inline && queue == Queue::Compute; }
	};
	virtual Execution getExecution(CompositorPass&) const = 0;

	// Per-frame work that does not record into the shared command list (kick a worker thread, prepare data).
	virtual void update(RenderContext& ctx, CompositorPass& pass) {};
	// Records inline into the shared command list. The queue (graphics/compute) is determined by the
	// list passed in, matching getExecution().queue.
	virtual void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) {};

	virtual bool forceTaskOrder() const { return false; }

protected:

	RenderProvider provider;
	RenderWorld& renderWorld;
};
