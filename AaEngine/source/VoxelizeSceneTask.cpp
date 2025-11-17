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

void VoxelizeSceneTask::clear()
{
	reset = true;
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

	for (auto& v : voxelCascades)
	{
		v.clearAll(c, clearSceneTexture, clearBufferCS);
	}

	reset = false;
}

void VoxelizeSceneTask::showVoxelsInfo(bool show)
{
	if (show != showVoxelsEnabled)
	{
		showVoxelsEnabled = show;

		if (!show)
			hideVoxels();
		else
			showVoxels();
	}
}

void VoxelizeSceneTask::showVoxelsUpdate(Camera& camera)
{
	if (!showVoxelsEnabled)
		return;

	auto debugVoxel = sceneMgr.getEntity("DebugVoxel");
	if (!debugVoxel)
		debugVoxel = showVoxels();

	auto orientation = camera.getOrientation();
	auto pos = camera.getPosition() - orientation * Vector3(0, 5.f, 0) + camera.getCameraDirection() * 1.75;

	debugVoxel->setTransformation({ orientation, pos, Vector3(10, 10, 1) }, true);
}

SceneEntity* VoxelizeSceneTask::showVoxels()
{
	auto debugVoxel = sceneMgr.createEntity("DebugVoxel");
	debugVoxel->material = provider.resources.materials.getMaterial("VisualizeVoxelTexture");
	debugVoxel->geometry.fromModel(*provider.resources.models.getLoadedModel("box.mesh", ResourceGroup::Core));

	return debugVoxel;
}

void VoxelizeSceneTask::hideVoxels()
{
	if (auto debugVoxel = sceneMgr.getEntity("DebugVoxel"))
	{
		sceneMgr.removeEntity(debugVoxel);
	}
}

constexpr float VoxelSize = 128.f;

constexpr DXGI_FORMAT VoxelFormat = DXGI_FORMAT_R8G8B8A8_UNORM; //DXGI_FORMAT_R16G16B16A16_FLOAT;

AsyncTasksInfo VoxelizeSceneTask::initialize(CompositorPass& pass)
{
	constexpr float CascadeDistances[CascadesCount] = { 200, 600, 2000, 6000 };
	for (UINT i = 0; auto& voxels : voxelCascades)
	{
		voxels.initialize("VoxelCascade" + std::to_string(i), provider.renderSystem.core.device, provider.resources);
		voxels.settings.extends = CascadeDistances[i];
		voxels.idx = i++;
	}

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
	commands = provider.renderSystem.core.CreateCommandList(L"Voxelize", PixColor::Voxelize);

	worker = std::thread([this, &pass]
		{
			while (WaitForSingleObject(eventBegin, INFINITE) == WAIT_OBJECT_0 && running)
			{
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

				auto marker = provider.renderSystem.core.StartCommandList(commands);

				static bool evenFrame = false;
				constexpr UINT VoxelizeInterations = 1;

				for (UINT i = 0; auto & voxel : voxelCascades)
				{
					if (buildCounter[i] < VoxelizeInterations)
					{
						buildCounter[i]++;
						voxelizeCascade(commands, marker, pass.target, voxel);
					}
					else if (evenFrame == (i % 2))
						bounceCascade(commands, marker, pass.target, voxel);

					i++;
				}

				evenFrame = !evenFrame;

				marker.close();
				SetEvent(eventFinish);
			}
		});

	tasks.emplace_back(eventFinish, commands);

	revoxelize();

	return tasks;
}

