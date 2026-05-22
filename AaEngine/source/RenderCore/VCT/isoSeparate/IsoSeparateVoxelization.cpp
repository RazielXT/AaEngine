#include "RenderCore/VCT/isoSeparate/IsoSeparateVoxelization.h"
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

void IsoSeparateVoxelization::initialize(RenderSystem& renderSystem, const FrameParameters& params, GraphicsResources& resources, ShadowMaps& shadows, RenderWorld& mgr, DXGI_FORMAT outputFormat)
{
	shadowMaps = &shadows;
	renderWorld = &mgr;
	frameParams = &params;

	shadows.createShadowMap(shadowMap, 256, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, "VoxelShadowMapSeparate");
	shadowRenderables = mgr.getRenderables(Order::Normal);
	shadowQueue = mgr.createQueue({}, MaterialTechnique::DepthShadowmap);

	constexpr float CascadeDistances[CascadesCount] = { 30, 60, 120, 240 };
	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& voxels = voxelCascades[i];
		voxels.initialize("VoxelSeparateCascade" + std::to_string(i), renderSystem.core.device, resources);
		voxels.settings.extends = CascadeDistances[i];
		voxels.idx = i;
	}

	clearColorTexture.Init(renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, VoxelColorFormat);
	clearColorTexture.SetName("VoxelSeparateClearColor");
	clearOccupancyTexture.Init(renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, VoxelOccupancyFormat);
	clearOccupancyTexture.SetName("VoxelSeparateClearOccupancy");

	cbuffer = resources.shaderBuffers.CreateCbufferResource(sizeof(cbufferData), "SceneVoxelInfo");

	computeMips.init(*renderSystem.core.device, "generate3DMipmaps3x", resources.shaders);

	auto bouncesShader = resources.shaders.getShader("voxelBouncesSeparateCS", ShaderType::Compute, ShaderRef{"vct/isoSeparate/voxelBouncesCS.hlsl", "main", "cs_6_6"});
	bouncesCS.init(*renderSystem.core.device, *bouncesShader);

	auto opacityGridShader = resources.shaders.getShader("opacityGridCS", ShaderType::Compute, ShaderRef{"vct/opacityGridCS.hlsl", "CSMain", "cs_6_6"});
	opacityGridCS.init(*renderSystem.core.device, *opacityGridShader);

	auto clearBufferShader = resources.shaders.getShader("clearBufferCS", ShaderType::Compute, ShaderRef{ "utils/clearBufferCS.hlsl", "main", "cs_6_6" });
	clearBufferCS.init(*renderSystem.core.device, *clearBufferShader);

	sceneQueue = mgr.createQueue({ outputFormat }, MaterialTechnique::Voxelize);

	revoxelize();
}

void IsoSeparateVoxelization::shutdown()
{
	shadowMaps->removeShadowMap(shadowMap);
}

void IsoSeparateVoxelization::clear(ID3D12GraphicsCommandList* c)
{
	revoxelize();

	for (auto& cascade : voxelCascades)
	{
		cascade.clearAll(c, clearColorTexture, clearOccupancyTexture, clearBufferCS);
	}

	reset = false;
}

void IsoSeparateVoxelization::revoxelize()
{
	for (auto& b : buildCounter)
	{
		b = 0;
	}
}

