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
const UINT TerrainModelSize = 33;// TextureSize - WaterModelChucks + 1;

GridInstanceMesh terrainGridMesh[5][5];
GridInstanceMesh waterGridMesh;
GridLODSystem terrainGridTiles;

void WaterSim::initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, SceneManager& sceneMgr)
{
	terrainTexture = resources.textures.loadFile(*renderSystem.core.device, batch, "Mountain Range Height Map PNG.png");
	resources.descriptors.createTextureView(*terrainTexture);
	terrainTexture2 = resources.textures.loadFile(*renderSystem.core.device, batch, "Caustics_tex.png");
	resources.descriptors.createTextureView(*terrainTexture2);

	// Generate gradient data
	std::vector<float> gradient(TextureSize* TextureSize);
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
	
	std::vector<XMFLOAT2> velocityData(TextureSize * TextureSize);
	for (UINT y = 0; y < TextureSize; ++y)
	{
		for (UINT x = 0; x < TextureSize; ++x)
		{
			velocityData[y * TextureSize + x] = { 1, 0 };
		}
	}
	srcVelocity = TextureUtils::CreateUploadTexture(renderSystem.core.device, batch, velocityData.data(), TextureSize, TextureSize, DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	terrainModel.CreateIndexBufferGrid(renderSystem.core.device, &batch, TerrainModelSize);
	terrainModel.bbox.Extents = { 150, 150, 150 };

	waterModel.CreateIndexBufferGrid(renderSystem.core.device, &batch, TerrainModelSize);
	waterModel.bbox = terrainModel.bbox;

	terrainHeight.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R16_UNORM, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	terrainHeight.SetName("WaterSimTerrainHeight");
	resources.descriptors.createUAVView(terrainHeight);
	resources.descriptors.createTextureView(terrainHeight);

	terrainNormal.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R8G8_SNORM, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, { .mipLevels = 5 });
	terrainNormal.SetName("WaterSimTerrainNormal");
	resources.descriptors.createUAVView(terrainNormal);
	resources.descriptors.createTextureView(terrainNormal, -1);
	terrainNormalMips = resources.descriptors.createUAVMips(terrainNormal);

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

	Vector2 gridTileSize = { 102.4f, 102.4f };
	Vector3 terrainCenterPosition = { -20, -20, 30 };
	for (int x = 0; x < 5; x++)
		for (int y = 0; y < 5; y++)
		{
			auto e = sceneMgr.createEntity("WaterSimTerrain" + std::format("{}{}",x,y), Order::Normal);
			terrainGridMesh[x][y].create(64 * 64 * 4 * 4);
			terrainGridMesh[x][y].entity = e;
			e->geometry.fromInstancedModel(terrainModel, 0, terrainGridMesh[x][y].gpuBuffer.data[0].GpuAddress());
			e->setBoundingBox(terrainModel.bbox);
			e->material = resources.materials.getMaterial("DarkGreenGrid", batch);
			e->Material().setParam("TexIdHeightmap", terrainHeight.view.srvHeapIndex);
			e->Material().setParam("TexIdNormalmap", terrainNormal.view.srvHeapIndex);

			Vector3 pos = terrainCenterPosition;
			pos.x += (x - 2) * gridTileSize.x;
			pos.z += (y - 2) * gridTileSize.y;
			e->setPosition(pos);
		}

	terrainGridTiles.Initialize(gridTileSize, terrainCenterPosition, 7);

	auto e2 = sceneMgr.createEntity("WaterSim", Order::Transparent);
	waterGridMesh.create(64 * 64 * 4 * 4);
	waterGridMesh.entity = e2;
	e2->geometry.fromInstancedModel(waterModel, 0, waterGridMesh.gpuBuffer.data[0].GpuAddress());
	e2->setBoundingBox(waterModel.bbox);
	e2->material = resources.materials.getMaterial("WaterLake", batch);
	e2->Material().setParam("TexIdHeightmap", waterHeight[0].view.srvHeapIndex);
	e2->Material().setParam("TexIdNormalmap", waterNormalTexture.view.srvHeapIndex);
	e2->setPosition({ -20, -20, 30 });

	auto csShader = resources.shaders.getShader("momentumComputeShader", ShaderType::Compute, ShaderRef{ "waterSim/frenzySimMomentumCS.hlsl", "CS_Momentum", "cs_6_6" }); //frenzySimMomentumCS
	momentumComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("continuityComputeShader", ShaderType::Compute, ShaderRef{ "waterSim/frenzySimContinuityCS.hlsl", "CS_Continuity", "cs_6_6" }); //frenzySimContinuityCS
	continuityComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("generateHeightmapNormalsCS", ShaderType::Compute, ShaderRef{ "grid/generateHeightmapNormalsCS.hlsl", "CSMain", "cs_6_6" });
	heightmapToNormalCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("watermapToTextureCS", ShaderType::Compute, ShaderRef{ "utils/watermapToTextureCS.hlsl", "CSMain", "cs_6_6" });
	waterToTextureCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("copyTexturesCS", ShaderType::Compute, ShaderRef{ "utils/copyTexturesCS.hlsl", "CSMain", "cs_6_6" });
	copyTexturesCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("meshTextureCS", ShaderType::Compute, ShaderRef{ "waterSim/colorWaterSimTexture.hlsl", "CSMain", "cs_6_6" });
	meshTextureCS.init(*renderSystem.core.device, *csShader);

	generateNormalMipsCS.init(*renderSystem.core.device, "generateNormalMipmaps4x", resources.shaders);
}

