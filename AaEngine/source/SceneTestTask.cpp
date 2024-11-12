#include "SceneTestTask.h"

SceneTestTask::SceneTestTask(RenderProvider p, AaSceneManager& s) : CompositorTask(p, s)
{

}

SceneTestTask::~SceneTestTask()
{

}

CommandsData commands;

AsyncTasksInfo SceneTestTask::initialize(CompositorPass& pass)
{
	tmpQueue = sceneMgr.createManualQueue();
	tmpQueue.targets = pass.target.texture->formats;

	heap.Init(provider.renderSystem->device, tmpQueue.targets.size(), provider.renderSystem->FrameCount, L"tempHeap");
	tmp.Init(provider.renderSystem->device, 512, 512, 2, heap, tmpQueue.targets, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	tmp.SetName(L"tmpTex");

	DescriptorManager::get().createTextureView(tmp);

	commands = provider.renderSystem->CreateCommandList(L"tempCmd");

	return {};
}

void SceneTestTask::run(RenderContext& ctx, CommandsData& c, CompositorPass&)
{
	static bool initialize = false;
	if (!initialize)
	{
		initialize = true;

		tmpQueue.update({ { EntityChange::Add, sceneMgr.getEntity("Plane001") } });
		tmpQueue.update({ { EntityChange::Add, sceneMgr.getEntity("Torus001") } });
	}

	AaCamera tmpCamera;
	tmpCamera.setOrthographicCamera(100, 100, 1, 300);
	tmpCamera.setPosition({ 81, 100, -72 });
	tmpCamera.pitch(-90);
	tmpCamera.updateMatrix();

	static RenderInformation objInfo{ sceneMgr.getRenderables(Order::Normal) };
	objInfo.updateVisibility(tmpCamera);

	provider.renderSystem->StartCommandList(commands);

	tmp.PrepareAsTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_COMMON);

	tmpQueue.renderObjects(tmpCamera, objInfo, provider.params, commands.commandList, provider.renderSystem->frameIndex);

	tmp.PrepareAsView(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_COMMON);
	provider.renderSystem->ExecuteCommandList(commands);
}

bool SceneTestTask::isSync() const
{
	return true;
}
