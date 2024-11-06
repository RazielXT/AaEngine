#include "SceneTestTask.h"

SceneTestTask::SceneTestTask(RenderProvider p) : provider(p)
{

}

SceneTestTask::~SceneTestTask()
{

}

CommandsData commands;

AsyncTasksInfo SceneTestTask::initialize(AaSceneManager* s, RenderTargetTexture* target)
{
	sceneMgr = s;

	tmpQueue = sceneMgr->createManualQueue();
	tmpQueue.targets = target->formats;

	heap.Init(provider.renderSystem->device, target->formats.size(), provider.renderSystem->FrameCount, L"tempHeap");
	tmp.Init(provider.renderSystem->device, 512, 512, 2, heap, target->formats, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	tmp.SetName(L"tmpTex");

	ResourcesManager::get().createShaderResourceView(tmp);

	commands = provider.renderSystem->CreateCommandList(L"tempCmd");

	return {};
}

void SceneTestTask::run(RenderContext& ctx, CommandsData& c)
{
	static bool initialize = false;
	if (!initialize)
	{
		initialize = true;

		tmpQueue.update({ { EntityChange::Add, sceneMgr->getEntity("Plane001") } });
		tmpQueue.update({ { EntityChange::Add, sceneMgr->getEntity("Torus001") } });
	}

	AaCamera tmpCamera;
	tmpCamera.setOrthographicCamera(100, 100, 1, 300);
	tmpCamera.setPosition({ 81, 100, -72 });
	tmpCamera.pitch(-90);

	RenderInformation objInfo;
	ctx.renderables->updateRenderInformation(tmpCamera, objInfo);

	provider.renderSystem->StartCommandList(commands);

	tmp.PrepareAsTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_COMMON);

	tmpQueue.renderObjects(tmpCamera, objInfo, provider.params, commands.commandList, provider.renderSystem->frameIndex);

	tmp.PrepareAsView(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_COMMON);
	provider.renderSystem->ExecuteCommandList(commands);
}
