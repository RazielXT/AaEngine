#include "RenderCore/VCT/anisoSeparate/AnisoSeparateVoxelization.h"
#include "Scene/RenderWorld.h"
#include "Scene/RenderQueue.h"
#include "Resources/Material/MaterialResources.h"
#include "Resources/Textures/TextureResources.h"
#include "FrameCompositor/RenderContext.h"
#include "RenderCore/RenderSystem.h"
#include "Resources/Shader/ShaderConstantsProvider.h"

static constexpr float VoxelSize = 128.f;
static constexpr DXGI_FORMAT VoxelColorFormat = DXGI_FORMAT_R11G11B10_FLOAT;
static constexpr DXGI_FORMAT VoxelOccupancyFormat = DXGI_FORMAT_R8_UNORM;

void AnisoSeparateVoxelization::initialize(RenderSystem& renderSystem, const FrameParameters& params, GraphicsResources& resources, ShadowMaps& shadows, RenderWorld& mgr, DXGI_FORMAT outputFormat)
{
	shadowMaps = &shadows;
	renderWorld = &mgr;
	frameParams = &params;

	shadows.createShadowMap(shadowMap, 256, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, "AnisoSeparateVoxelShadowMap");
	shadowRenderables = mgr.getRenderables(Order::Normal);
	shadowQueue = mgr.createQueue({}, MaterialTechnique::DepthShadowmap);

	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& voxels = voxelCascades[i];
		voxels.initialize("AnisoSeparateVoxelCascade" + std::to_string(i), renderSystem.core.device, resources);
		voxels.settings.extends = CascadeExtends[i];
		voxels.idx = i;
	}

	clearColorTexture.Init(renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, VoxelColorFormat);
	clearColorTexture.SetName("AnisoSeparateVoxelClearColor");
	clearOccupancyTexture.Init(renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, VoxelOccupancyFormat);
	clearOccupancyTexture.SetName("AnisoSeparateVoxelClearOccupancy");

	cbuffer = resources.shaderBuffers.CreateCbufferResource(sizeof(cbufferData), "SceneVoxelInfo");

	computeMips.init(*renderSystem.core.device, "generate3DMipmaps3x", resources.shaders);

	auto bouncesShader = resources.shaders.getShader("anisoSeparateVoxelBouncesCS", ShaderType::Compute, ShaderRef{"vct/anisoSeparate/anisoSeparateVoxelBouncesCS.hlsl", "main", "cs_6_6"});
	bouncesCS.init(*renderSystem.core.device, *bouncesShader);

	auto clearBufferShader = resources.shaders.getShader("clearBufferCS", ShaderType::Compute, ShaderRef{ "utils/clearBufferCS.hlsl", "main", "cs_6_6" });
	clearBufferCS.init(*renderSystem.core.device, *clearBufferShader);

	auto clearTextureShader = resources.shaders.getShader("clearTextureCS", ShaderType::Compute, ShaderRef{ "utils/clearTextureCS.hlsl", "main", "cs_6_6" });
	clearTextureCS.init(*renderSystem.core.device, *clearTextureShader);

	auto bitmaskShader = resources.shaders.getShader("opacityBitmaskCS", ShaderType::Compute, ShaderRef{ "vct/opacityBitmaskCS.hlsl", "CSMain", "cs_6_6" });
	occupancyBitmaskCS.init(*renderSystem.core.device, *bitmaskShader);

	sceneQueue = mgr.createQueue({ outputFormat }, MaterialTechnique::Voxelize);

	revoxelize();
}

void AnisoSeparateVoxelization::shutdown()
{
	shadowMaps->removeShadowMap(shadowMap);
}

void AnisoSeparateVoxelization::clear(ID3D12GraphicsCommandList* c)
{
	revoxelize();

	for (auto& cascade : voxelCascades)
	{
		cascade.clearAll(c, clearTextureCS);
	}

	reset = false;
}

void AnisoSeparateVoxelization::revoxelize()
{
	for (auto& b : buildCounter)
	{
		b = 0;
	}
}

