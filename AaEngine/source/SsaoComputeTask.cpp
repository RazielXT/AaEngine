#include "SsaoComputeTask.h"
#include "DescriptorManager.h"
#include "AaCamera.h"

SsaoComputeTask::SsaoComputeTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s)
{
}

SsaoComputeTask::~SsaoComputeTask()
{
	running = false;

	if (eventBegin)
	{
		SetEvent(eventBegin);
		worker.join();
		commands.deinit();
		CloseHandle(eventBegin);
		CloseHandle(eventFinish);
	}
}

AsyncTasksInfo SsaoComputeTask::initialize(CompositorPass& pass)
{
	eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	commands = provider.renderSystem->CreateCommandList(L"SSAO");

	prepareDepthBuffersCS.init(provider.renderSystem->device, "AoPrepareDepthBuffers");
	prepareDepthBuffers2CS.init(provider.renderSystem->device, "AoPrepareDepthBuffers2");
	aoRenderCS.init(provider.renderSystem->device, "AoRenderCS");
	aoRenderInterleaveCS.init(provider.renderSystem->device, "AoRenderInterleaveCS");
	aoBlurAndUpsampleCS.init(provider.renderSystem->device, "AoBlurAndUpsampleCS");
	aoBlurAndUpsampleFinalCS.init(provider.renderSystem->device, "AoBlurAndUpsampleFinalCS");

	initializeResources(pass);

	worker = std::thread([this]
		{
			while (WaitForSingleObject(eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				provider.renderSystem->StartCommandList(commands);

				//downsample depth
				{
					auto input = textures.sceneDepth;
					input->TransitionDepth(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

					prepareDepthBuffersCS.dispatch(input->width, input->height, input->depthView, commands.commandList, provider.renderSystem->frameIndex);
			
					input->TransitionDepth(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				}
				{
					auto input = textures.linearDepthDownsample4;
					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					prepareDepthBuffers2CS.dispatch(input->width, input->height, input->textureView(), commands.commandList, provider.renderSystem->frameIndex);

					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				}

				//occlusion
				{
					auto input = textures.linearDepthDownsampleAtlas8;
					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					auto output = textures.occlusionInterleaved8;
					aoRenderInterleaveCS.dispatch(input->width, input->height, input->arraySize, TanHalfFovH, input->textureView(), output->textureView(), commands.commandList, provider.renderSystem->frameIndex);

					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				}
				{
					auto input = textures.linearDepthDownsample8;
					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					auto output = textures.occlusion8;
					aoRenderCS.dispatch(input->width, input->height, input->arraySize, TanHalfFovH, input->textureView(), output->textureView(), commands.commandList, provider.renderSystem->frameIndex);

					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				}
				{
					auto input = textures.linearDepthDownsampleAtlas4;
					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					auto output = textures.occlusionInterleaved4;
					aoRenderInterleaveCS.dispatch(input->width, input->height, input->arraySize, TanHalfFovH, input->textureView(), output->textureView(), commands.commandList, provider.renderSystem->frameIndex);

					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				}
				{
					auto input = textures.linearDepthDownsample4;
					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					auto output = textures.occlusion4;
					aoRenderCS.dispatch(input->width, input->height, input->arraySize, TanHalfFovH, input->textureView(), output->textureView(), commands.commandList, provider.renderSystem->frameIndex);

					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				}
				{
					auto input = textures.linearDepthDownsampleAtlas2;
					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					auto output = textures.occlusionInterleaved2;
					aoRenderInterleaveCS.dispatch(input->width, input->height, input->arraySize, TanHalfFovH, input->textureView(), output->textureView(), commands.commandList, provider.renderSystem->frameIndex);

					input->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				}

				//upscale

				textures.linearDepthDownsample8->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				textures.linearDepthDownsample4->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				textures.linearDepthDownsample2->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				textures.linearDepth->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				textures.occlusionInterleaved8->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				textures.occlusion8->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				textures.occlusionInterleaved4->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				textures.occlusion4->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				textures.occlusionInterleaved2->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				{
					aoBlurAndUpsampleCS.data.ResIdLoResDB = textures.linearDepthDownsample8->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdHiResDB = textures.linearDepthDownsample4->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdLoResAO1 = textures.occlusionInterleaved8->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdLoResAO2 = textures.occlusion8->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdHiResAO = textures.occlusionInterleaved4->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdAoResult = textures.aoSmooth4->uavHeapIndex();

					aoBlurAndUpsampleCS.dispatch(textures.aoSmooth->width, textures.aoSmooth4->height, textures.aoSmooth4->width, textures.occlusion8->height, textures.occlusion8->width,
						commands.commandList, provider.renderSystem->frameIndex);
				}
				textures.aoSmooth4->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				{
					aoBlurAndUpsampleCS.data.ResIdLoResDB = textures.linearDepthDownsample4->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdHiResDB = textures.linearDepthDownsample2->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdLoResAO1 = textures.aoSmooth4->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdLoResAO2 = textures.occlusion4->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdHiResAO = textures.occlusionInterleaved2->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdAoResult = textures.aoSmooth2->uavHeapIndex();

					aoBlurAndUpsampleCS.dispatch(textures.aoSmooth->width, textures.aoSmooth2->height, textures.aoSmooth2->width, textures.occlusion4->height, textures.occlusion4->width,
						commands.commandList, provider.renderSystem->frameIndex);
				}
				textures.aoSmooth2->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				textures.aoSmooth->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				{
					aoBlurAndUpsampleFinalCS.data.ResIdLoResDB = textures.linearDepthDownsample2->uavHeapIndex();
					aoBlurAndUpsampleFinalCS.data.ResIdHiResDB = textures.linearDepth->uavHeapIndex();
					aoBlurAndUpsampleFinalCS.data.ResIdLoResAO1 = textures.aoSmooth2->uavHeapIndex();
					aoBlurAndUpsampleFinalCS.data.ResIdAoResult = textures.aoSmooth->uavHeapIndex();

					aoBlurAndUpsampleFinalCS.dispatch(textures.aoSmooth->width, textures.aoSmooth->height, textures.aoSmooth->width, textures.aoSmooth2->height, textures.aoSmooth2->width,
						commands.commandList, provider.renderSystem->frameIndex);
				}
				textures.linearDepthDownsample8->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				textures.linearDepthDownsample4->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				textures.linearDepthDownsample2->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				textures.linearDepth->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				textures.occlusionInterleaved8->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				textures.occlusion8->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				textures.occlusionInterleaved4->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				textures.occlusion4->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				textures.occlusionInterleaved2->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				textures.aoSmooth4->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				textures.aoSmooth2->TransitionTarget(commands.commandList, provider.renderSystem->frameIndex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

				SetEvent(eventFinish);
			}
		});

	return { { eventFinish, commands } };
}

void SsaoComputeTask::run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass)
{
	prepareDepthBuffersCS.data.ZMagic = ctx.camera->getCameraZ();
	TanHalfFovH = ctx.camera->getTanHalfFovH();

	SetEvent(eventBegin);
}

void SsaoComputeTask::resize(CompositorPass& pass)
{
	initializeResources(pass);
}

void SsaoComputeTask::initializeResources(CompositorPass& pass)
{
	loadTextures(pass);

	//downsample
	DescriptorManager::get().createDepthView(*textures.sceneDepth);
	prepareDepthBuffersCS.data.ResIdLinearZ = DescriptorManager::get().createUAVView(*textures.linearDepth);
	prepareDepthBuffersCS.data.ResIdDS2x = DescriptorManager::get().createUAVView(*textures.linearDepthDownsample2);
	prepareDepthBuffersCS.data.ResIdDS2xAtlas = DescriptorManager::get().createUAVView(*textures.linearDepthDownsampleAtlas2);
	prepareDepthBuffersCS.data.ResIdDS4x = DescriptorManager::get().createUAVView(*textures.linearDepthDownsample4);
	prepareDepthBuffersCS.data.ResIdDS4xAtlas = DescriptorManager::get().createUAVView(*textures.linearDepthDownsampleAtlas4);

	prepareDepthBuffers2CS.data.ResIdDS8x = DescriptorManager::get().createUAVView(*textures.linearDepthDownsample8);
	prepareDepthBuffers2CS.data.ResIdDS8xAtlas = DescriptorManager::get().createUAVView(*textures.linearDepthDownsampleAtlas8);
	prepareDepthBuffers2CS.data.ResIdDS16x = DescriptorManager::get().createUAVView(*textures.linearDepthDownsample16);
	prepareDepthBuffers2CS.data.ResIdDS16xAtlas = DescriptorManager::get().createUAVView(*textures.linearDepthDownsampleAtlas16);

	//occlusion
	DescriptorManager::get().createUAVView(*textures.occlusionInterleaved8);
	DescriptorManager::get().createUAVView(*textures.occlusion8);
	DescriptorManager::get().createUAVView(*textures.occlusionInterleaved4);
	DescriptorManager::get().createUAVView(*textures.occlusion4);
	DescriptorManager::get().createUAVView(*textures.occlusionInterleaved2);

	DescriptorManager::get().createUAVView(*textures.aoSmooth4);
	DescriptorManager::get().createUAVView(*textures.aoSmooth2);
	DescriptorManager::get().createUAVView(*textures.aoSmooth);
}

void SsaoComputeTask::loadTextures(CompositorPass& pass)
{
	auto inputPtr = &pass.inputs[0];
	textures.sceneDepth = inputPtr++->texture;
	textures.linearDepth = inputPtr++->texture;
	textures.linearDepthDownsample2 = inputPtr++->texture;
	textures.linearDepthDownsampleAtlas2 = inputPtr++->texture;
	textures.linearDepthDownsample4 = inputPtr++->texture;
	textures.linearDepthDownsampleAtlas4 = inputPtr++->texture;
	textures.linearDepthDownsample8 = inputPtr++->texture;
	textures.linearDepthDownsampleAtlas8 = inputPtr++->texture;
	textures.linearDepthDownsample16 = inputPtr++->texture;
	textures.linearDepthDownsampleAtlas16 = inputPtr++->texture;

	textures.occlusionInterleaved8 = inputPtr++->texture;
	textures.occlusion8 = inputPtr++->texture;
	textures.occlusionInterleaved4 = inputPtr++->texture;
	textures.occlusion4 = inputPtr++->texture;
	textures.occlusionInterleaved2 = inputPtr++->texture;

	textures.aoSmooth4 = inputPtr++->texture;
	textures.aoSmooth2 = inputPtr++->texture;
	textures.aoSmooth = inputPtr++->texture;
}
