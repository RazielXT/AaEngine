#include "SceneTestTask.h"

SceneTestTask::SceneTestTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s)
{

}

SceneTestTask::~SceneTestTask()
{

}

CommandsData commands;

AsyncTasksInfo SceneTestTask::initialize(CompositorPass& pass)
{
	tmpQueue = sceneMgr.createManualQueue();
	tmpQueue.targets = { pass.target.texture->format };

	heap.InitRtv(provider.renderSystem->device, tmpQueue.targets.size(), L"tempHeap");
	tmp.Init(provider.renderSystem->device, 512, 512, heap, pass.target.texture->format, D3D12_RESOURCE_STATE_RENDER_TARGET);
	tmp.SetName("tmpTex");

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

		tmpQueue.update({ { EntityChange::Add, Order::Normal, sceneMgr.getEntity("Plane001") } });
		tmpQueue.update({ { EntityChange::Add, Order::Normal, sceneMgr.getEntity("Torus001") } });
	}

	AaCamera tmpCamera;
	tmpCamera.setOrthographicCamera(100, 100, 1, 300);
	tmpCamera.setPosition({ 81, 100, -72 });
	tmpCamera.pitch(-90);
	tmpCamera.updateMatrix();

	static RenderObjectsVisibilityData sceneInfo;
	sceneMgr.getRenderables(Order::Normal)->updateVisibility(tmpCamera, sceneInfo);

	auto marker = provider.renderSystem->StartCommandList(commands);

	tmp.PrepareAsTarget(commands.commandList, D3D12_RESOURCE_STATE_COMMON);

// 	ShaderConstantsProvider constants(provider.params, sceneInfo, tmpCamera, tmp);
// 	tmpQueue.renderObjects(constants, commands.commandList);
// 
// 	tmp.PrepareAsView(commands.commandList, D3D12_RESOURCE_STATE_COMMON);
// 	provider.renderSystem->ExecuteCommandList(commands);
}

bool SceneTestTask::writesSyncCommands() const
{
	return true;
}
