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
	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& voxels = voxelCascades[i];
		voxels.initialize("VoxelCascade" + std::to_string(i), provider.renderSystem.core.device, provider.resources);
		voxels.settings.extends = CascadeDistances[i];
		voxels.idx = i;
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
		TextureTransitions<2 * CascadesCount> tr;

		//get ready for compute queue
		for (auto& voxel : voxelCascades)
		{
			tr.add(voxel.voxelSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			tr.add(voxel.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}

		tr.push(syncCommands.commandList);
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

	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& voxel = voxelCascades[i];
		Vector3 movement = voxel.update(cameraPosition);
		if (movement != Vector3::Zero)
			buildCounter[i] = 0;

		updateCBufferCascade(cbufferData.cascadeInfo[i], movement, voxel);
	}

	updateCBuffer(provider.renderSystem.core.frameIndex);

	static UINT idx = 0;

	for (UINT i = 0; i < CascadesCount; i++)
	{
		if (buildCounter[i] && i == (idx % 4))
			bounceCascade(computeCommands, voxelCascades[i]);
	}

	idx++;
}

void VoxelizeSceneTask::updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 moveOffset, SceneVoxelsCascade& cascade)
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

void VoxelizeSceneTask::updateCBuffer(UINT frameIndex)
{
	cbufferData.middleConeRatioDistance = params.middleConeRatioDistance;
	cbufferData.sideConeRatioDistance = params.sideConeRatioDistance;

	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &cbufferData, sizeof(cbufferData));
}