void AnisoSeparateVoxelization::transitionForCompute(ID3D12GraphicsCommandList* commandList)
{
	for (auto& voxel : voxelCascades)
	{
		for (UINT f = 0; f < AnisoSeparateVoxelCascade::FaceCount; f++)
		{
			voxel.voxelFaceTextures[f].Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			voxel.voxelPreviousFaceTextures[f].Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}

		voxel.voxelOccupancyTexture.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		voxel.voxelPreviousOccupancyTexture.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
}

void AnisoSeparateVoxelization::updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 moveOffset, AnisoSeparateVoxelCascade& cascade)
{
	info.voxelSceneSize = cascade.settings.extends * 2;

	float sceneToVoxel = VoxelSize / info.voxelSceneSize;
	info.voxelDensity = sceneToVoxel;

	info.voxelOffset = cascade.settings.center - Vector3(cascade.settings.extends);
	info.moveOffset = moveOffset;

	for (UINT f = 0; f < AnisoSeparateVoxelCascade::FaceCount; f++)
	{
		info.texIdFaces[f] = cascade.voxelFaceTextures[f].view.srvHeapIndex;
		info.texIdPrevFaces[f] = cascade.voxelPreviousFaceTextures[f].view.srvHeapIndex;
	}

	info.texIdOccupancy = cascade.voxelOccupancyTexture.view.srvHeapIndex;
	info.texIdPrevOccupancy = cascade.voxelPreviousOccupancyTexture.view.srvHeapIndex;
	info.resIdDataBuffer = cascade.voxelInfoBuffer.view.heapIndex;
	info.texIdOccupancyBitmask = cascade.voxelOccupancyBitmaskTexture.view.srvHeapIndex;
}

void AnisoSeparateVoxelization::updateCBuffer(const Vector3& cameraPosition, const AnisoSeparateVoxelTracingParams& params)
{
	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& voxel = voxelCascades[i];
		Vector3 movement = voxel.update(cameraPosition);
		if (movement != Vector3::Zero)
			buildCounter[i] = 0;

		updateCBufferCascade(cbufferData.cascadeInfo[i], movement, voxel);
	}

	cbufferData.middleConeRatioDistance = params.middleConeRatioDistance;
	cbufferData.sideConeRatioDistance = params.sideConeRatioDistance;

	auto& cbufferResource = *cbuffer.data[frameParams->frameIndex];
	memcpy(cbufferResource.Memory(), &cbufferData, sizeof(cbufferData));
}

