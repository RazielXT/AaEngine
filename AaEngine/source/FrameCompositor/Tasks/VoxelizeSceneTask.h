#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "RenderCore/VCT/anisoSeparate/AnisoSeparateVoxelization.h"
#include <thread>

struct RenderQueue;
class RenderWorld;

class VoxelizeSceneTask : public CompositorTask
{
public:

	VoxelizeSceneTask(RenderProvider provider, RenderWorld&, ShadowMaps& shadows);
	~VoxelizeSceneTask();

	AsyncTasksInfo buildAsyncTasks(CompositorPass& pass) override;

	void update(RenderContext& ctx, CompositorPass& pass) override;
	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	static VoxelizeSceneTask& Get();

	void revoxelize();
	void clear(ID3D12GraphicsCommandList* list = nullptr);

	AnisoSeparateVoxelTracingParams params;

	Execution getExecution(CompositorPass&) const override;

private:

	AnisoSeparateVoxelization voxelization;

	CommandsData voxelizeCommands;
	HANDLE eventBegin{};
	HANDLE eventFinish{};
	std::thread worker;

	bool running = true;

	RenderContext ctx{};

	CbufferView frameCbuffer;

	ShadowMaps& shadowMaps;
};
