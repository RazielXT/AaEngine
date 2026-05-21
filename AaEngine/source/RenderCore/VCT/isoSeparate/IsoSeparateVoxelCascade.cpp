#include "RenderCore/VCT/isoSeparate/IsoSeparateVoxelCascade.h"
#include "Resources/GraphicsResources.h"
#include "Resources/Compute/ClearBufferCS.h"
#include "Utils/StringUtils.h"

constexpr float VoxelSize = 128.f;
constexpr DXGI_FORMAT VoxelColorFormat = DXGI_FORMAT_R11G11B10_FLOAT;
constexpr DXGI_FORMAT VoxelOccupancyFormat = DXGI_FORMAT_R8_UNORM;

void IsoSeparateVoxelCascade::initialize(const std::string& n, ID3D12Device* device, GraphicsResources& resources)
{
	name = n;

	voxelSceneTexture.Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelColorFormat);
	voxelSceneTexture.SetName(name.c_str());
	resources.descriptors.createTextureView(voxelSceneTexture);
	resources.descriptors.createUAVView(voxelSceneTexture);

	auto prevName = name + "Previous";
	voxelPreviousSceneTexture.Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelColorFormat);
	voxelPreviousSceneTexture.SetName(prevName.c_str());
	resources.descriptors.createTextureView(voxelPreviousSceneTexture);

	auto occupancyName = name + "Occupancy";
	voxelOccupancyTexture.Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelOccupancyFormat);
	voxelOccupancyTexture.SetName(occupancyName.c_str());
	resources.descriptors.createTextureView(voxelOccupancyTexture);
	resources.descriptors.createUAVView(voxelOccupancyTexture);

	auto prevOccupancyName = name + "PreviousOccupancy";
	voxelPreviousOccupancyTexture.Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelOccupancyFormat);
	voxelPreviousOccupancyTexture.SetName(prevOccupancyName.c_str());
	resources.descriptors.createTextureView(voxelPreviousOccupancyTexture);

	voxelInfoBuffer.data = resources.shaderBuffers.CreateStructuredBuffer(DataElementSize * DataElementCount);
	voxelInfoBuffer.data->SetName(as_wstring(name + "DataBuffer").c_str());
	voxelInfoBuffer.view = resources.descriptors.createBufferView(voxelInfoBuffer.data.Get(), DataElementSize, DataElementCount);
}

void IsoSeparateVoxelCascade::prepareForVoxelization(ID3D12GraphicsCommandList* commandList, TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, TextureStatePair& voxelOccupancy, TextureStatePair& prevVoxelOccupancy, const GpuTexture3D& clearColor, const GpuTexture3D& clearOccupancy, ClearBufferComputeShader& clearBufferCS)
{
	clearBufferCS.dispatch(commandList, voxelInfoBuffer.data.Get(), DataElementSize * DataElementCount / sizeof(float));

	TextureTransitions<4> tr;
	tr.add(voxelScene, D3D12_RESOURCE_STATE_COPY_SOURCE);
	tr.add(prevVoxelScene, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.add(voxelOccupancy, D3D12_RESOURCE_STATE_COPY_SOURCE);
	tr.add(prevVoxelOccupancy, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.push(commandList);

	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), voxelSceneTexture.texture.Get());
	commandList->CopyResource(voxelPreviousOccupancyTexture.texture.Get(), voxelOccupancyTexture.texture.Get());

	voxelScene.Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	voxelOccupancy.Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyResource(voxelSceneTexture.texture.Get(), clearColor.texture.Get());
	commandList->CopyResource(voxelOccupancyTexture.texture.Get(), clearOccupancy.texture.Get());

	auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(voxelInfoBuffer.data.Get());
	commandList->ResourceBarrier(1, &uavBarrier);

	tr.add(voxelScene, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	tr.add(prevVoxelScene, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	tr.add(voxelOccupancy, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	tr.add(prevVoxelOccupancy, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	tr.push(commandList);
}

void IsoSeparateVoxelCascade::clearAll(ID3D12GraphicsCommandList* commandList, const GpuTexture3D& clearColor, const GpuTexture3D& clearOccupancy, ClearBufferComputeShader& clearBufferCS)
{
	clearBufferCS.dispatch(commandList, voxelInfoBuffer.data.Get(), DataElementSize * DataElementCount / sizeof(float));

	TextureTransitions<4> tr;
	tr.add(voxelSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.add(voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.add(voxelOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.add(voxelPreviousOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	tr.push(commandList);

	commandList->CopyResource(voxelSceneTexture.texture.Get(), clearColor.texture.Get());
	commandList->CopyResource(voxelPreviousSceneTexture.texture.Get(), clearColor.texture.Get());
	commandList->CopyResource(voxelOccupancyTexture.texture.Get(), clearOccupancy.texture.Get());
	commandList->CopyResource(voxelPreviousOccupancyTexture.texture.Get(), clearOccupancy.texture.Get());

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(voxelInfoBuffer.data.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	tr.add(voxelSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.add(voxelPreviousSceneTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.add(voxelOccupancyTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.add(voxelPreviousOccupancyTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.push(commandList);
}

static Vector3 SnapToGrid(Vector3 position, float gridStep)
{
	position.x = std::round(position.x / gridStep) * gridStep;
	position.y = std::round(position.y / gridStep) * gridStep;
	position.z = std::round(position.z / gridStep) * gridStep;
	return position;
}

Vector3 IsoSeparateVoxelCascade::update(const Vector3& cameraPosition)
{
	float step = settings.extends * 2 / VoxelSize;

	const float VoxelMipmapStep = 32;
	step *= VoxelMipmapStep;

	settings.center = SnapToGrid(cameraPosition, step);

	auto diff = (settings.lastCenter - settings.center) / step;
	settings.lastCenter = settings.center;

	return diff;
}