void VoxelizeSceneTask::run(RenderContext& renderCtx, CommandsData& syncCommands, CompositorPass&)
{
	showVoxelsUpdate(*renderCtx.camera);

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

void VoxelizeSceneTask::updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 diff, SceneVoxelsCascade& cascade)
{
	info.voxelSceneSize = cascade.settings.extends * 2;

	float sceneToVoxel = VoxelSize / info.voxelSceneSize;
	info.voxelDensity = sceneToVoxel;

	info.voxelOffset = cascade.settings.center - Vector3(cascade.settings.extends);

	info.diff = diff;
	info.texId = cascade.voxelSceneTexture.textureView.srvHeapIndex;
	info.texIdBounces = cascade.voxelPreviousSceneTexture.textureView.srvHeapIndex;
	info.resIdDataBuffer = cascade.dataBufferHeapIdx;

// 	float deltaTime = provider.params.timeDelta;
// 	deltaTime += deltaTime - deltaTime * deltaTime;
// 	deltaTime = max(deltaTime, 1.0f);
// 	info.lerpFactor = deltaTime;
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

void VoxelizeSceneTask::voxelizeCascade(CommandsData& commands, CommandsMarker& marker, PassTarget& viewportOutput, SceneVoxelsCascade& cascade)
{
	if (reset)
		clear(commands.commandList);

	viewportOutput.texture->PrepareAsTarget(commands.commandList, viewportOutput.previousState, true);

	cascade.prepareForVoxelization(commands.commandList, clearSceneTexture, clearBufferCS);
	auto b = CD3DX12_RESOURCE_BARRIER::Transition(cascade.dataBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commands.commandList->ResourceBarrier(1, &b);

	auto center = cascade.settings.center;
	auto orthoHalfSize = cascade.settings.extends;

	constexpr float NearClipDistance = 1;

	Camera camera;
	camera.setOrthographicCamera(orthoHalfSize * 2, orthoHalfSize * 2, NearClipDistance, NearClipDistance + orthoHalfSize * 2);

	static RenderObjectsVisibilityData sceneInfo;
	auto& renderables = *sceneMgr.getRenderables(Order::Normal);

	ShaderConstantsProvider constants(provider.params, sceneInfo, camera, *viewportOutput.texture);

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

	marker.move("VoxelMipMaps", PixColor::Voxelize);
	computeMips.dispatch(commands.commandList, cascade.voxelSceneTexture);

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(cascade.dataBuffer.Get());
		commands.commandList->ResourceBarrier(1, &uavBarrier);
	}

	auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(cascade.dataBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commands.commandList->ResourceBarrier(1, &b2);

	cascade.prepareForReading(commands.commandList);
}

void VoxelizeSceneTask::bounceCascade(CommandsData& commands, CommandsMarker& marker, PassTarget& viewportOutput, SceneVoxelsCascade& cascade)
{
	marker.move("VoxelBounces", PixColor::Voxelize);
	TextureResource::TransitionState(commands.commandList, cascade.voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
	TextureResource::TransitionState(commands.commandList, cascade.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);

	commands.commandList->CopyResource(cascade.voxelPreviousSceneTexture.texture.Get(), cascade.voxelSceneTexture.texture.Get());

	TextureResource::TransitionState(commands.commandList, cascade.voxelSceneTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	TextureResource::TransitionState(commands.commandList, cascade.voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	XM_ALIGNED_STRUCT(16)
	{
		Vector3 cameraPosition;
		float padding;
		Vector3 VoxelsOffset;
		float padding2;
		Vector3 VoxelsSceneSize;
	}
	data = { ctx.camera->getPosition(), {}, cascade.settings.center - Vector3(cascade.settings.extends), {}, Vector3(cascade.settings.extends * 2) };

	bouncesCS.dispatch(commands.commandList, std::span{ (float*) & data, 11 }, cascade.voxelSceneTexture.textureView, cascade.voxelPreviousSceneTexture.textureView, cascade.dataBuffer->GetGPUVirtualAddress());

	marker.move("VoxelMipMaps", PixColor::Voxelize);
	computeMips.dispatch(commands.commandList, cascade.voxelSceneTexture);

	cascade.prepareForReading(commands.commandList);
}

void SceneVoxelsCascade::initialize(const std::string& n, ID3D12Device* device, GraphicsResources& resources)
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

void SceneVoxelsCascade::prepareForVoxelization(ID3D12GraphicsCommandList* commandList, const TextureResource& clearSceneTexture, ClearBufferComputeShader& clearBufferCS)
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

	TextureResource::TransitionState(commandList, voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	TextureResource::TransitionState(commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void SceneVoxelsCascade::clearAll(ID3D12GraphicsCommandList* commandList, const TextureResource& clearSceneTexture, ClearBufferComputeShader& clearBufferCS)
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

void SceneVoxelsCascade::prepareForReading(ID3D12GraphicsCommandList* commandList)
{
	TextureResource::TransitionState(commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
	commandList->SetComputeRootDescriptorTable(2, source.srvHandles);
	commandList->SetComputeRootDescriptorTable(3, target.uavHandles);

	const UINT threadSize = 4;
	const auto voxelSize = UINT(VoxelSize);
	const uint32_t groupSize = (voxelSize + threadSize - 1) / threadSize;

	commandList->Dispatch(groupSize, groupSize, groupSize);
}
