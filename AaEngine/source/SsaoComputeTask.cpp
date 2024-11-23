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

				RenderTargetTransitions<11> tr;

				auto textures = this->textures;

				//downsample depth
				{
					auto& input = textures.sceneDepth;
					tr.addDepthAndPush(input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, commands.commandList);

					prepareDepthBuffersCS.dispatch(input.texture->width, input.texture->height, input.texture->depthView, commands.commandList);

					tr.addDepthAndPush(input, D3D12_RESOURCE_STATE_DEPTH_WRITE, commands.commandList);
				}
				{
					auto& input = textures.linearDepthDownsample4;
					tr.addAndPush(input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, commands.commandList);

					prepareDepthBuffers2CS.dispatch(input.texture->width, input.texture->height, input.texture->textureView(), commands.commandList);

					tr.addAndPush(input, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, commands.commandList);	
				}

				//occlusion
				auto renderAo = [&](RenderTargetTextureState& input, RenderTargetTextureState& output, AoRenderCS& aoRenderCS)
					{
						tr.addAndPush(input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, commands.commandList);

						aoRenderCS.dispatch(input.texture->width, input.texture->height, input.texture->arraySize, TanHalfFovH, input.texture->textureView(), output.texture->textureView(), commands.commandList);

						tr.addAndPush(input, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, commands.commandList);
					};
				renderAo(textures.linearDepthDownsampleAtlas8, textures.occlusionInterleaved8, aoRenderInterleaveCS);
				renderAo(textures.linearDepthDownsample8, textures.occlusion8, aoRenderCS);
				renderAo(textures.linearDepthDownsampleAtlas4, textures.occlusionInterleaved4, aoRenderInterleaveCS);
				renderAo(textures.linearDepthDownsample4, textures.occlusion4, aoRenderCS);
				renderAo(textures.linearDepthDownsampleAtlas2, textures.occlusionInterleaved2, aoRenderInterleaveCS);

				//upscale
				tr.add(textures.linearDepthDownsample8, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				tr.add(textures.linearDepthDownsample4, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				tr.add(textures.linearDepthDownsample2, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				tr.add(textures.linearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				tr.add(textures.occlusionInterleaved8, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				tr.add(textures.occlusion8, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				tr.add(textures.occlusionInterleaved4, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				tr.add(textures.occlusion4, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				tr.addAndPush(textures.occlusionInterleaved2, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, commands.commandList);
				{
					aoBlurAndUpsampleCS.data.ResIdLoResDB = textures.linearDepthDownsample8.texture->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdHiResDB = textures.linearDepthDownsample4.texture->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdLoResAO1 = textures.occlusionInterleaved8.texture->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdLoResAO2 = textures.occlusion8.texture->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdHiResAO = textures.occlusionInterleaved4.texture->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdAoResult = textures.aoSmooth4.texture->uavHeapIndex();

					aoBlurAndUpsampleCS.dispatch(textures.aoSmooth.texture->width, textures.aoSmooth4.texture->height, textures.aoSmooth4.texture->width, textures.occlusion8.texture->height, textures.occlusion8.texture->width,
						commands.commandList);
				}
				tr.addAndPush(textures.aoSmooth4, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, commands.commandList);
				{
					aoBlurAndUpsampleCS.data.ResIdLoResDB = textures.linearDepthDownsample4.texture->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdHiResDB = textures.linearDepthDownsample2.texture->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdLoResAO1 = textures.aoSmooth4.texture->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdLoResAO2 = textures.occlusion4.texture->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdHiResAO = textures.occlusionInterleaved2.texture->uavHeapIndex();
					aoBlurAndUpsampleCS.data.ResIdAoResult = textures.aoSmooth2.texture->uavHeapIndex();

					aoBlurAndUpsampleCS.dispatch(textures.aoSmooth.texture->width, textures.aoSmooth2.texture->height, textures.aoSmooth2.texture->width, textures.occlusion4.texture->height, textures.occlusion4.texture->width,
						commands.commandList);
				}
				tr.add(textures.aoSmooth2, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				tr.addAndPush(textures.aoSmooth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, commands.commandList);
				{
					aoBlurAndUpsampleFinalCS.data.ResIdLoResDB = textures.linearDepthDownsample2.texture->uavHeapIndex();
					aoBlurAndUpsampleFinalCS.data.ResIdHiResDB = textures.linearDepth.texture->uavHeapIndex();
					aoBlurAndUpsampleFinalCS.data.ResIdLoResAO1 = textures.aoSmooth2.texture->uavHeapIndex();
					aoBlurAndUpsampleFinalCS.data.ResIdAoResult = textures.aoSmooth.texture->uavHeapIndex();

					aoBlurAndUpsampleFinalCS.dispatch(textures.aoSmooth.texture->width, textures.aoSmooth.texture->height, textures.aoSmooth.texture->width, textures.aoSmooth2.texture->height, textures.aoSmooth2.texture->width,
						commands.commandList);
				}
				tr.add(textures.linearDepthDownsample8, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				tr.add(textures.linearDepthDownsample4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				tr.add(textures.linearDepthDownsample2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				tr.add(textures.linearDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				tr.add(textures.occlusionInterleaved8, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				tr.add(textures.occlusion8, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				tr.add(textures.occlusionInterleaved4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				tr.add(textures.occlusion4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				tr.add(textures.occlusionInterleaved2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				tr.add(textures.aoSmooth4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				tr.addAndPush(textures.aoSmooth2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, commands.commandList);

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
	DescriptorManager::get().createDepthView(*textures.sceneDepth.texture);
	prepareDepthBuffersCS.data.ResIdLinearZ = DescriptorManager::get().createUAVView(*textures.linearDepth.texture);
	prepareDepthBuffersCS.data.ResIdDS2x = DescriptorManager::get().createUAVView(*textures.linearDepthDownsample2.texture);
	prepareDepthBuffersCS.data.ResIdDS2xAtlas = DescriptorManager::get().createUAVView(*textures.linearDepthDownsampleAtlas2.texture);
	prepareDepthBuffersCS.data.ResIdDS4x = DescriptorManager::get().createUAVView(*textures.linearDepthDownsample4.texture);
	prepareDepthBuffersCS.data.ResIdDS4xAtlas = DescriptorManager::get().createUAVView(*textures.linearDepthDownsampleAtlas4.texture);

	prepareDepthBuffers2CS.data.ResIdDS8x = DescriptorManager::get().createUAVView(*textures.linearDepthDownsample8.texture);
	prepareDepthBuffers2CS.data.ResIdDS8xAtlas = DescriptorManager::get().createUAVView(*textures.linearDepthDownsampleAtlas8.texture);
	prepareDepthBuffers2CS.data.ResIdDS16x = DescriptorManager::get().createUAVView(*textures.linearDepthDownsample16.texture);
	prepareDepthBuffers2CS.data.ResIdDS16xAtlas = DescriptorManager::get().createUAVView(*textures.linearDepthDownsampleAtlas16.texture);

	//occlusion
	DescriptorManager::get().createUAVView(*textures.occlusionInterleaved8.texture);
	DescriptorManager::get().createUAVView(*textures.occlusion8.texture);
	DescriptorManager::get().createUAVView(*textures.occlusionInterleaved4.texture);
	DescriptorManager::get().createUAVView(*textures.occlusion4.texture);
	DescriptorManager::get().createUAVView(*textures.occlusionInterleaved2.texture);

	DescriptorManager::get().createUAVView(*textures.aoSmooth4.texture);
	DescriptorManager::get().createUAVView(*textures.aoSmooth2.texture);
	DescriptorManager::get().createUAVView(*textures.aoSmooth.texture);
}

void SsaoComputeTask::loadTextures(CompositorPass& pass)
{
	auto inputPtr = &pass.inputs[0];
	textures.sceneDepth = *inputPtr++;
	textures.linearDepth = *inputPtr++;
	textures.linearDepthDownsample2 = *inputPtr++;
	textures.linearDepthDownsampleAtlas2 = *inputPtr++;
	textures.linearDepthDownsample4 = *inputPtr++;
	textures.linearDepthDownsampleAtlas4 = *inputPtr++;
	textures.linearDepthDownsample8 = *inputPtr++;
	textures.linearDepthDownsampleAtlas8 = *inputPtr++;
	textures.linearDepthDownsample16 = *inputPtr++;
	textures.linearDepthDownsampleAtlas16 = *inputPtr++;

	textures.occlusionInterleaved8 = *inputPtr++;
	textures.occlusion8 = *inputPtr++;
	textures.occlusionInterleaved4 = *inputPtr++;
	textures.occlusion4 = *inputPtr++;
	textures.occlusionInterleaved2 = *inputPtr++;

	textures.aoSmooth4 = *inputPtr++;
	textures.aoSmooth2 = *inputPtr++;
	textures.aoSmooth = *inputPtr++;
}