void VoxelizeSceneTask::voxelizeCascades(GpuTextureStates& viewportOutput)
{
	auto marker = provider.renderSystem.core.StartCommandList(voxelizeCommands);

	if (reset)
		clear(voxelizeCommands.commandList);

	TextureTransitions<2 * CascadesCount> tr;

	for (UINT i = 0; i < CascadesCount; i++)
	{
		auto& cascade = voxelCascades[i];
		TextureStatePair voxelSceneTexture_state = { &cascade.voxelSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		TextureStatePair voxelPreviousSceneTexture_state = { &cascade.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };

		if (!buildCounter[i])
		{
			buildCounter[i]++;
			voxelizeCascade(voxelSceneTexture_state, voxelPreviousSceneTexture_state, viewportOutput, cascade);
		}

		tr.add(voxelSceneTexture_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(voxelPreviousSceneTexture_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	tr.push(voxelizeCommands.commandList);

	marker.close();
}

void VoxelizeSceneTask::voxelizeCascade(TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, GpuTextureStates& viewportOutput, SceneVoxelsCascade& cascade)
{
	viewportOutput.texture->PrepareAsRenderTarget(voxelizeCommands.commandList, viewportOutput.previousState, true);

	cascade.prepareForVoxelization(voxelizeCommands.commandList, voxelScene, prevVoxelScene, clearSceneTexture, clearBufferCS);
	auto b = CD3DX12_RESOURCE_BARRIER::Transition(cascade.voxelInfoBuffer.data.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	voxelizeCommands.commandList->ResourceBarrier(1, &b);

	auto center = cascade.settings.center;
	auto orthoHalfSize = cascade.settings.extends;

	const float NearClipDistance = 1;

	Camera camera;
	camera.setOrthographicCamera(orthoHalfSize * 2, orthoHalfSize * 2, NearClipDistance, NearClipDistance + orthoHalfSize * 2);

	static RenderObjectsVisibilityData sceneInfo;
	auto& renderables = *sceneMgr.getRenderables(Order::Normal);

	ShaderConstantsProvider constants(provider.params, sceneInfo, camera, *viewportOutput.texture);
	constants.uavBarrier = cascade.voxelInfoBuffer.data.Get();

	sceneQueue->iterateMaterials([&cascade](AssignedMaterial* material)
		{
			material->SetUAV(cascade.voxelInfoBuffer.data.Get(), 0);
			material->SetParameter("VoxelIdx", &cascade.idx);
		});

	//Z
	camera.setPosition({ center.x, center.y, center.z - orthoHalfSize - NearClipDistance });
	camera.setDirection({ 0, 0, 1 });
	camera.updateMatrix();
	renderables.updateVisibility(camera, sceneInfo);
 	sceneQueue->renderObjects(constants, voxelizeCommands.commandList);
	//X
//	camera.setPosition({ center.x - orthoHalfSize - NearClipDistance, center.y, center.z });
//	camera.setDirection({ 1, 0, 0 });
//	camera.updateMatrix();
// 	sceneQueue->renderObjects(constants, voxelizeCommands.commandList);
	//Y
//	camera.setPosition({ center.x, center.y - orthoHalfSize - NearClipDistance, center.z });
//	camera.pitch(XM_PIDIV2);
//	camera.updateMatrix();
//	sceneQueue->renderObjects(constants, voxelizeCommands.commandList);

	// mipmaps
	voxelScene.Transition(voxelizeCommands.commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	computeMips.dispatch(voxelizeCommands.commandList, cascade.voxelSceneTexture);
	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(cascade.voxelInfoBuffer.data.Get());
		voxelizeCommands.commandList->ResourceBarrier(1, &uavBarrier);
	}

	auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(cascade.voxelInfoBuffer.data.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	voxelizeCommands.commandList->ResourceBarrier(1, &b2);
}

void VoxelizeSceneTask::bounceCascade(CommandsData& commands, SceneVoxelsCascade& cascade)
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

void SceneVoxelsCascade::initialize(const std::string& n, ID3D12Device* device, GraphicsResources& resources)
{
	name = n;

	voxelSceneTexture.Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	voxelSceneTexture.SetName(name.c_str());
	resources.descriptors.createTextureView(voxelSceneTexture);
	resources.descriptors.createUAVView(voxelSceneTexture);

	auto prevName = name + "Previous";
	voxelPreviousSceneTexture.Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	voxelPreviousSceneTexture.SetName(prevName.c_str());
	resources.descriptors.createTextureView(voxelPreviousSceneTexture);

	voxelInfoBuffer.data = resources.shaderBuffers.CreateStructuredBuffer(DataElementSize * DataElementCount);
	voxelInfoBuffer.data->SetName(as_wstring(name + "DataBuffer").c_str());
	voxelInfoBuffer.view = resources.descriptors.createBufferView(voxelInfoBuffer.data.Get(), DataElementSize, DataElementCount);
}

void SceneVoxelsCascade::prepareForVoxelization(ID3D12GraphicsCommandList* commandList, TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, const GpuTexture3D& clearSceneTexture, ClearBufferComputeShader& clearBufferCS)
{
	//std::swap(currentBuffer, previousBuffer);
	clearBufferCS.dispatch(commandList, voxelInfoBuffer.data.Get(), DataElementSize * DataElementCount / sizeof(float));

	TextureTransitions<2> tr;
	tr.add(voxelScene, D3D12_RESOURCE_STATE_COPY_SOURCE);
	tr.add(prevVoxelScene, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.push(commandList);

	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), voxelSceneTexture.texture.Get());

	//fast clear
	voxelScene.Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyResource(voxelSceneTexture.texture.Get(), clearSceneTexture.texture.Get());

	auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(voxelInfoBuffer.data.Get());
	commandList->ResourceBarrier(1, &uavBarrier);

	tr.add(voxelScene, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	tr.add(prevVoxelScene, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	tr.push(commandList);
}

void SceneVoxelsCascade::clearAll(ID3D12GraphicsCommandList* commandList, const GpuTexture3D& clearSceneTexture, ClearBufferComputeShader& clearBufferCS)
{
	clearBufferCS.dispatch(commandList, voxelInfoBuffer.data.Get(), DataElementSize * DataElementCount / sizeof(float));

	TextureTransitions<2> tr;
	tr.add(voxelSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.add(voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.push(commandList);

	commandList->CopyResource(voxelSceneTexture.texture.Get(), clearSceneTexture.texture.Get());
	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), clearSceneTexture.texture.Get());

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(voxelInfoBuffer.data.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	tr.add(voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.add(voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.push(commandList);
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
