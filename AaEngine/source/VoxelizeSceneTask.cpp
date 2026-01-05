#include "VoxelizeSceneTask.h"
#include "SceneManager.h"
#include "MaterialResources.h"
#include "TextureResources.h"
#include "DebugWindow.h"
#include "StringUtils.h"
#include <format>

static VoxelizeSceneTask* instance = nullptr;

VoxelizeSceneTask::VoxelizeSceneTask(RenderProvider p, SceneManager& s, ShadowMaps& shadows) : shadowMaps(shadows), CompositorTask(p, s)
{
	instance = this;
}

VoxelizeSceneTask::~VoxelizeSceneTask()
{
	running = false;

	if (eventBegin)
	{
		SetEvent(eventBegin);
		worker.join();
		voxelizeCommands.deinit();
		CloseHandle(eventBegin);
		CloseHandle(eventFinish);
	}

	instance = nullptr;
}

VoxelizeSceneTask& VoxelizeSceneTask::Get()
{
	return *instance;
}

void VoxelizeSceneTask::clear()
{
	reset = true;
}

bool VoxelizeSceneTask::writesSyncComputeCommands(CompositorPass& pass) const
{
	return pass.info.entry == "Bounces";
}

bool VoxelizeSceneTask::writesSyncCommands(CompositorPass& pass) const
{
	return pass.info.entry == "EndFrame";
}

void VoxelizeSceneTask::revoxelize()
{
	for (auto& b : buildCounter)
	{
		b = 0;
	}
}

void VoxelizeSceneTask::clear(ID3D12GraphicsCommandList* c)
{
	revoxelize();

	for (auto& cascade : voxelCascades)
	{
		cascade.clearAll(c, clearSceneTexture, clearBufferCS);
	}

	reset = false;
}

constexpr float VoxelSize = 128.f;

constexpr DXGI_FORMAT VoxelFormat = DXGI_FORMAT_R8G8B8A8_UNORM; //DXGI_FORMAT_R16G16B16A16_FLOAT;

