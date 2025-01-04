#include "VoxelizeSceneTask.h"
#include "SceneManager.h"
#include "MaterialResources.h"
#include "TextureResources.h"
#include "GenerateMipsComputeShader.h"
#include "DebugWindow.h"
#include <format>

VoxelizeSceneTask* instance = nullptr;

VoxelizeSceneTask::VoxelizeSceneTask(RenderProvider p, SceneManager& s, AaShadowMap& shadows) : shadowMaps(shadows), CompositorTask(p, s)
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

UINT buildNearCounter = 0;
UINT buildFarCounter = 0;

void VoxelizeSceneTask::reset()
{
	buildNearCounter = buildFarCounter = 0;
}

const float NearClipDistance = 1;

const float VoxelSize = 128.f;

const DXGI_FORMAT VoxelFormat = DXGI_FORMAT_R8G8B8A8_UNORM; //DXGI_FORMAT_R16G16B16A16_FLOAT;

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

	computeMips.init(*provider.renderSystem.core.device, "generateMipmaps", provider.resources.shaders);

	sceneQueue = sceneMgr.createQueue({ pass.target.texture->format }, MaterialTechnique::Voxelize);

	AsyncTasksInfo tasks;
	eventBegin = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
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

				constexpr UINT BouncingInterations = 30;

				auto marker = provider.renderSystem.core.StartCommandList(commands);
				if (buildNearCounter > BouncingInterations && buildFarCounter > BouncingInterations)
				{
					marker.close();
					SetEvent(eventFinish);
					continue;
				}

				static bool evenFrame = false;

				if (buildNearCounter < BouncingInterations)
				{
					buildNearCounter++;
					voxelizeChunk(commands, marker, pass.target, nearVoxels);
				}
				if (buildFarCounter < BouncingInterations)
				{
					buildFarCounter++;
					voxelizeChunk(commands, marker, pass.target, farVoxels);
				}

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
	chunk.clear(commands.commandList, clearSceneTexture);

	static RenderObjectsVisibilityData sceneInfo;
	auto& renderables = *sceneMgr.getRenderables(Order::Normal);

	viewportOutput.texture->PrepareAsTarget(commands.commandList, viewportOutput.previousState, true);

	chunk.prepareForVoxelization(commands.commandList);

	auto center = chunk.settings.center;
	auto orthoHalfSize = chunk.settings.extends;

	Camera camera;
	camera.setOrthographicCamera(orthoHalfSize * 2, orthoHalfSize * 2, NearClipDistance, NearClipDistance + orthoHalfSize * 2);

	ShaderConstantsProvider constants(provider.params, sceneInfo, camera, *viewportOutput.texture);
	constants.voxelIdx = chunk.idx;

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

	chunk.prepareForReading(commands.commandList);
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

	voxelPreviousSceneTexture.create3D(device, VoxelSize, VoxelSize, VoxelSize, VoxelFormat);
	voxelPreviousSceneTexture.setName("SceneVoxelBounces");
	resources.descriptors.createTextureView(voxelPreviousSceneTexture);
	resources.textures.setNamedTexture("SceneVoxelBounces", voxelPreviousSceneTexture.textureView);
}

void SceneVoxelsChunk::clear(ID3D12GraphicsCommandList* commandList, const TextureResource& clearSceneTexture)
{
	TextureResource::TransitionState(commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
	TextureResource::TransitionState(commandList, voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);

	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), voxelSceneTexture.texture.Get());

	//fast clear
	TextureResource::TransitionState(commandList, voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyResource(voxelSceneTexture.texture.Get(), clearSceneTexture.texture.Get());
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
