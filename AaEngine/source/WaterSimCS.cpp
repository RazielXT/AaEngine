#include "WaterSimCS.h"

struct CSParams
{
	XMUINT2 gridSize;	// simulation dimensions
	float dt;		  // timestep
	float gravity;	 // gravity constant
	float cellSize;	// grid spacing
	UINT TexIdHeighMap;
	UINT TexIdWaterMap;
	UINT TexIdVelocityMap;
	UINT TexIdOutWaterMap;
	UINT TexIdOutVelocityMap;
};

// struct CSParams
// {
// 	XMFLOAT2 uvScale; // size of grid in world
// 	XMUINT2 gridSize;	// simulation dimensions
// 	XMFLOAT4 misc12;
// 	float worldScale; // 1/uvScale
// 	float dt; // timestep
// 	float gravity; // gravity constant
// 	float terrainScale;
// 	XMFLOAT4 terrainSampleOffset; // cb2[1]
// 	XMFLOAT4 heightSampleOffset;  // cb2[2]
// 	XMFLOAT2 gridScale;      // cb2[0]
// 	UINT TexIdHeighMap;
// 	UINT TexIdWaterMap;
// 	UINT TexIdVelocityMap;
// 	UINT TexIdOutWaterMap;
// 	UINT TexIdOutVelocityMap;
// };
// XMFLOAT4 misc12 = { 0.00042f, 0.00104f, 1.00000E-06f, 0.00f };

void WaterSimMomentumComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, UINT gridSize, float dt, float spacing, WaterSimTextures textures)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	CSParams data = {
		{gridSize, gridSize}, dt, 9.81f, spacing, textures.heighMapId, textures.waterMapId, textures.velocityMapId, textures.outWaterMapId, textures.outVelocityMapId
	};

// 	XMFLOAT4 scales = { 1.f / gridSize, 1.f / gridSize, (float)gridSize, (float)gridSize };
// 	CSParams data = {
// 		{51.2f,51.2f}, {gridSize, gridSize}, misc12, 1 / 51.2f, dt, 9.81f, 10, scales, scales, { scales.x, scales.y }, textures.heighMapId, textures.waterMapId, textures.velocityMapId, textures.outWaterMapId, textures.outVelocityMapId
// 	};

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);

	const auto groupSize = gridSize / 8;
	commandList->Dispatch(groupSize, groupSize, 1);
}

void WaterSimContinuityComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, UINT gridSize, float dt, float spacing, WaterSimTextures textures)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	CSParams data = {
		{gridSize, gridSize}, dt, 9.81f, spacing, textures.heighMapId, textures.waterMapId, textures.velocityMapId, textures.outWaterMapId, textures.outVelocityMapId
	};

// 	XMFLOAT4 scales = { 1.f / gridSize, 1.f / gridSize, (float)gridSize, (float)gridSize };
// 	CSParams data = {
// 		{51.2f,51.2f}, {gridSize, gridSize}, misc12, 1 / 51.2f, dt, 9.81f, 10, scales, scales, { scales.x, scales.y }, textures.heighMapId, textures.waterMapId, textures.velocityMapId, textures.outWaterMapId, textures.outVelocityMapId
// 	};

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);

	const auto groupSize = gridSize / 8;
	commandList->Dispatch(groupSize, groupSize, 1);
}

void WaterMeshTextureCS::dispatch(ID3D12GraphicsCommandList* commandList, InputParams input)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(float), &input, 0);

	commandList->Dispatch(input.gridSize.x / 8, input.gridSize.y / 8, 1);
}
