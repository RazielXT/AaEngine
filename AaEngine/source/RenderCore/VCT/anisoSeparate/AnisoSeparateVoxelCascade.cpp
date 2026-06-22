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

void AnisoSeparateVoxelCascade::prepareForVoxelization(ID3D12GraphicsCommandList* commandList, TextureStatePair faceStates[FaceCount], TextureStatePair prevFaceStates[FaceCount], TextureStatePair& occupancyState, TextureStatePair& prevOccupancyState, ClearTextureComputeShader& clearTextureCS)
{
	TextureTransitions<FaceCount + 1> tr;

	// Clear current state ---------------------------

	tr.add(occupancyState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	for (UINT f = 0; f < FaceCount; f++) {
		tr.add(faceStates[f], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}
	tr.push(commandList);

	clearTextureCS.dispatch(commandList, voxelOccupancyTexture.view, { VoxelSize, VoxelSize, VoxelSize });

	for (UINT f = 0; f < FaceCount; f++)
		clearTextureCS.dispatch(commandList, voxelFaceTextures[f].view, { VoxelSize, VoxelSize, VoxelSize });

	// Transition for usage ---------------------------

	tr.add(occupancyState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for (UINT f = 0; f < FaceCount; f++) {
		tr.add(faceStates[f], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	tr.push(commandList);

	// UAV ---------------------------

	tr.addUav(voxelOccupancyTexture.texture.Get());

	for (UINT f = 0; f < FaceCount; f++) {
		tr.addUav(voxelFaceTextures[f].texture.Get());
	}
	tr.push(commandList);
}

void AnisoSeparateVoxelCascade::clearAll(ID3D12GraphicsCommandList* commandList, ClearTextureComputeShader& clearTextureCS)
{
	TextureTransitions<FaceCount + 1> tr;
	tr.add(voxelOccupancyTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	for (UINT f = 0; f < FaceCount; f++)
		tr.add(voxelFaceTextures[f], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tr.push(commandList);

	clearTextureCS.dispatch(commandList, voxelOccupancyTexture.view, { VoxelSize, VoxelSize, VoxelSize });

	for (UINT f = 0; f < FaceCount; f++)
		clearTextureCS.dispatch(commandList, voxelFaceTextures[f].view, { VoxelSize, VoxelSize, VoxelSize });

	tr.add(voxelOccupancyTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	for (UINT f = 0; f < FaceCount; f++)
		tr.add(voxelFaceTextures[f], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	tr.push(commandList);

	// UAV ---------------------------

	tr.addUav(voxelOccupancyTexture.texture.Get());

	for (UINT f = 0; f < FaceCount; f++) {
		tr.addUav(voxelFaceTextures[f].texture.Get());
	}
	tr.push(commandList);
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