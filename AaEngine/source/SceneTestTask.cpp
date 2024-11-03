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
	tmp.Init(provider.renderSystem->device, 512, 512, 2, heap, target->formats, true);
	tmp.SetName(L"tmpTex");

	ResourcesManager::get().createShaderResourceView(tmp);

	commands = provider.renderSystem->CreateCommandList(L"tempCmd");

	return {};
}

void SceneTestTask::prepare(RenderContext& ctx, CommandsData& c)
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

	//sceneMgr->renderables.updateTransformation();

	RenderInformation objInfo;
	ctx.renderables->updateRenderInformation(tmpCamera, objInfo);

// 	FrameGpuParameters gpuParams;
// 	gpuParams.time = 0;
// 	gpuParams.timeDelta = 0;
//	gpuParams.sunDirection = lights.directionalLight.direction;
//	XMStoreFloat4x4(&gpuParams.shadowMapViewProjectionTransposed, XMMatrixTranspose(shadowMap->camera[0].getViewProjectionMatrix()));

//	static auto commands = ctx.renderSystem->CreateCommandList(L"tempCmd");

	provider.renderSystem->StartCommandList(commands);

	tmp.PrepareAsTarget(commands.commandList, provider.renderSystem->frameIndex);

	tmpQueue.renderObjects(tmpCamera, objInfo, provider.params, commands.commandList, provider.renderSystem->frameIndex);

	tmp.PrepareAsView(commands.commandList, provider.renderSystem->frameIndex);
	provider.renderSystem->ExecuteCommandList(commands);
}

void SceneTestTask::finish()
{
	//renderSystem->ExecuteCommandList(commands);
}
