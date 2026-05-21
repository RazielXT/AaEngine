#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "RenderCore/VCT/isoSeparate/IsoSeparateVoxelization.h"
#include <thread>

struct RenderQueue;
class RenderWorld;

class VoxelizeSceneTask : public CompositorTask
{
public:

	VoxelizeSceneTask(RenderProvider provider, RenderWorld&, ShadowMaps& shadows);
	~VoxelizeSceneTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;

	void run(RenderContext& ctx, CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;
	void runCompute(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	static VoxelizeSceneTask& Get();

	void revoxelize();
	void clear(ID3D12GraphicsCommandList* list = nullptr);

	IsoSeparateVoxelTracingParams params;

	RunType getRunType(CompositorPass&) const override;

private:

	IsoSeparateVoxelization voxelization;

	CommandsData voxelizeCommands;
	HANDLE eventBegin{};
	HANDLE eventFinish{};
	std::thread worker;

	bool running = true;

	RenderContext ctx{};

	CbufferView frameCbuffer;

	ShadowMaps& shadowMaps;
};