void IsoSeparateVoxelization::transitionForCompute(ID3D12GraphicsCommandList* commandList)
{
	TextureTransitions<4 * CascadesCount> tr;

	for (auto& voxel : voxelCascades)
	{
		tr.add(voxel.voxelSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.add(voxel.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.add(voxel.voxelOccupancyTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.add(voxel.voxelPreviousOccupancyTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	tr.push(commandList);
}

void IsoSeparateVoxelization::updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 moveOffset, IsoSeparateVoxelCascade& cascade)
{
	info.voxelSceneSize = cascade.settings.extends * 2;

	float sceneToVoxel = VoxelSize / info.voxelSceneSize;
	info.voxelDensity = sceneToVoxel;

	info.voxelOffset = cascade.settings.center - Vector3(cascade.settings.extends);

	info.moveOffset = moveOffset;
	info.texId = cascade.voxelSceneTexture.view.srvHeapIndex;
	info.texIdPrev = cascade.voxelPreviousSceneTexture.view.srvHeapIndex;
	info.texIdOccupancy = cascade.voxelOccupancyTexture.view.srvHeapIndex;
	info.texIdPrevOccupancy = cascade.voxelPreviousOccupancyTexture.view.srvHeapIndex;
	info.resIdDataBuffer = cascade.voxelInfoBuffer.view.heapIndex;
}

void IsoSeparateVoxelization::updateCBuffer(const Vector3& cameraPosition, const IsoSeparateVoxelTracingParams& params)
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

void IsoSeparateVoxelization::voxelizeCascades(CommandsData& commands, GpuTextureStates& viewportOutput, const RenderContext& ctx)
{
	if (reset)
		clear(commands.commandList);

	TextureTransitions<4 * CascadesCount> tr;

	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& cascade = voxelCascades[i];
		TextureStatePair voxelSceneTexture_state = { &cascade.voxelSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		TextureStatePair voxelPreviousSceneTexture_state = { &cascade.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		TextureStatePair voxelOccupancyTexture_state = { &cascade.voxelOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		TextureStatePair voxelPreviousOccupancyTexture_state = { &cascade.voxelPreviousOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };

		if (!buildCounter[i])
		{
			buildCounter[i]++;
			voxelizeCascade(commands.commandList, voxelSceneTexture_state, voxelPreviousSceneTexture_state, voxelOccupancyTexture_state, voxelPreviousOccupancyTexture_state, viewportOutput, cascade, ctx);
		}

		tr.add(voxelSceneTexture_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(voxelPreviousSceneTexture_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(voxelOccupancyTexture_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(voxelPreviousOccupancyTexture_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	tr.push(commands.commandList);
}

void IsoSeparateVoxelization::voxelizeCascade(ID3D12GraphicsCommandList* commandList, TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, TextureStatePair& voxelOccupancy, TextureStatePair& prevVoxelOccupancy, GpuTextureStates& viewportOutput, IsoSeparateVoxelCascade& cascade, const RenderContext& ctx)
{
	renderShadowMap(commandList, cascade.settings.extends, ctx);

	viewportOutput.texture->PrepareAsRenderTarget(commandList, viewportOutput.previousState, true);

	cascade.prepareForVoxelization(commandList, voxelScene, prevVoxelScene, voxelOccupancy, prevVoxelOccupancy, clearColorTexture, clearOccupancyTexture, clearBufferCS);
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

	camera.setPosition({ center.x, center.y, center.z - orthoHalfSize - NearClipDistance });
	camera.setDirection({ 0, 0, 1 });
	camera.updateMatrix();
	renderables.updateVisibility(camera, sceneInfo);
	sceneQueue->renderObjects(constants, commandList);

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(cascade.voxelOccupancyTexture.texture.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	voxelScene.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	voxelOccupancy.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	computeMips.dispatch(commandList, cascade.voxelSceneTexture);
	computeMips.dispatch(commandList, cascade.voxelOccupancyTexture);
	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(cascade.voxelInfoBuffer.data.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(cascade.voxelInfoBuffer.data.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &b2);
}

void IsoSeparateVoxelization::renderShadowMap(ID3D12GraphicsCommandList* commandList, float extends, const RenderContext& ctx)
{
	shadowMaps->updateShadowMap(shadowMap, ctx.camera->getPosition(), extends * 1.5f);

	CommandsMarker marker(commandList, "VoxelShadowMapSeparate", PixColor::DarkTurquoise);

	shadowRenderables->updateVisibility(shadowMap.camera, shadowRenderablesData);

	shadowMap.texture.PrepareAsDepthTarget(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	ShaderConstantsProvider constants(*frameParams, shadowRenderablesData, shadowMap.camera, *ctx.camera, shadowMap.texture);
	shadowQueue->renderObjects(constants, commandList);

	shadowMap.texture.PrepareAsView(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void IsoSeparateVoxelization::bounceCascade(CommandsData& commands, IsoSeparateVoxelCascade& cascade, const RenderContext& ctx)
{
	TextureStatePair voxelSceneTexture_state = { &cascade.voxelSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
	TextureStatePair voxelPreviousSceneTexture_state = { &cascade.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
	TextureStatePair voxelOccupancyTexture_state = { &cascade.voxelOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
	TextureStatePair voxelPreviousOccupancyTexture_state = { &cascade.voxelPreviousOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };

	TextureTransitions<4> tr;
	tr.add(voxelSceneTexture_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
	tr.add(voxelPreviousSceneTexture_state, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.add(voxelOccupancyTexture_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
	tr.add(voxelPreviousOccupancyTexture_state, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.push(commands.commandList);

	commands.commandList->CopyResource(cascade.voxelPreviousSceneTexture.texture.Get(), cascade.voxelSceneTexture.texture.Get());
	commands.commandList->CopyResource(cascade.voxelPreviousOccupancyTexture.texture.Get(), cascade.voxelOccupancyTexture.texture.Get());

	tr.add(voxelSceneTexture_state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tr.add(voxelPreviousSceneTexture_state, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.add(voxelOccupancyTexture_state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tr.add(voxelPreviousOccupancyTexture_state, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.push(commands.commandList);

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

	bouncesCS.dispatch(commands.commandList, std::span{ (float*) & data, sizeof(data) / sizeof(float)}, cascade.voxelSceneTexture.view, cascade.voxelOccupancyTexture.view, cascade.voxelPreviousSceneTexture.view, cascade.voxelPreviousOccupancyTexture.view, cascade.voxelInfoBuffer.data->GetGPUVirtualAddress());

	voxelSceneTexture_state.Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	voxelOccupancyTexture_state.Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	computeMips.dispatch(commands.commandList, cascade.voxelSceneTexture);
	computeMips.dispatch(commands.commandList, cascade.voxelOccupancyTexture);

	auto commandList = commands.commandList;
	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(cascade.voxelOccupancyTexture.texture.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	cascade.opacityGridTextureState.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	opacityGridCS.dispatch(commandList, cascade.voxelOccupancyTexture.view, cascade.opacityGridTexture.view);
	cascade.opacityGridTextureState.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void IsoSeparateVoxelization::runBounces(CommandsData& computeCommands, const RenderContext& ctx, const IsoSeparateVoxelTracingParams& params)
{
	CommandsMarker marker(computeCommands.commandList, "VoxelSeparateBounces", PixColor::LightBlue);

	updateCBuffer(ctx.camera->getPosition(), params);

	static UINT idx = 0;

	for (UINT i = 0; i < CascadesCount; i++)
	{
		if (buildCounter[i] && i == (idx % 4))
			bounceCascade(computeCommands, voxelCascades[i], ctx);
	}

	idx++;
}

void IsoSeparateBounceVoxelsCS::dispatch(ID3D12GraphicsCommandList* commandList, const std::span<float>& data, const ShaderTextureView& targetColor, const ShaderTextureView& targetOccupancy, const ShaderTextureView& sourceColor, const ShaderTextureView& sourceOccupancy, D3D12_GPU_VIRTUAL_ADDRESS dataAddr)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, data.size(), data.data(), 0);
	commandList->SetComputeRootShaderResourceView(1, dataAddr);
	commandList->SetComputeRootDescriptorTable(2, sourceColor.srvHandle);
	commandList->SetComputeRootDescriptorTable(3, sourceOccupancy.srvHandle);
	commandList->SetComputeRootDescriptorTable(4, targetColor.uavHandle);
	commandList->SetComputeRootDescriptorTable(5, targetOccupancy.uavHandle);

	const UINT threadSize = 4;
	const auto voxelSize = UINT(VoxelSize);
	const uint32_t groupSize = (voxelSize + threadSize - 1) / threadSize;

	commandList->Dispatch(groupSize, groupSize, groupSize);
}

void IsoSeparateOpacityGridCS::dispatch(ID3D12GraphicsCommandList* commandList, const ShaderTextureView& sourceOccupancy, const ShaderTextureView& targetOpacityGrid)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRootDescriptorTable(0, sourceOccupancy.srvHandle);
	commandList->SetComputeRootDescriptorTable(1, targetOpacityGrid.uavHandle);

	const UINT groupSize = 32;
	commandList->Dispatch(groupSize, groupSize, groupSize);
}