AsyncTasksInfo VoxelizeSceneTask::initialize(CompositorPass& pass)
{
	if (pass.info.entry != "Voxelize")
		return {};

	constexpr float CascadeDistances[CascadesCount] = { 200, 600, 2000, 6000 };
	for (UINT i = 0; auto& voxels : voxelCascades)
	{
		voxels.initialize("VoxelCascade" + std::to_string(i), provider.renderSystem.core.device, provider.resources);
		voxels.settings.extends = CascadeDistances[i];
		voxels.idx = i++;
	}

	clearSceneTexture.Init(provider.renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	clearSceneTexture.SetName("VoxelClear");

	cbuffer = provider.resources.shaderBuffers.CreateCbufferResource(sizeof(cbufferData), "SceneVoxelInfo");
	frameCbuffer = provider.resources.shaderBuffers.CreateCbufferResource(sizeof(Matrix), "FrameInfo");

	computeMips.init(*provider.renderSystem.core.device, "generate3DMipmaps3x", provider.resources.shaders);

	auto bouncesShader = provider.resources.shaders.getShader("voxelBouncesCS", ShaderType::Compute, ShaderRef{"voxelBouncesCS.hlsl", "main", "cs_6_6"});
	bouncesCS.init(*provider.renderSystem.core.device, *bouncesShader);

	auto clearBufferShader = provider.resources.shaders.getShader("clearBufferCS", ShaderType::Compute, ShaderRef{ "utils/clearBufferCS.hlsl", "main", "cs_6_6" });
	clearBufferCS.init(*provider.renderSystem.core.device, *clearBufferShader);

	sceneQueue = sceneMgr.createQueue({ pass.targets.front().texture->format }, MaterialTechnique::Voxelize);

	AsyncTasksInfo tasks;
	eventBegin = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	eventFinish = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	voxelizeCommands = provider.renderSystem.core.CreateCommandList(L"Voxelize", PixColor::Voxelize);

	worker = std::thread([this, &pass]
		{
			while (WaitForSingleObject(eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				voxelizeCascades(pass.targets.front());
				
				SetEvent(eventFinish);
			}
		});

	tasks.emplace_back(eventFinish, voxelizeCommands);

	revoxelize();

	return tasks;
}

void VoxelizeSceneTask::run(RenderContext& renderCtx, CommandsData& syncCommands, CompositorPass& pass)
{
	if (pass.info.entry == "EndFrame")
	{
		//get ready for compute queue
		for ( auto& voxel : voxelCascades)
		{
			voxel.voxelSceneTexture.Transition(syncCommands.commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			voxel.voxelPreviousSceneTexture.Transition(syncCommands.commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
		return;
	}
}

void VoxelizeSceneTask::run(RenderContext& renderCtx, CompositorPass& pass)
{
	{
		auto m = XMMatrixMultiplyTranspose(renderCtx.camera->getViewMatrix(), renderCtx.camera->getProjectionMatrixNoReverse());
		Matrix data;
		XMStoreFloat4x4(&data, m);
		auto& cbufferResource = *frameCbuffer.data[provider.params.frameIndex];
		memcpy(cbufferResource.Memory(), &data, sizeof(data));
	}

	ctx = renderCtx;
	SetEvent(eventBegin);
}

void VoxelizeSceneTask::runCompute(RenderContext& ctx, CommandsData& computeCommands, CompositorPass& pass)
{
	CommandsMarker marker(computeCommands.commandList, "VoxelBounces", PixColor::LightBlue);

	auto cameraPosition = ctx.camera->getPosition();

	Vector3 diff[CascadesCount];
	for (UINT i = 0; auto& voxel : voxelCascades)
	{
		diff[i] = voxel.update(cameraPosition);
		if (diff[i].x != 0 || diff[i].y != 0 || diff[i].z != 0)
			buildCounter[i] = 0;

		updateCBufferCascade(cbufferData.cascadeInfo[i], diff[i], voxel);
		i++;
	}

	updateCBuffer(provider.renderSystem.core.frameIndex);

	static UINT idx = 0;
	constexpr UINT VoxelizeInterations = 1;

	for (UINT i = 0; auto& voxel : voxelCascades)
	{
		if (buildCounter[i] < VoxelizeInterations)
		{
		}
		else if (i == (idx % 4))
			bounceCascade(computeCommands, voxel);

		i++;
	}

	idx++;
}

void VoxelizeSceneTask::updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 diff, SceneVoxelsCascade& cascade)
{
	info.voxelSceneSize = cascade.settings.extends * 2;

	float sceneToVoxel = VoxelSize / info.voxelSceneSize;
	info.voxelDensity = sceneToVoxel;

	info.voxelOffset = cascade.settings.center - Vector3(cascade.settings.extends);

	info.diff = diff;
	info.texId = cascade.voxelSceneTexture.view.srvHeapIndex;
	info.texIdBounces = cascade.voxelPreviousSceneTexture.view.srvHeapIndex;
	info.resIdDataBuffer = cascade.dataBufferView.heapIndex;
}

void VoxelizeSceneTask::updateCBuffer(UINT frameIndex)
{
	auto& state = imgui::DebugWindow::state;

	cbufferData.steppingBounces = state.voxelSteppingBounces;
	cbufferData.steppingDiffuse = state.voxelSteppingDiffuse;
	cbufferData.middleConeRatioDistance = state.middleConeRatioDistance;
	cbufferData.sideConeRatioDistance = state.sideConeRatioDistance;
	cbufferData.voxelizeLighting = 0.0f;

	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &cbufferData, sizeof(cbufferData));
}

void VoxelizeSceneTask::voxelizeCascades(GpuTextureStates& viewportOutput)
{
	auto marker = provider.renderSystem.core.StartCommandList(voxelizeCommands);
	constexpr UINT VoxelizeInterations = 1;

	if (reset)
		clear(voxelizeCommands.commandList);

	for (UINT i = 0; auto& cascade : voxelCascades)
	{
		TextureStatePair voxelSceneTexture_state = { &cascade.voxelSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		TextureStatePair voxelPreviousSceneTexture_state = { &cascade.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };

		if (buildCounter[i] < VoxelizeInterations)
		{
			buildCounter[i]++;
			voxelizeCascade(voxelSceneTexture_state, voxelPreviousSceneTexture_state, viewportOutput, cascade);
		}

		voxelSceneTexture_state.Transition(voxelizeCommands.commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		voxelPreviousSceneTexture_state.Transition(voxelizeCommands.commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		i++;
	}

	marker.close();
}

void VoxelizeSceneTask::voxelizeCascade(TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, GpuTextureStates& viewportOutput, SceneVoxelsCascade& cascade)
{
	viewportOutput.texture->PrepareAsRenderTarget(voxelizeCommands.commandList, viewportOutput.previousState, true);

	cascade.prepareForVoxelization(voxelizeCommands.commandList, voxelScene, prevVoxelScene, clearSceneTexture, clearBufferCS);
	auto b = CD3DX12_RESOURCE_BARRIER::Transition(cascade.dataBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	voxelizeCommands.commandList->ResourceBarrier(1, &b);

	auto center = cascade.settings.center;
	auto orthoHalfSize = cascade.settings.extends;

	const float NearClipDistance = 1;

	Camera camera;
	camera.setOrthographicCamera(orthoHalfSize * 2, orthoHalfSize * 2, NearClipDistance, NearClipDistance + orthoHalfSize * 2);

	static RenderObjectsVisibilityData sceneInfo;
	auto& renderables = *sceneMgr.getRenderables(Order::Normal);

	ShaderConstantsProvider constants(provider.params, sceneInfo, camera, *viewportOutput.texture);
	constants.uavBarrier = cascade.dataBuffer.Get();

	sceneQueue->iterateMaterials([&cascade](AssignedMaterial* material)
		{
			material->SetUAV(cascade.dataBuffer.Get(), 0);
			material->SetParameter(FastParam::VoxelIdx, &cascade.idx);
		});

	//from all 3 axes
	//Z
	camera.setPosition({ center.x, center.y, center.z - orthoHalfSize - NearClipDistance });
	camera.setDirection({ 0, 0, 1 });
	camera.updateMatrix();
	renderables.updateVisibility(camera, sceneInfo);
	sceneQueue->renderObjects(constants, voxelizeCommands.commandList);
	//X
	camera.setPosition({ center.x - orthoHalfSize - NearClipDistance, center.y, center.z });
	camera.setDirection({ 1, 0, 0 });
	camera.updateMatrix();
	//renderables.updateVisibility(camera, sceneInfo);
	sceneQueue->renderObjects(constants, voxelizeCommands.commandList);
	//Y
	camera.setPosition({ center.x, center.y - orthoHalfSize - NearClipDistance, center.z });
	camera.pitch(XM_PIDIV2);
	camera.updateMatrix();
	//renderables.updateVisibility(camera, sceneInfo);
	sceneQueue->renderObjects(constants, voxelizeCommands.commandList);

	// mipmaps
	voxelScene.Transition(voxelizeCommands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	computeMips.dispatch(voxelizeCommands.commandList, cascade.voxelSceneTexture);
	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(cascade.dataBuffer.Get());
		voxelizeCommands.commandList->ResourceBarrier(1, &uavBarrier);
	}

	auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(cascade.dataBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	voxelizeCommands.commandList->ResourceBarrier(1, &b2);
}

void VoxelizeSceneTask::bounceCascade(CommandsData& commands, SceneVoxelsCascade& cascade)
{
	TextureStatePair voxelSceneTexture_state = { &cascade.voxelSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
	TextureStatePair voxelPreviousSceneTexture_state = { &cascade.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };

	voxelSceneTexture_state.Transition(commands.commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	voxelPreviousSceneTexture_state.Transition(commands.commandList, D3D12_RESOURCE_STATE_COPY_DEST);

	commands.commandList->CopyResource(cascade.voxelPreviousSceneTexture.texture.Get(), cascade.voxelSceneTexture.texture.Get());

	voxelSceneTexture_state.Transition(commands.commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	voxelPreviousSceneTexture_state.Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	XM_ALIGNED_STRUCT(16)
	{
		Vector3 cameraPosition;
		float padding;
		Vector3 VoxelsOffset;
		float padding2;
		Vector3 VoxelsSceneSize;
	}
	data = { ctx.camera->getPosition(), {}, cascade.settings.center - Vector3(cascade.settings.extends), {}, Vector3(cascade.settings.extends * 2) };

	bouncesCS.dispatch(commands.commandList, std::span{ (float*) & data, 11 }, cascade.voxelSceneTexture.view, cascade.voxelPreviousSceneTexture.view, cascade.dataBuffer->GetGPUVirtualAddress());

	voxelSceneTexture_state.Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	computeMips.dispatch(commands.commandList, cascade.voxelSceneTexture);

	//voxelSceneTexture_state.Transition(commands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void SceneVoxelsCascade::initialize(const std::string& n, ID3D12Device* device, GraphicsResources& resources)
{
	name = n;

	voxelSceneTexture.Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	voxelSceneTexture.SetName(name.c_str());
	resources.descriptors.createUAVView(voxelSceneTexture);
	resources.textures.setNamedUAV(name, voxelSceneTexture.uav.front());
	resources.descriptors.createTextureView(voxelSceneTexture);
	resources.textures.setNamedTexture(name, voxelSceneTexture.view);

	auto bounceName = name + "Bounces";
	voxelPreviousSceneTexture.Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	voxelPreviousSceneTexture.SetName(bounceName.c_str());
	resources.descriptors.createTextureView(voxelPreviousSceneTexture);
	resources.textures.setNamedTexture(bounceName, voxelPreviousSceneTexture.view);

	dataBuffer = resources.shaderBuffers.CreateStructuredBuffer(DataElementSize * DataElementCount);
	dataBuffer->SetName(as_wstring(name + "DataBuffer").c_str());
	dataBufferView = resources.descriptors.createBufferView(dataBuffer.Get(), DataElementSize, DataElementCount);
}

void SceneVoxelsCascade::prepareForVoxelization(ID3D12GraphicsCommandList* commandList, TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, const GpuTexture3D& clearSceneTexture, ClearBufferComputeShader& clearBufferCS)
{
	clearBufferCS.dispatch(commandList, dataBuffer.Get(), DataElementSize * DataElementCount / sizeof(float));

	voxelScene.Transition(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	prevVoxelScene.Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), voxelSceneTexture.texture.Get());

	//fast clear
	voxelScene.Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyResource(voxelSceneTexture.texture.Get(), clearSceneTexture.texture.Get());
//	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), clearSceneTexture.texture.Get());

	auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(dataBuffer.Get());
	commandList->ResourceBarrier(1, &uavBarrier);

	voxelScene.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	prevVoxelScene.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void SceneVoxelsCascade::clearAll(ID3D12GraphicsCommandList* commandList, const GpuTexture3D& clearSceneTexture, ClearBufferComputeShader& clearBufferCS)
{
	clearBufferCS.dispatch(commandList, dataBuffer.Get(), DataElementSize * DataElementCount / sizeof(float));

	voxelSceneTexture.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	voxelPreviousSceneTexture.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

	commandList->CopyResource(voxelSceneTexture.texture.Get(), clearSceneTexture.texture.Get());
	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), clearSceneTexture.texture.Get());

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(dataBuffer.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	voxelSceneTexture.Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	voxelPreviousSceneTexture.Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

static Vector3 SnapToGrid(Vector3 position, float gridStep)
{
	position.x = std::round(position.x / gridStep) * gridStep;
	position.y = std::round(position.y / gridStep) * gridStep;
	position.z = std::round(position.z / gridStep) * gridStep;
	return position;
}

Vector3 SceneVoxelsCascade::update(const Vector3& cameraPosition)
{
	float step = settings.extends * 2 / VoxelSize;

	const float VoxelMipmapStep = 32;
	step *= VoxelMipmapStep;

	settings.center = SnapToGrid(cameraPosition, step);

	auto diff = (settings.lastCenter - settings.center) / step;
	settings.lastCenter = settings.center;

// 	std::string logg = std::format("diff {} {} {} \n", diff.x, diff.y, diff.z);
// 	OutputDebugStringA(logg.c_str());

	return diff;
}

void BounceVoxelsCS::dispatch(ID3D12GraphicsCommandList* commandList, const std::span<float>& data, const ShaderTextureView& target, const ShaderTextureView& source, D3D12_GPU_VIRTUAL_ADDRESS dataAddr)
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
