#include "VoxelizeSceneTask.h"
#include "SceneManager.h"
#include "MaterialResources.h"
#include "TextureResources.h"
#include "GenerateMipsComputeShader.h"
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
		commands.deinit();
		CloseHandle(eventBegin);
		CloseHandle(eventFinish);
	}

	instance = nullptr;
}

VoxelizeSceneTask& VoxelizeSceneTask::Get()
{
	return *instance;
}

void VoxelizeSceneTask::revoxelize()
{
	buildNearCounter = buildFarCounter = 0;
}

void VoxelizeSceneTask::clear(ID3D12GraphicsCommandList* c)
{
	revoxelize();

	nearVoxels.clearAll(c, clearSceneTexture, clearBufferCS);
	farVoxels.clearAll(c, clearSceneTexture, clearBufferCS);
}

void VoxelizeSceneTask::showVoxelsInfo(bool show)
{
	if (show != showVoxelsEnabled)
	{
		showVoxelsEnabled = show;

		if (!show)
			hideVoxels();
	}
}

void VoxelizeSceneTask::showVoxels(Camera& camera)
{
	auto debugVoxel = sceneMgr.getEntity("DebugVoxel");

	if (!debugVoxel)
	{
		debugVoxel = sceneMgr.createEntity("DebugVoxel");
		debugVoxel->material = provider.resources.materials.getMaterial("VisualizeVoxelTexture");
		debugVoxel->geometry.fromModel(*provider.resources.models.getLoadedModel("box.mesh", ResourceGroup::Core));
	}

	auto orientation = camera.getOrientation();
	auto pos = camera.getPosition() - orientation * Vector3(0, 5.f, 0) + camera.getCameraDirection() * 1.75;

	debugVoxel->setTransformation({ orientation, pos, Vector3(10, 10, 1) }, true);
}

void VoxelizeSceneTask::hideVoxels()
{
	auto debugVoxel = sceneMgr.getEntity("DebugVoxel");

	if (debugVoxel)
	{
		sceneMgr.removeEntity(debugVoxel);
	}
}

constexpr float NearClipDistance = 1;

constexpr float VoxelSize = 128.f;

constexpr DXGI_FORMAT VoxelFormat = DXGI_FORMAT_R8G8B8A8_UNORM; //DXGI_FORMAT_R16G16B16A16_FLOAT;

