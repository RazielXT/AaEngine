#include "RenderCore/VCT/iso/IsoVoxelization.h"
#include "Scene/SceneManager.h"
#include "Scene/RenderQueue.h"
#include "Resources/Material/MaterialResources.h"
#include "Resources/Textures/TextureResources.h"
#include "FrameCompositor/RenderContext.h"
#include "RenderCore/RenderSystem.h"
#include "Resources/Shader/ShaderConstantsProvider.h"

static constexpr float VoxelSize = 128.f;
static constexpr DXGI_FORMAT VoxelFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

void IsoVoxelization::initialize(RenderSystem& renderSystem, const FrameParameters& params, GraphicsResources& resources, ShadowMaps& shadows, SceneManager& mgr, DXGI_FORMAT outputFormat)
{
	shadowMaps = &shadows;
	sceneMgr = &mgr;
	frameParams = &params;

	shadows.createShadowMap(shadowMap, 256, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, "VoxelShadowMap");
	shadowRenderables = mgr.getRenderables(Order::Normal);
	shadowQueue = mgr.createQueue({}, MaterialTechnique::DepthShadowmap);

	constexpr float CascadeDistances[CascadesCount] = { 200, 400, 800, 1600 };
	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& voxels = voxelCascades[i];
		voxels.initialize("VoxelCascade" + std::to_string(i), renderSystem.core.device, resources);
		voxels.settings.extends = CascadeDistances[i];
		voxels.idx = i;
	}

	clearSceneTexture.Init(renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	clearSceneTexture.SetName("VoxelClear");

	cbuffer = resources.shaderBuffers.CreateCbufferResource(sizeof(cbufferData), "SceneVoxelInfo");

	computeMips.init(*renderSystem.core.device, "generate3DMipmaps3x", resources.shaders);

	auto bouncesShader = resources.shaders.getShader("voxelBouncesCS", ShaderType::Compute, ShaderRef{"vct/iso/voxelBouncesCS.hlsl", "main", "cs_6_6"});
	bouncesCS.init(*renderSystem.core.device, *bouncesShader);

	auto clearBufferShader = resources.shaders.getShader("clearBufferCS", ShaderType::Compute, ShaderRef{ "utils/clearBufferCS.hlsl", "main", "cs_6_6" });
	clearBufferCS.init(*renderSystem.core.device, *clearBufferShader);

	sceneQueue = mgr.createQueue({ outputFormat }, MaterialTechnique::Voxelize);

	revoxelize();
}

void IsoVoxelization::shutdown()
{
	shadowMaps->removeShadowMap(shadowMap);
}

void IsoVoxelization::clear(ID3D12GraphicsCommandList* c)
{
	revoxelize();

	for (auto& cascade : voxelCascades)
	{
		cascade.clearAll(c, clearSceneTexture, clearBufferCS);
	}

	reset = false;
}

void IsoVoxelization::revoxelize()
{
	for (auto& b : buildCounter)
	{
		b = 0;
	}
}