void AnisoSeparateVoxelization::voxelizeCascades(CommandsData& commands, GpuTextureStates& viewportOutput, const RenderContext& ctx)
{
	if (reset)
		clear(commands.commandList);

	TextureTransitions<(2 * AnisoSeparateVoxelCascade::FaceCount + 2) * CascadesCount> tr;

	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& cascade = voxelCascades[i];
		TextureStatePair faceStates[AnisoSeparateVoxelCascade::FaceCount];
		TextureStatePair prevFaceStates[AnisoSeparateVoxelCascade::FaceCount];
		TextureStatePair occupancyState = { &cascade.voxelOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		TextureStatePair prevOccupancyState = { &cascade.voxelPreviousOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		for (UINT f = 0; f < AnisoSeparateVoxelCascade::FaceCount; f++)
		{
			faceStates[f] = { &cascade.voxelFaceTextures[f], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
			prevFaceStates[f] = { &cascade.voxelPreviousFaceTextures[f], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		}

		if (!buildCounter[i])
		{
			buildCounter[i]++;
			voxelizeCascade(commands.commandList, faceStates, prevFaceStates, occupancyState, prevOccupancyState, viewportOutput, voxelCascades[i], ctx);
		}

		for (UINT f = 0; f < AnisoSeparateVoxelCascade::FaceCount; f++)
		{
			tr.add(faceStates[f], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			tr.add(prevFaceStates[f], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		tr.add(occupancyState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(prevOccupancyState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	tr.push(commands.commandList);
}

void AnisoSeparateVoxelization::voxelizeCascade(ID3D12GraphicsCommandList* commandList, TextureStatePair faceStates[AnisoSeparateVoxelCascade::FaceCount], TextureStatePair prevFaceStates[AnisoSeparateVoxelCascade::FaceCount], TextureStatePair& occupancyState, TextureStatePair& prevOccupancyState, GpuTextureStates& viewportOutput, AnisoSeparateVoxelCascade& cascade, const RenderContext& ctx)
{
	renderShadowMap(commandList, cascade.settings.extends, ctx);

	viewportOutput.texture->PrepareAsRenderTarget(commandList, viewportOutput.previousState, true);

	cascade.prepareForVoxelization(commandList, faceStates, prevFaceStates, occupancyState, prevOccupancyState, clearTextureCS);
	auto b = CD3DX12_RESOURCE_BARRIER::Transition(cascade.voxelInfoBuffer.data.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &b);

	auto center = cascade.settings.center;
	auto orthoHalfSize = cascade.settings.extends;

	const float NearClipDistance = 1;

	Camera camera;
	camera.setOrthographicCamera(orthoHalfSize * 2, orthoHalfSize * 2, NearClipDistance, NearClipDistance + orthoHalfSize * 2);

	static RenderObjectsVisibilityData sceneInfo;
	auto& renderables = *renderWorld->getRenderables(Order::Normal);

	ShaderConstantsProvider constants(*frameParams, sceneInfo, camera, *viewportOutput.texture);
	constants.viewId = RenderViewId(RenderViewId_Voxelize0 + cascade.idx);
	constants.uavBarrier = cascade.voxelInfoBuffer.data.Get();

	auto shadowMatrix = XMMatrixTranspose(shadowMap.camera.getViewProjectionMatrix());

	sceneQueue->iterateMaterials([&](AssignedMaterial* material)
		{
			//material->SetUAV(cascade.voxelInfoBuffer.data.Get(), 0);
			material->SetParameter("VoxelIdx", &cascade.idx);
			material->SetParameter("ShadowMatrix", &shadowMatrix);
			material->SetParameter("ShadowMapIdx", &shadowMap.texture.view.srvHeapIndex);
		});

	camera.setPosition({ center.x, center.y, center.z - orthoHalfSize - NearClipDistance });
	camera.setDirection({ 0, 0, 1 });
	camera.updateMatrix();
	renderables.updateVisibility(camera, sceneInfo);
	sceneQueue->renderObjects(constants, commandList);

	for (UINT f = 0; f < AnisoSeparateVoxelCascade::FaceCount; f++)
	{
		faceStates[f].Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		computeMips.dispatch(commandList, cascade.voxelFaceTextures[f]);
	}

	occupancyState.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	computeMips.dispatch(commandList, cascade.voxelOccupancyTexture);

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(cascade.voxelInfoBuffer.data.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(cascade.voxelInfoBuffer.data.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &b2);
}

void AnisoSeparateVoxelization::renderShadowMap(ID3D12GraphicsCommandList* commandList, float extends, const RenderContext& ctx)
{
	shadowMaps->updateShadowMap(shadowMap, ctx.camera->getPosition(), extends * 1.5f);

	CommandsMarker marker(commandList, "AnisoSeparateVoxelShadowMap", PixColor::DarkTurquoise);

	static std::vector<UINT> filtered;
	shadowRenderables->createFilteredIds(RenderObjectFlag::NoVoxelization, filtered);
	shadowRenderables->updateVisibility(shadowMap.camera, filtered, shadowRenderablesData);

	shadowMap.texture.PrepareAsDepthTarget(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	ShaderConstantsProvider constants(*frameParams, shadowRenderablesData, shadowMap.camera, *ctx.camera, shadowMap.texture);
	constants.viewId = RenderViewId_VoxelizeShadowMap;
	shadowQueue->renderObjects(constants, commandList);

	shadowMap.texture.PrepareAsView(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void AnisoSeparateVoxelization::bounceCascade(CommandsData& commands, AnisoSeparateVoxelCascade& cascade, const RenderContext& ctx)
{
	CommandsMarker marker(commands.commandList, "AnisoSeparateVoxelBounces", PixColor::LightBlue);

	for (UINT f = 0; f < AnisoSeparateVoxelCascade::FaceCount; f++)
	{
		cascade.voxelFaceTextures[f].Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
		cascade.voxelPreviousFaceTextures[f].Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

		commands.commandList->CopyResource(cascade.voxelPreviousFaceTextures[f].texture.Get(), cascade.voxelFaceTextures[f].texture.Get());

		cascade.voxelFaceTextures[f].Transition(commands.commandList, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cascade.voxelPreviousFaceTextures[f].Transition(commands.commandList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	cascade.voxelOccupancyTexture.Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	cascade.voxelPreviousOccupancyTexture.Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	commands.commandList->CopyResource(cascade.voxelPreviousOccupancyTexture.texture.Get(), cascade.voxelOccupancyTexture.texture.Get());
	cascade.voxelOccupancyTexture.Transition(commands.commandList, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cascade.voxelPreviousOccupancyTexture.Transition(commands.commandList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	struct
	{
		Vector3 SunColor;
	}
	data = {
		.SunColor = frameParams->sky.SunColor
	};

	ShaderTextureView faceViews[AnisoSeparateVoxelCascade::FaceCount];
	ShaderTextureView prevFaceViews[AnisoSeparateVoxelCascade::FaceCount];
	for (UINT f = 0; f < AnisoSeparateVoxelCascade::FaceCount; f++)
	{
		faceViews[f] = cascade.voxelFaceTextures[f].view;
		prevFaceViews[f] = cascade.voxelPreviousFaceTextures[f].view;
	}

	bouncesCS.dispatch(commands.commandList, std::span{ (float*)&data, sizeof(data) / sizeof(float) }, faceViews, cascade.voxelOccupancyTexture.view, prevFaceViews, cascade.voxelPreviousOccupancyTexture.view, cascade.voxelInfoBuffer.data->GetGPUVirtualAddress());

	for (UINT f = 0; f < AnisoSeparateVoxelCascade::FaceCount; f++)
	{
		cascade.voxelFaceTextures[f].Transition(commands.commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		computeMips.dispatch(commands.commandList, cascade.voxelFaceTextures[f]);
	}

	cascade.voxelOccupancyTexture.Transition(commands.commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	computeMips.dispatch(commands.commandList, cascade.voxelOccupancyTexture);

	cascade.voxelOccupancyBitmaskTexture.Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	occupancyBitmaskCS.dispatch(commands.commandList, cascade.voxelOccupancyTexture.view, cascade.voxelOccupancyBitmaskTexture.view);
	cascade.voxelOccupancyBitmaskTexture.Transition(commands.commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void AnisoSeparateVoxelization::runBounces(CommandsData& computeCommands, const RenderContext& ctx, const AnisoSeparateVoxelTracingParams& params)
{
	updateCBuffer(ctx.camera->getPosition(), params);

// 	static UINT idx = 0;
// 
// 	for (UINT i = 0; i < CascadesCount; i++)
// 	{
// 		if (buildCounter[i] == 1 && i == (idx % 4))
// 		{
// 			bounceCascade(computeCommands, voxelCascades[i], ctx);
// 			buildCounter[i]++;
// 		}
// 	}
// 
// 	idx++;
}

void AnisoSeparateBounceVoxelsCS::dispatch(ID3D12GraphicsCommandList* commandList, const std::span<float>& data, const ShaderTextureView faceViews[AnisoSeparateVoxelCascade::FaceCount], const ShaderTextureView& occupancyView, const ShaderTextureView prevFaceViews[AnisoSeparateVoxelCascade::FaceCount], const ShaderTextureView& prevOccupancyView, D3D12_GPU_VIRTUAL_ADDRESS dataAddr)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, data.size(), data.data(), 0);
	commandList->SetComputeRootShaderResourceView(1, dataAddr);

	UINT rootIndex = 2;

// 	for (UINT f = 0; f < AnisoSeparateVoxelCascade::FaceCount; f++)
// 	{
// 		commandList->SetComputeRootDescriptorTable(rootIndex++, prevFaceViews[f].srvHandle);
// 	}
//
//	commandList->SetComputeRootDescriptorTable(rootIndex++, prevOccupancyView.srvHandle);

	for (UINT f = 0; f < AnisoSeparateVoxelCascade::FaceCount; f++)
	{
		commandList->SetComputeRootDescriptorTable(rootIndex++, faceViews[f].uavHandle);
	}

	commandList->SetComputeRootDescriptorTable(rootIndex++, occupancyView.uavHandle);

	const UINT threadSize = 4;
	const auto voxelSize = UINT(VoxelSize);
	const uint32_t groupSize = (voxelSize + threadSize - 1) / threadSize;

	commandList->Dispatch(groupSize, groupSize, groupSize);
}

void AnisoSeparateOccupancyBitmaskCS::dispatch(ID3D12GraphicsCommandList* commandList, const ShaderTextureView& sourceOccupancy, const ShaderTextureView& target)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRootDescriptorTable(0, sourceOccupancy.srvHandle);
	commandList->SetComputeRootDescriptorTable(1, target.uavHandle);

	const UINT threadSize = 8;
	const auto voxelSize = UINT(VoxelSize);
	const UINT groupSize = (voxelSize / 2 + threadSize - 1) / threadSize;
	commandList->Dispatch(groupSize, groupSize, groupSize);
}
