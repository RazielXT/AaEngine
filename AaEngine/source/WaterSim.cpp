#include "WaterSim.h"
#include "TextureUtils.h"

WaterSim::WaterSim()
{
}

WaterSim::~WaterSim()
{
}

const UINT TextureSize = 512;

void WaterSim::initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources)
{
	ResourceUploadBatch batch(renderSystem.core.device);
	batch.Begin();

	terrainTexture = resources.textures.loadFile(*renderSystem.core.device, batch, "simpleHeightmap.png");

	// Generate gradient data
	std::vector<float> gradient(TextureSize* TextureSize);
	for (UINT y = 0; y < TextureSize; ++y)
	{
		for (UINT x = 0; x < TextureSize; ++x)
		{
			float t = static_cast<float>(x) / (TextureSize - 1);
			gradient[y * TextureSize + x] = t * 10;
		}
	}
	srcWater = TextureUtils::CreateUploadTexture(renderSystem.core.device, batch, gradient.data(), TextureSize, TextureSize, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	auto uploadResourcesFinished = batch.End(renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();

	resources.descriptors.createTextureView(*terrainTexture);


	terrainHeight.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	terrainHeight.SetName("WaterSimTerrainHeight");
	resources.descriptors.createUAVView(terrainHeight);
	resources.descriptors.createTextureView(terrainHeight);

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

	auto csShader = resources.shaders.getShader("WaterSimCS", ShaderTypeCompute, ShaderRef{ "waterSim/waterSimCS.hlsl", "main", "cs_6_6" });
	waterSimCS.init(*renderSystem.core.device, "WaterSimCS", *csShader);
}

bool first = true;

void WaterSim::update(ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx)
{
	const UINT prevFrameIdx = (frameIdx == 0) ? (FrameCount - 1) : (frameIdx - 1);

	if (first)
	{
		TextureUtils::CopyTextureBuffer(commandList, srcWater.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterHeight[prevFrameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		first = false;
	}

	WaterSimComputeShader::Textures t =
	{
		terrainTexture->srvHeapIndex,
		waterHeight[prevFrameIdx].view.uavHeapIndex,
		waterVelocity[prevFrameIdx].view.uavHeapIndex,
		waterHeight[frameIdx].view.uavHeapIndex,
		waterVelocity[frameIdx].view.uavHeapIndex
	};

	waterSimCS.dispatch(commandList, TextureSize, dt, t);
}

void WaterSim::clear()
{
}
