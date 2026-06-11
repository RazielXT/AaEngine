#include "RenderCore/VCT/anisoSeparate/AnisoSeparateVoxelCascade.h"
#include "Resources/GraphicsResources.h"
#include "Resources/Compute/ClearBufferCS.h"
#include "Utils/StringUtils.h"

static const char* FaceNames[AnisoSeparateVoxelCascade::FaceCount] = { "PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ" };

void AnisoSeparateVoxelCascade::initialize(const std::string& n, ID3D12Device* device, GraphicsResources& resources)
{
	name = n;

	for (UINT f = 0; f < FaceCount; f++)
	{
		auto faceName = name + FaceNames[f];
		voxelFaceTextures[f].Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelColorFormat);
		voxelFaceTextures[f].SetName(faceName.c_str());
		resources.descriptors.createTextureView(voxelFaceTextures[f]);
		resources.descriptors.createUAVView(voxelFaceTextures[f]);

		auto prevName = faceName + "Prev";
		voxelPreviousFaceTextures[f].Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelColorFormat);
		voxelPreviousFaceTextures[f].SetName(prevName.c_str());
		resources.descriptors.createTextureView(voxelPreviousFaceTextures[f]);
	}

	auto occupancyName = name + "Occupancy";
	voxelOccupancyTexture.Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelOccupancyFormat);
	voxelOccupancyTexture.SetName(occupancyName.c_str());
	resources.descriptors.createTextureView(voxelOccupancyTexture);
	resources.descriptors.createUAVView(voxelOccupancyTexture);

	auto prevOccupancyName = name + "PrevOccupancy";
	voxelPreviousOccupancyTexture.Init(device, VoxelSize, VoxelSize, VoxelSize, VoxelOccupancyFormat);
	voxelPreviousOccupancyTexture.SetName(prevOccupancyName.c_str());
	resources.descriptors.createTextureView(voxelPreviousOccupancyTexture);

	auto opacityGridName = name + "OccupancyBitmask";
	voxelOccupancyBitmaskTexture.Init(device, OccupancyBitmaskSize, OccupancyBitmaskSize, OccupancyBitmaskSize, OccupancyBitmaskFormat);
	voxelOccupancyBitmaskTexture.SetName(opacityGridName.c_str());
	resources.descriptors.createTextureView(voxelOccupancyBitmaskTexture);
	resources.descriptors.createUAVView(voxelOccupancyBitmaskTexture);

	voxelInfoBuffer.data = resources.shaderBuffers.CreateStructuredBuffer(DataElementSize * DataElementCount);
	voxelInfoBuffer.data->SetName(as_wstring(name + "DataBuffer").c_str());
	voxelInfoBuffer.view = resources.descriptors.createBufferView(voxelInfoBuffer.data.Get(), DataElementSize, DataElementCount);
}

void AnisoSeparateVoxelCascade::prepareForVoxelization(ID3D12GraphicsCommandList* commandList, TextureStatePair faceStates[FaceCount], TextureStatePair prevFaceStates[FaceCount], TextureStatePair& occupancyState, TextureStatePair& prevOccupancyState, const GpuTexture3D& clearColor, const GpuTexture3D& clearOccupancy, ClearBufferComputeShader& clearBufferCS)
{
	clearBufferCS.dispatch(commandList, voxelInfoBuffer.data.Get(), DataElementSize * DataElementCount / sizeof(float));

	{
		TextureTransitions<2> tr;
		tr.add(occupancyState, D3D12_RESOURCE_STATE_COPY_SOURCE);
		tr.add(prevOccupancyState, D3D12_RESOURCE_STATE_COPY_DEST);
		tr.push(commandList);

		commandList->CopyResource(voxelPreviousOccupancyTexture.texture.Get(), voxelOccupancyTexture.texture.Get());

		occupancyState.Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->CopyResource(voxelOccupancyTexture.texture.Get(), clearOccupancy.texture.Get());

		tr.add(occupancyState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(prevOccupancyState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.push(commandList);
	}

	for (UINT f = 0; f < FaceCount; f++)
	{
		TextureTransitions<2> tr;
		tr.add(faceStates[f], D3D12_RESOURCE_STATE_COPY_SOURCE);
		tr.add(prevFaceStates[f], D3D12_RESOURCE_STATE_COPY_DEST);
		tr.push(commandList);

		commandList->CopyResource(voxelPreviousFaceTextures[f].texture.Get(), voxelFaceTextures[f].texture.Get());

		faceStates[f].Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->CopyResource(voxelFaceTextures[f].texture.Get(), clearColor.texture.Get());

		tr.add(faceStates[f], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(prevFaceStates[f], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.push(commandList);
	}

	auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(voxelInfoBuffer.data.Get());
	commandList->ResourceBarrier(1, &uavBarrier);
}

void AnisoSeparateVoxelCascade::clearAll(ID3D12GraphicsCommandList* commandList, const GpuTexture3D& clearColor, const GpuTexture3D& clearOccupancy, ClearBufferComputeShader& clearBufferCS)
{
	clearBufferCS.dispatch(commandList, voxelInfoBuffer.data.Get(), DataElementSize * DataElementCount / sizeof(float));

	{
		TextureTransitions<2> tr;
		tr.add(voxelOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		tr.add(voxelPreviousOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		tr.push(commandList);

		commandList->CopyResource(voxelOccupancyTexture.texture.Get(), clearOccupancy.texture.Get());
		commandList->CopyResource(voxelPreviousOccupancyTexture.texture.Get(), clearOccupancy.texture.Get());

		tr.add(voxelOccupancyTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.add(voxelPreviousOccupancyTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.push(commandList);
	}

	for (UINT f = 0; f < FaceCount; f++)
	{
		TextureTransitions<2> tr;
		tr.add(voxelFaceTextures[f], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		tr.add(voxelPreviousFaceTextures[f], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		tr.push(commandList);

		commandList->CopyResource(voxelFaceTextures[f].texture.Get(), clearColor.texture.Get());
		commandList->CopyResource(voxelPreviousFaceTextures[f].texture.Get(), clearColor.texture.Get());

		tr.add(voxelFaceTextures[f], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.add(voxelPreviousFaceTextures[f], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.push(commandList);
	}

	{
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(voxelInfoBuffer.data.Get());
		commandList->ResourceBarrier(1, &uavBarrier);
	}
}

static Vector3 SnapToGrid(Vector3 position, float gridStep)
{
	position.x = std::round(position.x / gridStep) * gridStep;
	position.y = std::round(position.y / gridStep) * gridStep;
	position.z = std::round(position.z / gridStep) * gridStep;
	return position;
}

Vector3 AnisoSeparateVoxelCascade::update(const Vector3& cameraPosition)
{
	float step = settings.extends * 2 / (float)VoxelSize;

	const float VoxelMipmapStep = 32;
	step *= VoxelMipmapStep;

	settings.center = SnapToGrid(cameraPosition, step);

	auto diff = (settings.lastCenter - settings.center) / step;
	settings.lastCenter = settings.center;

	return diff;
}