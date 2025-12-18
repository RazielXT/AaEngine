#include "SsaoComputeTask.h"
#include "DescriptorManager.h"
#include "Camera.h"

SsaoComputeTask::SsaoComputeTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s)
{
}

SsaoComputeTask::~SsaoComputeTask()
{
}

AsyncTasksInfo SsaoComputeTask::initialize(CompositorPass& pass)
{
	auto& device = *provider.renderSystem.core.device;
	prepareDepthBuffersCS.init(device, "AoPrepareDepthBuffers", provider.resources.shaders);
	prepareDepthBuffers2CS.init(device, "AoPrepareDepthBuffers2", provider.resources.shaders);
	aoRenderCS.init(device, "AoRenderCS", provider.resources.shaders);
	aoRenderInterleaveCS.init(device, "AoRenderInterleaveCS", provider.resources.shaders);
	aoBlurAndUpsampleCS.init(device, "AoBlurAndUpsampleCS", provider.resources.shaders);
	aoBlurAndUpsampleFinalCS.init(device, "AoBlurAndUpsampleFinalCS", provider.resources.shaders);

	initializeResources(pass);

	return {};
}

void SsaoComputeTask::runCompute(RenderContext& ctx, CommandsData& commands, CompositorPass& pass)
{
	prepareDepthBuffersCS.data.ZMagic = ctx.camera->getCameraZ();
	TanHalfFovH = ctx.camera->getTanHalfFovH();

	{
		auto marker = CommandsMarker(commands.commandList, "SSAO", PixColor::SSAO);

		TextureTransitions<17> tr;

		auto textures = this->textures;
		tr.add(textures.linearDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.linearDepthDownsample16, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.linearDepthDownsample8, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.linearDepthDownsample4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.linearDepthDownsample2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.occlusionInterleaved8, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.occlusion8, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.occlusionInterleaved4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.occlusion4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.occlusionInterleaved2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.linearDepthDownsampleAtlas16, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.linearDepthDownsampleAtlas8, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.linearDepthDownsampleAtlas4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.linearDepthDownsampleAtlas2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.aoSmooth4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.aoSmooth2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.add(textures.aoSmooth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.push(commands.commandList);

		//downsample depth
		{
			auto& input = textures.sceneDepth;

			prepareDepthBuffersCS.dispatch(input.texture->width, input.texture->height, input.texture->view, commands.commandList);
		}
		{
			auto& input = textures.linearDepthDownsample4;
			tr.addAndPush(input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, commands.commandList);

			prepareDepthBuffers2CS.dispatch(input.texture->width, input.texture->height, input.texture->view, commands.commandList);
		}

		//occlusion
		auto renderAo = [&](TextureStatePair& input, TextureStatePair& output, AoRenderCS& aoRenderCS)
			{
				tr.addAndPush(input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, commands.commandList);

				aoRenderCS.dispatch(input.texture->width, input.texture->height, input.texture->depthOrArraySize, TanHalfFovH, input.texture->view, output.texture->view, commands.commandList);
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
			aoBlurAndUpsampleCS.data.ResIdLoResDB = textures.linearDepthDownsample8.texture->view.uavHeapIndex;
			aoBlurAndUpsampleCS.data.ResIdHiResDB = textures.linearDepthDownsample4.texture->view.uavHeapIndex;
			aoBlurAndUpsampleCS.data.ResIdLoResAO1 = textures.occlusionInterleaved8.texture->view.uavHeapIndex;
			aoBlurAndUpsampleCS.data.ResIdLoResAO2 = textures.occlusion8.texture->view.uavHeapIndex;
			aoBlurAndUpsampleCS.data.ResIdHiResAO = textures.occlusionInterleaved4.texture->view.uavHeapIndex;
			aoBlurAndUpsampleCS.data.ResIdAoResult = textures.aoSmooth4.texture->view.uavHeapIndex;

			aoBlurAndUpsampleCS.dispatch(textures.aoSmooth.texture->width, textures.aoSmooth4.texture->height, textures.aoSmooth4.texture->width, textures.occlusion8.texture->height, textures.occlusion8.texture->width,
				commands.commandList);
		}

		tr.addAndPush(textures.aoSmooth4, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, commands.commandList);
		{
			aoBlurAndUpsampleCS.data.ResIdLoResDB = textures.linearDepthDownsample4.texture->view.uavHeapIndex;
			aoBlurAndUpsampleCS.data.ResIdHiResDB = textures.linearDepthDownsample2.texture->view.uavHeapIndex;
			aoBlurAndUpsampleCS.data.ResIdLoResAO1 = textures.aoSmooth4.texture->view.uavHeapIndex;
			aoBlurAndUpsampleCS.data.ResIdLoResAO2 = textures.occlusion4.texture->view.uavHeapIndex;
			aoBlurAndUpsampleCS.data.ResIdHiResAO = textures.occlusionInterleaved2.texture->view.uavHeapIndex;
			aoBlurAndUpsampleCS.data.ResIdAoResult = textures.aoSmooth2.texture->view.uavHeapIndex;

			aoBlurAndUpsampleCS.dispatch(textures.aoSmooth.texture->width, textures.aoSmooth2.texture->height, textures.aoSmooth2.texture->width, textures.occlusion4.texture->height, textures.occlusion4.texture->width,
				commands.commandList);
		}

		tr.addAndPush(textures.aoSmooth2, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, commands.commandList);
		{
			aoBlurAndUpsampleFinalCS.data.ResIdLoResDB = textures.linearDepthDownsample2.texture->view.uavHeapIndex;
			aoBlurAndUpsampleFinalCS.data.ResIdHiResDB = textures.linearDepth.texture->view.uavHeapIndex;
			aoBlurAndUpsampleFinalCS.data.ResIdLoResAO1 = textures.aoSmooth2.texture->view.uavHeapIndex;
			aoBlurAndUpsampleFinalCS.data.ResIdAoResult = textures.aoSmooth.texture->view.uavHeapIndex;

			aoBlurAndUpsampleFinalCS.dispatch(textures.aoSmooth.texture->width, textures.aoSmooth.texture->height, textures.aoSmooth.texture->width, textures.aoSmooth2.texture->height, textures.aoSmooth2.texture->width,
				commands.commandList);
		}

		marker.close();
	}
}

void SsaoComputeTask::resize(CompositorPass& pass)
{
	initializeResources(pass);
}

void SsaoComputeTask::initializeResources(CompositorPass& pass)
{
	loadTextures(pass);

	auto& descriptors = provider.resources.descriptors;

	//downsample
	descriptors.createTextureView(*textures.sceneDepth.texture);
	prepareDepthBuffersCS.data.ResIdLinearZ = descriptors.createUAVView(*textures.linearDepth.texture);
	prepareDepthBuffersCS.data.ResIdDS2x = descriptors.createUAVView(*textures.linearDepthDownsample2.texture);
	prepareDepthBuffersCS.data.ResIdDS2xAtlas = descriptors.createUAVView(*textures.linearDepthDownsampleAtlas2.texture);
	prepareDepthBuffersCS.data.ResIdDS4x = descriptors.createUAVView(*textures.linearDepthDownsample4.texture);
	prepareDepthBuffersCS.data.ResIdDS4xAtlas = descriptors.createUAVView(*textures.linearDepthDownsampleAtlas4.texture);

	prepareDepthBuffers2CS.data.ResIdDS8x = descriptors.createUAVView(*textures.linearDepthDownsample8.texture);
	prepareDepthBuffers2CS.data.ResIdDS8xAtlas = descriptors.createUAVView(*textures.linearDepthDownsampleAtlas8.texture);
	prepareDepthBuffers2CS.data.ResIdDS16x = descriptors.createUAVView(*textures.linearDepthDownsample16.texture);
	prepareDepthBuffers2CS.data.ResIdDS16xAtlas = descriptors.createUAVView(*textures.linearDepthDownsampleAtlas16.texture);

	//occlusion
	descriptors.createUAVView(*textures.occlusionInterleaved8.texture);
	descriptors.createUAVView(*textures.occlusion8.texture);
	descriptors.createUAVView(*textures.occlusionInterleaved4.texture);
	descriptors.createUAVView(*textures.occlusion4.texture);
	descriptors.createUAVView(*textures.occlusionInterleaved2.texture);

	descriptors.createUAVView(*textures.aoSmooth4.texture);
	descriptors.createUAVView(*textures.aoSmooth2.texture);
	descriptors.createUAVView(*textures.aoSmooth.texture);
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
	textures.aoSmooth = pass.targets.front();
}
