#include "RenderObject/WaterSim/WaterSim.h"
#include "RenderCore/TextureUtils.h"
#include "Scene/RenderWorld.h"
#include <format>

WaterSim::WaterSim()
{
}

WaterSim::~WaterSim()
{
}

const UINT TextureSize = 1024;
const UINT WaterModelSize = 33;

void WaterSim::initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	// Generate gradient data
	std::vector<float> gradient(TextureSize * TextureSize);
	for (UINT y = 0; y < TextureSize; ++y)
	{
		for (UINT x = 0; x < TextureSize; ++x)
		{
			float t = static_cast<float>(TextureSize - x) / (TextureSize - 1);
			//t += static_cast<float>(TextureSize - y) / (TextureSize - 1);
			gradient[y * TextureSize + x] = 20;
		}
	}
	srcWater = TextureUtils::CreateUploadTexture(renderSystem.core.device, batch, gradient.data(), TextureSize, TextureSize, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	waterModel.CreateIndexBufferGrid(renderSystem.core.device, &batch, WaterModelSize);
	waterModel.bbox.Extents = { 150, 150, 150 };

	for (size_t i = 0; i < std::size(waterHeight); i++)
	{
		auto& w = waterHeight[i];
		w.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		w.SetName("WaterSimWaterHeight" + std::to_string(i));
		resources.descriptors.createUAVView(w);
		resources.descriptors.createTextureView(w);
	}
	for (size_t i = 0; i < std::size(waterVelocity); i++)
	{
		auto& w = waterVelocity[i];
		w.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		w.SetName("WaterSimWaterVelocity" + std::to_string(i));
		resources.descriptors.createUAVView(w);
		resources.descriptors.createTextureView(w);
	}

	waterNormalTexture.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R8G8_SNORM, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, { .mipLevels = 5 });
	waterNormalTexture.SetName("WaterNormalTexture");
	resources.descriptors.createUAVView(waterNormalTexture);
	resources.descriptors.createTextureView(waterNormalTexture);
	waterNormalTextureMips = resources.descriptors.createUAVMips(waterNormalTexture);

	waterFlowTexture.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	waterFlowTexture.SetName("WaterFlowTexture");
	resources.descriptors.createUAVView(waterFlowTexture);
	resources.descriptors.createTextureView(waterFlowTexture);
	waterFlowTextureMips = resources.descriptors.createUAVMips(waterFlowTexture);

	waterHeightMeshTexture.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	waterHeightMeshTexture.SetName("WaterHeightMeshTexture");
	resources.descriptors.createUAVView(waterHeightMeshTexture);
	resources.descriptors.createTextureView(waterHeightMeshTexture);

	waterMaterial = resources.materials.getMaterial("WaterLake", batch); 
	sceneRenderingStateBuffer = resources.shaderBuffers.CreateStructuredBuffer(sizeof(float) * 4, "SceneRenderingState");

	auto csShader = resources.shaders.getShader("cameraWaterStateCS", ShaderType::Compute, ShaderRef{ "waterSim/cameraWaterStateCS.hlsl", "CSMain", "cs_6_6" });
	cameraWaterStateCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("momentumComputeShader", ShaderType::Compute, ShaderRef{ "waterSim/frenzySimMomentumCS.hlsl", "CS_Momentum", "cs_6_6" });
	momentumComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("continuityComputeShader", ShaderType::Compute, ShaderRef{ "waterSim/frenzySimContinuityCS.hlsl", "CS_Continuity", "cs_6_6" });
	continuityComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("watermapToTextureCS", ShaderType::Compute, ShaderRef{ "waterSim/watermapToTextureCS.hlsl", "CSMain", "cs_6_6" });
	waterToTextureCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("waterAdjustCS", ShaderType::Compute, ShaderRef{ "waterSim/waterAdjustCS.hlsl", "CSMain", "cs_6_6" });
	waterAdjustCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("waterSmoothCS", ShaderType::Compute, ShaderRef{ "waterSim/waterSmoothCS.hlsl", "CSMain", "cs_6_6" });
	waterSmoothCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("waterInteractionCS", ShaderType::Compute, ShaderRef{ "waterSim/waterInteractionCS.hlsl", "CSMain", "cs_6_6" });
	interaction.initializeGpuResources(renderSystem, *csShader);

	generateXYMips4xCS.init(*renderSystem.core.device, "generateXYMips4xCS", resources.shaders);
}

