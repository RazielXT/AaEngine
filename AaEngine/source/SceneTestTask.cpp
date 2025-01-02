#include "SceneTestTask.h"

SceneTestTask::SceneTestTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s)
{
}

SceneTestTask::~SceneTestTask()
{
}

AsyncTasksInfo SceneTestTask::initialize(CompositorPass& pass)
{
	tmpQueue = sceneMgr.createManualQueue();
	tmpQueue.targets = pass.target.textureSet->formats;

	heap.InitRtv(provider.renderSystem.core.device, tmpQueue.targets.size(), L"testRttHeap");
	textures.Init(provider.renderSystem.core.device, 512, 512, heap, tmpQueue.targets, D3D12_RESOURCE_STATE_RENDER_TARGET);
	textures.SetName("testRtt");

	DescriptorManager::get().createTextureView(textures);

	return {};
}

void SceneTestTask::run(RenderContext& ctx, CommandsData& commands, CompositorPass&)
{
	static bool initialized = false;
	if (!initialized)
	{
		initialized = true;

		tmpQueue.update({ EntityChange::Add, Order::Normal, sceneMgr.getEntity("Plane001") }, provider.resources);
		tmpQueue.update({ EntityChange::Add, Order::Normal, sceneMgr.getEntity("Torus001") }, provider.resources);
	}

	Camera tmpCamera;
	tmpCamera.setOrthographicCamera(100, 100, 1, 300);
	tmpCamera.setPosition({ 81, 100, -72 });
	tmpCamera.pitch(-90);
	tmpCamera.updateMatrix();

	static RenderObjectsVisibilityData sceneInfo;
	sceneMgr.getRenderables(Order::Normal)->updateVisibility(tmpCamera, sceneInfo);

	CommandsMarker marker(commands.commandList, "Test");

	textures.PrepareAsTarget(commands.commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	ShaderConstantsProvider constants(provider.params, sceneInfo, tmpCamera, textures);
	tmpQueue.renderObjects(constants, commands.commandList);

	//textures.PrepareAsView(commands.commandList, D3D12_RESOURCE_STATE_COMMON);
}

bool SceneTestTask::writesSyncCommands(CompositorPass&) const
{
	return true;
}
