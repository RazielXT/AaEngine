#include "WaterSim.h"
#include "TextureUtils.h"
#include "SceneManager.h"
#include "GridMesh.h"
#include <format>

WaterSim::WaterSim()
{
}

WaterSim::~WaterSim()
{
}

const UINT TextureSize = 1024;
const UINT WaterModelSize = 33;// TextureSize - WaterModelChucks + 1;

GridInstanceMesh waterGridMesh;
GridLODSystem waterGridTiles;

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
			gradient[y * TextureSize + x] = t * 20;
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

	waterHeightMeshTexture.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	waterHeightMeshTexture.SetName("WaterHeightMeshTexture");
	resources.descriptors.createUAVView(waterHeightMeshTexture);
	resources.descriptors.createTextureView(waterHeightMeshTexture);

	waterMaterial = resources.materials.getMaterial("WaterLake", batch); 

	auto csShader = resources.shaders.getShader("momentumComputeShader", ShaderType::Compute, ShaderRef{ "waterSim/frenzySimMomentumCS.hlsl", "CS_Momentum", "cs_6_6" });
	momentumComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("continuityComputeShader", ShaderType::Compute, ShaderRef{ "waterSim/frenzySimContinuityCS.hlsl", "CS_Continuity", "cs_6_6" });
	continuityComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("watermapToTextureCS", ShaderType::Compute, ShaderRef{ "utils/watermapToTextureCS.hlsl", "CSMain", "cs_6_6" });
	waterToTextureCS.init(*renderSystem.core.device, *csShader);

	generateNormalMipsCS.init(*renderSystem.core.device, "generateNormalMipmaps4x", resources.shaders);
}

void WaterSim::initializeTarget(const GpuTexture2D& texture, SceneManager& sceneMgr, Vector2 size, Vector3 center)
{
	terrainHeight = &texture;

	size = { 8000.f, 8000.f };
	center = { 0, -100, 0 };
	waterGridTiles.Initialize(size, center, 7);

	auto e = sceneMgr.createEntity("WaterSim", Order::Transparent);
	waterGridMesh.create(64);
	waterGridMesh.entity = e;

	e->geometry.fromInstancedModel(waterModel, 0, waterGridMesh.gpuBuffer.data[0].GpuAddress());
	e->setBoundingBox(BoundingBox({}, { size.x, 150.f, size.y }));
	e->material = waterMaterial;
	e->Material().setParam("TexIdHeightmap", waterHeightMeshTexture.view.srvHeapIndex);
	e->Material().setParam("TexIdMeshNormal", waterNormalTexture.view.srvHeapIndex);
	auto GridHeightWidth = Vector2(size.x, size.x / 50.f);
	e->Material().setParam("GridHeightWidth", &GridHeightWidth, sizeof(GridHeightWidth));
	e->setPosition(center);
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

		lastCameraPos = cameraPos;
	}
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

	waterHeight[waterResultIdx].Transition(computeList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	waterToTextureCS.dispatch(computeList, waterHeight[waterResultIdx].view.srvHeapIndex, terrainHeight->view.srvHeapIndex, TextureSize, TextureSize, waterNormalTexture.view.uavHandle, waterHeightMeshTexture.view.uavHandle, lastCameraPos);
	generateNormalMipsCS.dispatch(computeList, TextureSize, waterNormalTextureMips);
	waterHeight[waterResultIdx].Transition(computeList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

void WaterSim::prepareForRendering(ID3D12GraphicsCommandList* commandList)
{
	waterNormalTexture.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	waterHeightMeshTexture.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void WaterSim::prepareAfterRendering(ID3D12GraphicsCommandList* commandList)
{
	waterNormalTexture.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	waterHeightMeshTexture.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}