void WaterSim::initializeTarget(const GpuTexture2D& texture, RenderWorld& renderWorld, Vector2 size, Vector3 center)
{
	terrainHeight = &texture;

	worldSize = size * (TextureSize / (TextureSize - 64.f));

	worldCenter = center;
	auto worldOffset = worldSize * (32.f / TextureSize);
	worldCenter.x -= worldOffset.x;
	worldCenter.z -= worldOffset.y;

	waterGridTiles.Initialize(worldSize, worldCenter, 10);

	auto e = renderWorld.createEntity(EntityCreateProperties{ .order = Order::Transparent });
	waterGridMesh.create(waterGridTiles.MaxInstances);
	waterGridMesh.entity = e;

	e->geometry.fromInstancedModel(waterModel, 0, waterGridMesh.gpuBuffer.data[0].GpuAddress());
	e->setBoundingBox(BoundingBox({ worldSize.x * 0.5f, size.x * 0.5f, worldSize.y * 0.5f }, { worldSize.x * 0.5f, size.x * 0.5f, worldSize.y * 0.5f }));
	e->material = waterMaterial;
	e->Material().setParam("TexIdHeightmap", waterHeightMeshTexture.view.srvHeapIndex);
	e->Material().setParam("TexIdFlowmap", waterFlowTexture.view.srvHeapIndex);
	e->Material().setParam("TexIdMeshNormal", waterNormalTexture.view.srvHeapIndex);
	e->Material().setParam("GridHeightWidth", Vector2(size.x, worldSize.x));
	e->setPosition(worldCenter);
}

static bool first = true;
Vector3 lastCameraPos{};