AsyncTasksInfo VoxelizeSceneTask::initialize(CompositorPass& pass)
{
	nearVoxels.initialize("SceneVoxel", provider.renderSystem.core.device, provider.resources);
	nearVoxels.settings.extends = 200;
	nearVoxels.idx = 0;
	farVoxels.initialize("SceneVoxelFar", provider.renderSystem.core.device, provider.resources);
	farVoxels.settings.extends = 600;
	farVoxels.idx = 1;

	clearSceneTexture.create3D(provider.renderSystem.core.device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	clearSceneTexture.setName("VoxelClear");

	cbuffer = provider.resources.shaderBuffers.CreateCbufferResource(sizeof(cbufferData), "SceneVoxelInfo");
	frameCbuffer = provider.resources.shaderBuffers.CreateCbufferResource(sizeof(Matrix), "FrameInfo");

	computeMips.init(*provider.renderSystem.core.device, "generateMipmaps", provider.resources.shaders);

	auto bouncesShader = provider.resources.shaders.getShader("voxelBouncesCS", ShaderTypeCompute, ShaderRef{"voxelBouncesCS.hlsl", "main", "cs_6_6"});
	bouncesCS.init(*provider.renderSystem.core.device, "voxelBouncesCS", *bouncesShader);

	auto clearBufferShader = provider.resources.shaders.getShader("clearBufferCS", ShaderTypeCompute, ShaderRef{ "clearBufferCS.hlsl", "main", "cs_6_6" });
	clearBufferCS.init(*provider.renderSystem.core.device, "clearBufferCS", *clearBufferShader);

	sceneQueue = sceneMgr.createQueue({ pass.target.texture->format }, MaterialTechnique::Voxelize);

	AsyncTasksInfo tasks;
	eventBegin = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	eventFinish = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	commands = provider.renderSystem.core.CreateCommandList(L"Voxelize");

	worker = std::thread([this, &pass]
		{
			while (WaitForSingleObject(eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
				auto diff = nearVoxels.update(ctx.camera->getPosition());
				if (diff.x != 0 || diff.y != 0 || diff.z != 0)
					buildNearCounter = 0;

				auto diffFar = farVoxels.update(ctx.camera->getPosition());
				if (diffFar.x != 0 || diffFar.y != 0 || diffFar.z != 0)
					buildFarCounter = 0;

				updateCBuffer(diff, diffFar, provider.renderSystem.core.frameIndex);

				constexpr UINT BouncingInterations = 1;

				auto marker = provider.renderSystem.core.StartCommandList(commands);
// 				if (buildNearCounter > BouncingInterations && buildFarCounter > BouncingInterations)
// 				{
// 					marker.close();
// 					SetEvent(eventFinish);
// 					continue;
// 				}

				static bool evenFrame = false;

				if (buildNearCounter < BouncingInterations)
				{
					buildNearCounter++;
					voxelizeChunk(commands, marker, pass.target, nearVoxels);
				}
				else if (evenFrame)
					bounceChunk(commands, marker, pass.target, nearVoxels);

				if (buildFarCounter < BouncingInterations)
				{
					buildFarCounter++;
					voxelizeChunk(commands, marker, pass.target, farVoxels);
				}
				else if (!evenFrame)
					bounceChunk(commands, marker, pass.target, farVoxels);

				evenFrame = !evenFrame;

				marker.close();
				SetEvent(eventFinish);
			}
		});

	tasks.emplace_back(eventFinish, commands);

	return tasks;
}

void VoxelizeSceneTask::run(RenderContext& renderCtx, CommandsData& syncCommands, CompositorPass&)
{
	if (showVoxelsEnabled)
		showVoxels(*renderCtx.camera);

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

void VoxelizeSceneTask::updateCBufferChunk(SceneVoxelChunkInfo& info, Vector3 diff, SceneVoxelsChunk& chunk)
{
	info.voxelSceneSize = chunk.settings.extends * 2;

	float sceneToVoxel = VoxelSize / info.voxelSceneSize;
	info.voxelDensity = sceneToVoxel;

	info.voxelOffset = chunk.settings.center - Vector3(chunk.settings.extends);

	info.diff = diff;
	info.texId = chunk.voxelSceneTexture.textureView.srvHeapIndex;
	info.texIdBounces = chunk.voxelPreviousSceneTexture.textureView.srvHeapIndex;
	info.resIdDataBuffer = chunk.dataBufferHeapIdx;

// 	float deltaTime = provider.params.timeDelta;
// 	deltaTime += deltaTime - deltaTime * deltaTime;
// 	deltaTime = max(deltaTime, 1.0f);
// 	info.lerpFactor = deltaTime;
}

void VoxelizeSceneTask::updateCBuffer(Vector3 diff, Vector3 diffFar, UINT frameIndex)
{
	updateCBufferChunk(cbufferData.nearVoxels, diff, nearVoxels);
	updateCBufferChunk(cbufferData.farVoxels, diffFar, farVoxels);

	auto& state = imgui::DebugWindow::state;

	cbufferData.steppingBounces = state.voxelSteppingBounces;
	cbufferData.steppingDiffuse = state.voxelSteppingDiffuse;
	cbufferData.middleConeRatioDistance = state.middleConeRatioDistance;
	cbufferData.sideConeRatioDistance = state.sideConeRatioDistance;
	cbufferData.voxelizeLighting = 0.0f;

	auto& cbufferResource = *cbuffer.data[frameIndex];
	memcpy(cbufferResource.Memory(), &cbufferData, sizeof(cbufferData));
}

void VoxelizeSceneTask::voxelizeChunk(CommandsData& commands, CommandsMarker& marker, PassTarget& viewportOutput, SceneVoxelsChunk& chunk)
{
	chunk.clearChunk(commands.commandList, clearSceneTexture, clearBufferCS);

	static RenderObjectsVisibilityData sceneInfo;
	auto& renderables = *sceneMgr.getRenderables(Order::Normal);

	viewportOutput.texture->PrepareAsTarget(commands.commandList, viewportOutput.previousState, true);

	chunk.prepareForVoxelization(commands.commandList);
	auto b = CD3DX12_RESOURCE_BARRIER::Transition(chunk.dataBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commands.commandList->ResourceBarrier(1, &b);

	auto center = chunk.settings.center;
	auto orthoHalfSize = chunk.settings.extends;

	Camera camera;
	camera.setOrthographicCamera(orthoHalfSize * 2, orthoHalfSize * 2, NearClipDistance, NearClipDistance + orthoHalfSize * 2);

	ShaderConstantsProvider constants(provider.params, sceneInfo, camera, *viewportOutput.texture);
	constants.voxelIdx = chunk.idx;
	constants.voxelBufferUAV = chunk.dataBuffer.Get();

	//from all 3 axes
	//Z
	camera.setPosition({ center.x, center.y, center.z - orthoHalfSize - NearClipDistance });
	camera.setDirection({ 0, 0, 1 });
	camera.updateMatrix();
	renderables.updateVisibility(camera, sceneInfo);
	sceneQueue->renderObjects(constants, commands.commandList);
	//X
	camera.setPosition({ center.x - orthoHalfSize - NearClipDistance, center.y, center.z });
	camera.setDirection({ 1, 0, 0 });
	camera.updateMatrix();
	//renderables.updateVisibility(camera, sceneInfo);
	sceneQueue->renderObjects(constants, commands.commandList);
	//Y
	camera.setPosition({ center.x, center.y - orthoHalfSize - NearClipDistance, center.z });
	camera.pitch(XM_PIDIV2);
	camera.updateMatrix();
	//renderables.updateVisibility(camera, sceneInfo);
	sceneQueue->renderObjects(constants, commands.commandList);

	marker.move("VoxelMipMaps");
	computeMips.dispatch(commands.commandList, chunk.voxelSceneTexture);

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(chunk.dataBuffer.Get());
		commands.commandList->ResourceBarrier(1, &uavBarrier);
	}

	auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(chunk.dataBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commands.commandList->ResourceBarrier(1, &b2);

	chunk.prepareForReading(commands.commandList);
}

void VoxelizeSceneTask::bounceChunk(CommandsData& commands, CommandsMarker& marker, PassTarget& viewportOutput, SceneVoxelsChunk& chunk)
{
	marker.move("VoxelBounces");
	TextureResource::TransitionState(commands.commandList, chunk.voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
	TextureResource::TransitionState(commands.commandList, chunk.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);

	commands.commandList->CopyResource(chunk.voxelPreviousSceneTexture.texture.Get(), chunk.voxelSceneTexture.texture.Get());

	TextureResource::TransitionState(commands.commandList, chunk.voxelSceneTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	TextureResource::TransitionState(commands.commandList, chunk.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	XM_ALIGNED_STRUCT(16)
	{
		Vector3 cameraPosition;
		float padding;
		Vector3 VoxelsOffset;
		float padding2;
		Vector3 VoxelsSceneSize;
	}
	data = { ctx.camera->getPosition(), {}, chunk.settings.center - Vector3(chunk.settings.extends), {}, Vector3(chunk.settings.extends * 2) };

	bouncesCS.dispatch(commands.commandList, std::span{ (float*) & data, 11 }, chunk.voxelSceneTexture.textureView, chunk.voxelPreviousSceneTexture.textureView, chunk.dataBuffer->GetGPUVirtualAddress());

	marker.move("VoxelMipMaps");
	computeMips.dispatch(commands.commandList, chunk.voxelSceneTexture);
}

void SceneVoxelsChunk::initialize(const std::string& n, ID3D12Device* device, GraphicsResources& resources)
{
	name = n;

	voxelSceneTexture.create3D(device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	voxelSceneTexture.setName(name.c_str());
	resources.descriptors.createUAVView(voxelSceneTexture);
	resources.textures.setNamedUAV(name, voxelSceneTexture.uav.front());
	resources.descriptors.createTextureView(voxelSceneTexture);
	resources.textures.setNamedTexture(name, voxelSceneTexture.textureView);

	auto bounceName = name + "Bounces";
	voxelPreviousSceneTexture.create3D(device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	voxelPreviousSceneTexture.setName(bounceName.c_str());
	resources.descriptors.createTextureView(voxelPreviousSceneTexture);
	resources.textures.setNamedTexture(bounceName, voxelPreviousSceneTexture.textureView);

	dataBuffer = resources.shaderBuffers.CreateStructuredBuffer(DataElementSize * DataElementCount);
	dataBuffer->SetName(as_wstring(name + "DataBuffer").c_str());
	dataBufferHeapIdx = resources.descriptors.createBufferView(dataBuffer.Get(), DataElementSize, DataElementCount);
}

void SceneVoxelsChunk::clearChunk(ID3D12GraphicsCommandList* commandList, const TextureResource& clearSceneTexture, ClearBufferComputeShader& clearBufferCS)
{
	clearBufferCS.dispatch(commandList, dataBuffer.Get(), DataElementSize * DataElementCount / sizeof(float));

	TextureResource::TransitionState(commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
	TextureResource::TransitionState(commandList, voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);

	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), voxelSceneTexture.texture.Get());

	//fast clear
	TextureResource::TransitionState(commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyResource(voxelSceneTexture.texture.Get(), clearSceneTexture.texture.Get());
//	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), clearSceneTexture.texture.Get());

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(dataBuffer.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}
}

void SceneVoxelsChunk::clearAll(ID3D12GraphicsCommandList* commandList, const TextureResource& clearSceneTexture, ClearBufferComputeShader& clearBufferCS)
{
	clearBufferCS.dispatch(commandList, dataBuffer.Get(), DataElementSize * DataElementCount / sizeof(float));

	TextureResource::TransitionState(commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);
	TextureResource::TransitionState(commandList, voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyResource(voxelSceneTexture.texture.Get(), clearSceneTexture.texture.Get());
	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), clearSceneTexture.texture.Get());

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(dataBuffer.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}
}

void SceneVoxelsChunk::prepareForVoxelization(ID3D12GraphicsCommandList* commandList)
{
	TextureResource::TransitionState(commandList, voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	TextureResource::TransitionState(commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void SceneVoxelsChunk::prepareForReading(ID3D12GraphicsCommandList* commandList)
{
	TextureResource::TransitionState(commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

static Vector3 SnapToGrid(Vector3 position, float gridStep)
{
	position.x = round(position.x / gridStep) * gridStep;
	position.y = round(position.y / gridStep) * gridStep;
	position.z = round(position.z / gridStep) * gridStep;
	return position;
}

Vector3 SceneVoxelsChunk::update(const Vector3& cameraPosition)
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
	commandList->SetComputeRootDescriptorTable(2, source.srvHandles);
	commandList->SetComputeRootDescriptorTable(3, target.uavHandles);

	const UINT threadSize = 4;
	const auto voxelSize = UINT(VoxelSize);
	const uint32_t groupSize = (voxelSize + threadSize - 1) / threadSize;

	commandList->Dispatch(groupSize, groupSize, groupSize);
}