void IsoVoxelization::transitionForCompute(ID3D12GraphicsCommandList* commandList)
{
	TextureTransitions<2 * CascadesCount> tr;

	for (auto& voxel : voxelCascades)
	{
		tr.add(voxel.voxelSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.add(voxel.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	tr.push(commandList);
}

void IsoVoxelization::updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 moveOffset, IsoVoxelCascade& cascade)
{
	info.voxelSceneSize = cascade.settings.extends * 2;

	float sceneToVoxel = VoxelSize / info.voxelSceneSize;
	info.voxelDensity = sceneToVoxel;

	info.voxelOffset = cascade.settings.center - Vector3(cascade.settings.extends);

	info.moveOffset = moveOffset;
	info.texId = cascade.voxelSceneTexture.view.srvHeapIndex;
	info.texIdPrev = cascade.voxelPreviousSceneTexture.view.srvHeapIndex;
	info.resIdDataBuffer = cascade.voxelInfoBuffer.view.heapIndex;
}

void IsoVoxelization::updateCBuffer(const Vector3& cameraPosition, const VoxelTracingParams& params)
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

void IsoVoxelization::voxelizeCascades(CommandsData& commands, GpuTextureStates& viewportOutput, const RenderContext& ctx)
{
	if (reset)
		clear(commands.commandList);

	TextureTransitions<2 * CascadesCount> tr;

	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& cascade = voxelCascades[i];
		TextureStatePair voxelSceneTexture_state = { &cascade.voxelSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		TextureStatePair voxelPreviousSceneTexture_state = { &cascade.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };

		if (!buildCounter[i])
		{
			buildCounter[i]++;
			voxelizeCascade(commands.commandList, voxelSceneTexture_state, voxelPreviousSceneTexture_state, viewportOutput, cascade, ctx);
		}

		tr.add(voxelSceneTexture_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(voxelPreviousSceneTexture_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	tr.push(commands.commandList);
}

void IsoVoxelization::voxelizeCascade(ID3D12GraphicsCommandList* commandList, TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, GpuTextureStates& viewportOutput, IsoVoxelCascade& cascade, const RenderContext& ctx)
{
	renderShadowMap(commandList, cascade.settings.extends, ctx);

	viewportOutput.texture->PrepareAsRenderTarget(commandList, viewportOutput.previousState, true);

	cascade.prepareForVoxelization(commandList, voxelScene, prevVoxelScene, clearSceneTexture, clearBufferCS);
	auto b = CD3DX12_RESOURCE_BARRIER::Transition(cascade.voxelInfoBuffer.data.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &b);

	auto center = cascade.settings.center;
	auto orthoHalfSize = cascade.settings.extends;

	const float NearClipDistance = 1;

	Camera camera;
	camera.setOrthographicCamera(orthoHalfSize * 2, orthoHalfSize * 2, NearClipDistance, NearClipDistance + orthoHalfSize * 2);

	static RenderObjectsVisibilityData sceneInfo;
	auto& renderables = *sceneMgr->getRenderables(Order::Normal);

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

	//Z
	camera.setPosition({ center.x, center.y, center.z - orthoHalfSize - NearClipDistance });
	camera.setDirection({ 0, 0, 1 });
	camera.updateMatrix();
	renderables.updateVisibility(camera, sceneInfo);
	sceneQueue->renderObjects(constants, commandList);

	// mipmaps
	voxelScene.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	computeMips.dispatch(commandList, cascade.voxelSceneTexture);
	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(cascade.voxelInfoBuffer.data.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(cascade.voxelInfoBuffer.data.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &b2);
}

void IsoVoxelization::renderShadowMap(ID3D12GraphicsCommandList* commandList, float extends, const RenderContext& ctx)
{
	shadowMaps->updateShadowMap(shadowMap, ctx.camera->getPosition(), extends * 1.5f);

	CommandsMarker marker(commandList, "VoxelShadowMap", PixColor::DarkTurquoise);

	shadowRenderables->updateVisibility(shadowMap.camera, shadowRenderablesData);

	shadowMap.texture.PrepareAsDepthTarget(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	ShaderConstantsProvider constants(*frameParams, shadowRenderablesData, shadowMap.camera, *ctx.camera, shadowMap.texture);
	shadowQueue->renderObjects(constants, commandList);

	shadowMap.texture.PrepareAsView(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void IsoVoxelization::bounceCascade(CommandsData& commands, IsoVoxelCascade& cascade, const RenderContext& ctx)
{
	TextureStatePair voxelSceneTexture_state = { &cascade.voxelSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
	TextureStatePair voxelPreviousSceneTexture_state = { &cascade.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };

	TextureTransitions<2> tr;
	tr.add(voxelSceneTexture_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
	tr.add(voxelPreviousSceneTexture_state, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.push(commands.commandList);

	commands.commandList->CopyResource(cascade.voxelPreviousSceneTexture.texture.Get(), cascade.voxelSceneTexture.texture.Get());

	tr.add(voxelSceneTexture_state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tr.add(voxelPreviousSceneTexture_state, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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

	bouncesCS.dispatch(commands.commandList, std::span{ (float*) & data, sizeof(data) / sizeof(float)}, cascade.voxelSceneTexture.view, cascade.voxelPreviousSceneTexture.view, cascade.voxelInfoBuffer.data->GetGPUVirtualAddress());

	voxelSceneTexture_state.Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	computeMips.dispatch(commands.commandList, cascade.voxelSceneTexture);
}

void IsoVoxelization::runBounces(CommandsData& computeCommands, const RenderContext& ctx, const VoxelTracingParams& params)
{
	CommandsMarker marker(computeCommands.commandList, "VoxelBounces", PixColor::LightBlue);

	updateCBuffer(ctx.camera->getPosition(), params);

	static UINT idx = 0;

	for (UINT i = 0; i < CascadesCount; i++)
	{
		if (buildCounter[i] && i == (idx % 4))
			bounceCascade(computeCommands, voxelCascades[i], ctx);
	}

	idx++;
}

void IsoBounceVoxelsCS::dispatch(ID3D12GraphicsCommandList* commandList, const std::span<float>& data, const ShaderTextureView& target, const ShaderTextureView& source, D3D12_GPU_VIRTUAL_ADDRESS dataAddr)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, data.size(), data.data(), 0);
	commandList->SetComputeRootShaderResourceView(1, dataAddr);
	commandList->SetComputeRootDescriptorTable(2, source.srvHandle);
	commandList->SetComputeRootDescriptorTable(3, target.uavHandle);

	const UINT threadSize = 4;
	const auto voxelSize = UINT(VoxelSize);
	const uint32_t groupSize = (voxelSize + threadSize - 1) / threadSize;

	commandList->Dispatch(groupSize, groupSize, groupSize);
}