void WaterSim::update(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx, Vector3 cameraPos)
{
	CommandsMarker marker(commandList, "WaterSimGrid", PixColor::Crimson);

	const UINT prevFrameIdx = (frameIdx == 0) ? (FrameCount - 1) : (frameIdx - 1);

	if (first)
	{
		TextureUtils::CopyTextureBuffer(commandList, srcWater.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterHeight[prevFrameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		TextureUtils::CopyTextureBuffer(commandList, srcWater.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterHeight[frameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	if (updateLod)
	{
		waterGridTiles.BuildLOD(cameraPos, {});
		waterGridMesh.update((UINT)waterGridTiles.m_renderList.size(), waterGridTiles.m_renderList.data(), (UINT)waterGridTiles.m_renderList.size() * sizeof(TileData), frameIdx);
	}

	lastCameraPos = cameraPos;

	interaction.buildRequest();
}

void WaterSim::updateCompute(RenderSystem& renderSystem, ID3D12GraphicsCommandList* computeList, float dt, UINT)
{
	if (!updateWater || !terrainHeight)
		return;

	if (first)
	{
		first = false;
		return;
	}

	const float DtPerUpdate = 1 / 60.f;
	static float updateDt = DtPerUpdate;

	updateDt += dt;
	if (updateDt < DtPerUpdate)
		return;

	while (updateDt >= DtPerUpdate)
		updateDt -= DtPerUpdate;

	CommandsMarker marker(computeList, "WaterSim", PixColor::LightBlue);

	const UINT WaterComputeIterations = 2;
	static UINT waterCurrentIdx = 0;
	const UINT waterPrevIdx = (waterCurrentIdx == 0) ? (WaterComputeIterations - 1) : (waterCurrentIdx - 1);
	const UINT BuffersCount = 2;
	const UINT waterResultIdx = (waterPrevIdx + WaterComputeIterations) % BuffersCount;

	// Apply pending water adjustments
	{
		std::lock_guard lock(adjustmentsMutex);
		for (const auto& adj : pendingAdjustments)
		{
			float texelX = (adj.worldPosition.x - worldCenter.x) / worldSize.x;
			float texelY = (adj.worldPosition.z - worldCenter.z) / worldSize.y;
			float radiusTexels = adj.radius / worldSize.x * TextureSize;

			waterAdjustCS.dispatch(computeList, TextureSize, { texelX * TextureSize, texelY * TextureSize }, radiusTexels, adj.heightDelta * dt, waterHeight[waterPrevIdx].view.uavHeapIndex);
		}
		if (!pendingAdjustments.empty())
		{
			auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[waterPrevIdx].texture.Get());
			D3D12_RESOURCE_BARRIER barriers[] = { uavb };
			computeList->ResourceBarrier(1, barriers);
			pendingAdjustments.clear();
		}
	}

	WaterSimTextures t =
	{
		terrainHeight->view.srvHeapIndex,
		waterHeight[waterPrevIdx].view.uavHeapIndex,
		waterVelocity[waterPrevIdx].view.uavHeapIndex,
		waterHeight[waterCurrentIdx].view.uavHeapIndex,
		waterVelocity[waterCurrentIdx].view.uavHeapIndex
	};
	WaterSimTextures t2 =
	{
		terrainHeight->view.srvHeapIndex,
		waterHeight[waterCurrentIdx].view.uavHeapIndex,
		waterVelocity[waterCurrentIdx].view.uavHeapIndex,
		waterHeight[waterPrevIdx].view.uavHeapIndex,
		waterVelocity[waterPrevIdx].view.uavHeapIndex
	};

	auto safeTime = DtPerUpdate * 0.25f;
	const float gridSpacing = 0.1f;

	{
		momentumComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterVelocity[waterCurrentIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);

		continuityComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t);

		uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[waterCurrentIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);
	}
	{
		momentumComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t2);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterVelocity[waterPrevIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);

		continuityComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t2);

		uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[waterPrevIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);
	}
// 	{
// 		momentumComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t);
// 
// 		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterVelocity[waterCurrentIdx].texture.Get());
// 		computeList->ResourceBarrier(1, &uavb);
// 
// 		continuityComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t);
// 
// 		uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[waterCurrentIdx].texture.Get());
// 		computeList->ResourceBarrier(1, &uavb);
// 	}

	// Smooth sharp water height features (volume-conserving edge-flux)
	const UINT smoothOutputIdx = 1 - waterResultIdx;
	{
		waterSmoothCS.dispatch(computeList, TextureSize, 0.4f,
			terrainHeight->view.srvHeapIndex,
			waterHeight[waterResultIdx].view.uavHeapIndex,
			waterHeight[smoothOutputIdx].view.uavHeapIndex);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[smoothOutputIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);
	}

	waterHeight[smoothOutputIdx].Transition(computeList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	waterVelocity[waterResultIdx].Transition(computeList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	waterToTextureCS.dispatch(computeList, waterHeight[smoothOutputIdx].view.srvHeapIndex, terrainHeight->view.srvHeapIndex, waterVelocity[waterResultIdx].view.srvHeapIndex, TextureSize, TextureSize, waterNormalTexture.view.uavHandle, waterHeightMeshTexture.view.uavHandle, waterFlowTexture.view.uavHandle, lastCameraPos);
	generateXYMips4xCS.dispatch(computeList, TextureSize, waterNormalTextureMips);
	generateXYMips4xCS.dispatch(computeList, TextureSize, waterFlowTextureMips);

	interaction.dispatchCompute(computeList, waterHeight[smoothOutputIdx], waterVelocity[waterResultIdx], worldCenter, worldSize);

	if (sceneRenderingStateBuffer)
	{
		auto toUav = CD3DX12_RESOURCE_BARRIER::Transition(
			sceneRenderingStateBuffer.Get(),
			sceneRenderingStateNeedsReset ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		computeList->ResourceBarrier(1, &toUav);

		CameraWaterStateCS::DispatchParams params;
		params.waterHeightSrvIndex = waterHeight[smoothOutputIdx].view.srvHeapIndex;
		params.dt = DtPerUpdate;
		params.worldCenter = { worldCenter.x, worldCenter.z };
		params.worldSize = worldSize;
		params.cameraPosition = lastCameraPos;
		params.waterHeightScale = 1000.0f;
		params.waterHeightStart = -500.0f;
		params.dryingSpeed = 1.1f;
		params.resetState = sceneRenderingStateNeedsReset ? 1 : 0;
		cameraWaterStateCS.dispatch(computeList, params, sceneRenderingStateBuffer.Get());

		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(sceneRenderingStateBuffer.Get());
		computeList->ResourceBarrier(1, &uavBarrier);

		auto toSrv = CD3DX12_RESOURCE_BARRIER::Transition(
			sceneRenderingStateBuffer.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		computeList->ResourceBarrier(1, &toSrv);

		sceneRenderingStateNeedsReset = false;
	}

	waterVelocity[waterResultIdx].Transition(computeList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	waterHeight[smoothOutputIdx].Transition(computeList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	waterCurrentIdx = (waterCurrentIdx + 1) % BuffersCount;
}

void WaterSim::clear()
{
}

void WaterSim::enableLodUpdating(bool enable)
{
	updateLod = enable;
}

void WaterSim::enableWaterUpdating(bool enable)
{
	updateWater = enable;
}

void WaterSim::addAdjustment(const WaterAdjustment& adjustment)
{
	std::lock_guard lock(adjustmentsMutex);
	pendingAdjustments.push_back(adjustment);
}

void WaterSim::prepareForRendering(ID3D12GraphicsCommandList* commandList)
{
	waterNormalTexture.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	waterFlowTexture.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	waterHeightMeshTexture.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void WaterSim::prepareAfterRendering(ID3D12GraphicsCommandList* commandList)
{
	waterNormalTexture.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	waterFlowTexture.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	waterHeightMeshTexture.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void WaterSim::submitInteractionQueries(const std::vector<InteractionQuery>& queries)
{
	interaction.submitQueries(queries);
}

bool WaterSim::consumeInteractionResults(std::vector<InteractionResult>& outResults)
{
	return interaction.consumeResults(outResults);
}
