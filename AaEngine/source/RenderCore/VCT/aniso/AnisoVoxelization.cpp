#include "RenderCore/VCT/aniso/AnisoVoxelization.h"
#include "Scene/RenderWorld.h"
#include "Scene/RenderQueue.h"
#include "Resources/Material/MaterialResources.h"
#include "Resources/Textures/TextureResources.h"
#include "FrameCompositor/RenderContext.h"
#include "RenderCore/RenderSystem.h"
#include "Resources/Shader/ShaderConstantsProvider.h"

static constexpr float VoxelSize = 128.f;
static constexpr DXGI_FORMAT VoxelFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

void AnisoVoxelization::initialize(RenderSystem& renderSystem, const FrameParameters& params, GraphicsResources& resources, ShadowMaps& shadows, RenderWorld& mgr, DXGI_FORMAT outputFormat)
{
	shadowMaps = &shadows;
	renderWorld = &mgr;
	frameParams = &params;

	shadows.createShadowMap(shadowMap, 256, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, "AnisoVoxelShadowMap");
	shadowRenderables = mgr.getRenderables(Order::Normal);
	shadowQueue = mgr.createQueue({}, MaterialTechnique::DepthShadowmap);

	constexpr float CascadeDistances[CascadesCount] = { 200, 400, 800, 1600 };
	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& voxels = voxelCascades[i];
		voxels.initialize("AnisoVoxelCascade" + std::to_string(i), renderSystem.core.device, resources);
		voxels.settings.extends = CascadeDistances[i];
		voxels.idx = i;
	}

	clearSceneTexture.Init(renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	clearSceneTexture.SetName("AnisoVoxelClear");

	cbuffer = resources.shaderBuffers.CreateCbufferResource(sizeof(cbufferData), "SceneVoxelInfo");

	computeMips.init(*renderSystem.core.device, "generate3DMipmaps3x", resources.shaders);

	auto bouncesShader = resources.shaders.getShader("anisoVoxelBouncesCS", ShaderType::Compute, ShaderRef{"vct/aniso/anisoVoxelBouncesCS.hlsl", "main", "cs_6_6"});
	bouncesCS.init(*renderSystem.core.device, *bouncesShader);

	auto clearBufferShader = resources.shaders.getShader("clearBufferCS", ShaderType::Compute, ShaderRef{ "utils/clearBufferCS.hlsl", "main", "cs_6_6" });
	clearBufferCS.init(*renderSystem.core.device, *clearBufferShader);

	sceneQueue = mgr.createQueue({ outputFormat }, MaterialTechnique::Voxelize);

	revoxelize();
}

void AnisoVoxelization::shutdown()
{
	shadowMaps->removeShadowMap(shadowMap);
}

void AnisoVoxelization::clear(ID3D12GraphicsCommandList* c)
{
	revoxelize();

	for (auto& cascade : voxelCascades)
	{
		cascade.clearAll(c, clearSceneTexture, clearBufferCS);
	}

	reset = false;
}

void AnisoVoxelization::revoxelize()
{
	for (auto& b : buildCounter)
	{
		b = 0;
	}
}

void AnisoVoxelization::transitionForCompute(ID3D12GraphicsCommandList* commandList)
{
	for (auto& voxel : voxelCascades)
	{
		for (UINT f = 0; f < AnisoVoxelCascade::FaceCount; f++)
		{
			voxel.voxelFaceTextures[f].Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			voxel.voxelPreviousFaceTextures[f].Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
	}
}

void AnisoVoxelization::updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 moveOffset, AnisoVoxelCascade& cascade)
{
	info.voxelSceneSize = cascade.settings.extends * 2;

	float sceneToVoxel = VoxelSize / info.voxelSceneSize;
	info.voxelDensity = sceneToVoxel;

	info.voxelOffset = cascade.settings.center - Vector3(cascade.settings.extends);

	info.moveOffset = moveOffset;

	for (UINT f = 0; f < AnisoVoxelCascade::FaceCount; f++)
	{
		info.texIdFaces[f] = cascade.voxelFaceTextures[f].view.srvHeapIndex;
		info.texIdPrevFaces[f] = cascade.voxelPreviousFaceTextures[f].view.srvHeapIndex;
	}

	info.resIdDataBuffer = cascade.voxelInfoBuffer.view.heapIndex;
}

