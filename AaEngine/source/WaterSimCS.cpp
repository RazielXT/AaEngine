#include "WaterSimCS.h"

void WaterSimComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, UINT gridSize, float dt, Textures textures)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct
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
	}
	data = {
		{gridSize, gridSize}, dt, 9.81f, 0.2f, textures.heighMapId, textures.waterMapId, textures.velocityMapId, textures.outWaterMapId, textures.outVelocityMapId
	};

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);
	//commandList->SetComputeRootUnorderedAccessView(1, vertexBuffer->GetGPUVirtualAddress());

	const auto groupSize = gridSize / 8;
	commandList->Dispatch(groupSize, groupSize, 1);
}