static bool first = true;

void WaterSim::update(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx, Vector3 cameraPos)
{
	const UINT prevFrameIdx = (frameIdx == 0) ? (FrameCount - 1) : (frameIdx - 1);

	if (first)
	{
		TextureUtils::CopyTextureBuffer(commandList, srcWater.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterHeight[prevFrameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		TextureUtils::CopyTextureBuffer(commandList, srcWater.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterHeight[frameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		TextureTransitions<2> tr;
		tr.add(terrainTexture->texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.add(&terrainNormal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.push(commandList);

		copyTexturesCS.dispatch(commandList, terrainTexture->srvHandles, TextureSize, TextureSize, terrainHeight.view.uavHandles);
		heightmapToNormalCS.dispatch(commandList, terrainHeight.view.srvHeapIndex, terrainNormal.view.uavHeapIndex, TextureSize, TextureSize, 50, 102.4f);

		tr.add(terrainTexture->texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(&terrainNormal, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(&terrainHeight, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.push(commandList);
	}
	else
	{
		TextureTransitions<1> tr;
		tr.add(&terrainNormal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.push(commandList);

		heightmapToNormalCS.dispatch(commandList, terrainHeight.view.srvHeapIndex, terrainNormal.view.uavHeapIndex, TextureSize, TextureSize, 50, 102.4f);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(terrainNormal.texture.Get());
		commandList->ResourceBarrier(1, &uavb);

		generateNormalMipsCS.dispatch(commandList, TextureSize, terrainNormalMips);

		tr.add(&terrainNormal, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.push(commandList);
	}

	for (int x = 0; x < 5; x++)
		for (int y = 0; y < 5; y++)
		{
			terrainGridTiles.BuildLOD(cameraPos, { x - 2, y - 2 });
			terrainGridMesh[x][y].update((UINT)terrainGridTiles.m_renderList.size(), terrainGridTiles.m_renderList.data(), (UINT)terrainGridTiles.m_renderList.size() * sizeof(TileData), frameIdx);
		}

	terrainGridTiles.BuildLOD(cameraPos, {});
	waterGridMesh.update((UINT)terrainGridTiles.m_renderList.size(), terrainGridTiles.m_renderList.data(), (UINT)terrainGridTiles.m_renderList.size() * sizeof(TileData), frameIdx);
}

void WaterSim::updateCompute(RenderSystem& renderSystem, ID3D12GraphicsCommandList* computeList, float dt, UINT)
{
	CommandsMarker marker(computeList, "WaterSim", PixColor::LightBlue);

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

	const UINT WaterComputeIterations = 2;
	static UINT waterCurrentIdx = 0;
	const UINT waterPrevIdx = (waterCurrentIdx == 0) ? (WaterComputeIterations - 1) : (waterCurrentIdx - 1);
	const UINT BuffersCount = 2;
	const UINT waterResultIdx = (waterPrevIdx + WaterComputeIterations) % BuffersCount;

	WaterSimTextures t =
	{
		terrainHeight.view.srvHeapIndex,
		waterHeight[waterPrevIdx].view.uavHeapIndex,
		waterVelocity[waterPrevIdx].view.uavHeapIndex,
		waterHeight[waterCurrentIdx].view.uavHeapIndex,
		waterVelocity[waterCurrentIdx].view.uavHeapIndex
	};
	WaterSimTextures t2 =
	{
		terrainHeight.view.srvHeapIndex,
		waterHeight[waterCurrentIdx].view.uavHeapIndex,
		waterVelocity[waterCurrentIdx].view.uavHeapIndex,
		waterHeight[waterPrevIdx].view.uavHeapIndex,
		waterVelocity[waterPrevIdx].view.uavHeapIndex
	};

	auto safeTime = DtPerUpdate * 0.5f;
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
	waterToTextureCS.dispatch(computeList, waterHeight[waterResultIdx].view.srvHeapIndex, terrainHeight.view.srvHeapIndex, TextureSize, TextureSize, waterNormalTexture.view.uavHandles);
	generateNormalMipsCS.dispatch(computeList, TextureSize, waterNormalTextureMips);
	waterHeight[waterResultIdx].Transition(computeList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	waterCurrentIdx = (waterCurrentIdx + 1) % BuffersCount;
}

void WaterSim::clear()
{
}

void WaterSim::prepareForRendering(ID3D12GraphicsCommandList* commandList)
{
	waterNormalTexture.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void WaterSim::prepareAfterRendering(ID3D12GraphicsCommandList* commandList)
{
	waterNormalTexture.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}