void AnisoVoxelization::updateCBuffer(const Vector3& cameraPosition, const AnisoVoxelTracingParams& params)
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

void AnisoVoxelization::voxelizeCascades(CommandsData& commands, GpuTextureStates& viewportOutput, const RenderContext& ctx)
{
	if (reset)
		clear(commands.commandList);

	TextureTransitions<2 * AnisoVoxelCascade::FaceCount * CascadesCount> tr;

	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& cascade = voxelCascades[i];
		TextureStatePair faceStates[AnisoVoxelCascade::FaceCount];
		TextureStatePair prevFaceStates[AnisoVoxelCascade::FaceCount];
		for (UINT f = 0; f < AnisoVoxelCascade::FaceCount; f++)
		{
			faceStates[f] = { &cascade.voxelFaceTextures[f], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
			prevFaceStates[f] = { &cascade.voxelPreviousFaceTextures[f], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		}

		if (!buildCounter[i])
		{
			buildCounter[i]++;
			voxelizeCascade(commands.commandList, faceStates, prevFaceStates, viewportOutput, voxelCascades[i], ctx);
		}

		for (UINT f = 0; f < AnisoVoxelCascade::FaceCount; f++)
		{
			tr.add(faceStates[f], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			tr.add(prevFaceStates[f], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	tr.push(commands.commandList);
}

void AnisoVoxelization::voxelizeCascade(ID3D12GraphicsCommandList* commandList, TextureStatePair faceStates[AnisoVoxelCascade::FaceCount], TextureStatePair prevFaceStates[AnisoVoxelCascade::FaceCount], GpuTextureStates& viewportOutput, AnisoVoxelCascade& cascade, const RenderContext& ctx)
{
	renderShadowMap(commandList, cascade.settings.extends, ctx);

	viewportOutput.texture->PrepareAsRenderTarget(commandList, viewportOutput.previousState, true);

	cascade.prepareForVoxelization(commandList, faceStates, prevFaceStates, clearSceneTexture, clearBufferCS);
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
	constants.uavBarrier = cascade.voxelInfoBuffer.data.Get();

	auto shadowMatrix = XMMatrixTranspose(shadowMap.camera.getViewProjectionMatrix());

	sceneQueue->iterateMaterials([&](AssignedMaterial* material)
		{
			material->SetUAV(cascade.voxelInfoBuffer.data.Get(), 0);
			material->SetParameter("VoxelIdx", &cascade.idx);
			material->SetParameter("ShadowMatrix", &shadowMatrix);
			material->SetParameter("ShadowMapIdx", &shadowMap.texture.view.srvHeapIndex);
		});

	// Single render pass - geometry shader selects dominant axis per triangle
	camera.setPosition({ center.x, center.y, center.z - orthoHalfSize - NearClipDistance });
	camera.setDirection({ 0, 0, 1 });
	camera.updateMatrix();
	renderables.updateVisibility(camera, sceneInfo);
	sceneQueue->renderObjects(constants, commandList);

	// Generate mipmaps for all face textures
	for (UINT f = 0; f < AnisoVoxelCascade::FaceCount; f++)
	{
		faceStates[f].Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		computeMips.dispatch(commandList, cascade.voxelFaceTextures[f]);
	}

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(cascade.voxelInfoBuffer.data.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(cascade.voxelInfoBuffer.data.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &b2);
}

void AnisoVoxelization::renderShadowMap(ID3D12GraphicsCommandList* commandList, float extends, const RenderContext& ctx)
{
	shadowMaps->updateShadowMap(shadowMap, ctx.camera->getPosition(), extends * 1.5f);

	CommandsMarker marker(commandList, "AnisoVoxelShadowMap", PixColor::DarkTurquoise);

	shadowRenderables->updateVisibility(shadowMap.camera, shadowRenderablesData);

	shadowMap.texture.PrepareAsDepthTarget(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	ShaderConstantsProvider constants(*frameParams, shadowRenderablesData, shadowMap.camera, *ctx.camera, shadowMap.texture);
	shadowQueue->renderObjects(constants, commandList);

	shadowMap.texture.PrepareAsView(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void AnisoVoxelization::bounceCascade(CommandsData& commands, AnisoVoxelCascade& cascade, const RenderContext& ctx)
{
	// Copy current face textures to previous
	for (UINT f = 0; f < AnisoVoxelCascade::FaceCount; f++)
	{
		cascade.voxelFaceTextures[f].Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
		cascade.voxelPreviousFaceTextures[f].Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

		commands.commandList->CopyResource(cascade.voxelPreviousFaceTextures[f].texture.Get(), cascade.voxelFaceTextures[f].texture.Get());

		cascade.voxelFaceTextures[f].Transition(commands.commandList, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cascade.voxelPreviousFaceTextures[f].Transition(commands.commandList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	struct
	{
		Vector3 CameraPosition;
		float padding;
		Vector3 VoxelsOffset;
		float padding2;
		Vector3 VoxelsSceneSize;
	}
	data = {
		.CameraPosition = ctx.camera->getPosition(),
		.VoxelsOffset = cascade.settings.center - Vector3(cascade.settings.extends),
		.VoxelsSceneSize = Vector3(cascade.settings.extends * 2),
	};

	ShaderTextureView faceViews[AnisoVoxelCascade::FaceCount];
	ShaderTextureView prevFaceViews[AnisoVoxelCascade::FaceCount];
	for (UINT f = 0; f < AnisoVoxelCascade::FaceCount; f++)
	{
		faceViews[f] = cascade.voxelFaceTextures[f].view;
		prevFaceViews[f] = cascade.voxelPreviousFaceTextures[f].view;
	}

	bouncesCS.dispatch(commands.commandList, std::span{ (float*)&data, sizeof(data) / sizeof(float) }, faceViews, prevFaceViews, cascade.voxelInfoBuffer.data->GetGPUVirtualAddress());

	// Transition back and generate mipmaps
	for (UINT f = 0; f < AnisoVoxelCascade::FaceCount; f++)
	{
		cascade.voxelFaceTextures[f].Transition(commands.commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		computeMips.dispatch(commands.commandList, cascade.voxelFaceTextures[f]);
	}
}

void AnisoVoxelization::runBounces(CommandsData& computeCommands, const RenderContext& ctx, const AnisoVoxelTracingParams& params)
{
	CommandsMarker marker(computeCommands.commandList, "AnisoVoxelBounces", PixColor::LightBlue);

	updateCBuffer(ctx.camera->getPosition(), params);

	static UINT idx = 0;

	for (UINT i = 0; i < CascadesCount; i++)
	{
		if (buildCounter[i] && i == (idx % 4))
			bounceCascade(computeCommands, voxelCascades[i], ctx);
	}

	idx++;
}

void AnisoBounceVoxelsCS::dispatch(ID3D12GraphicsCommandList* commandList, const std::span<float>& data, const ShaderTextureView faceViews[AnisoVoxelCascade::FaceCount], const ShaderTextureView prevFaceViews[AnisoVoxelCascade::FaceCount], D3D12_GPU_VIRTUAL_ADDRESS dataAddr)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, data.size(), data.data(), 0);
	commandList->SetComputeRootShaderResourceView(1, dataAddr);

	// Bind 6 previous face textures as SRV inputs
	for (UINT f = 0; f < AnisoVoxelCascade::FaceCount; f++)
	{
		commandList->SetComputeRootDescriptorTable(2 + f, prevFaceViews[f].srvHandle);
	}

	// Bind 6 current face textures as UAV outputs
	for (UINT f = 0; f < AnisoVoxelCascade::FaceCount; f++)
	{
		commandList->SetComputeRootDescriptorTable(2 + AnisoVoxelCascade::FaceCount + f, faceViews[f].uavHandle);
	}

	const UINT threadSize = 4;
	const auto voxelSize = UINT(VoxelSize);
	const uint32_t groupSize = (voxelSize + threadSize - 1) / threadSize;

	commandList->Dispatch(groupSize, groupSize, groupSize);
}
