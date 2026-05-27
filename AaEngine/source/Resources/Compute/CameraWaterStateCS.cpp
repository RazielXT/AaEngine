#include "Resources/Compute/CameraWaterStateCS.h"

void CameraWaterStateCS::dispatch(ID3D12GraphicsCommandList* commandList, const DispatchParams& params, ID3D12Resource* stateBuffer)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct CSParams
	{
		UINT TexIdWaterHeight;
		float dt;
		XMFLOAT2 worldCenter;
		XMFLOAT2 worldSize;
		XMFLOAT2 _pad0;
		XMFLOAT3 cameraPosition;
		float waterHeightScale;
		float waterHeightStart;
		float dryingSpeed;
		UINT resetState;
	}
	data = {
		params.waterHeightSrvIndex,
		params.dt,
		{ params.worldCenter.x, params.worldCenter.y },
		{ params.worldSize.x, params.worldSize.y },
		{ 0.0f, 0.0f },
		{ params.cameraPosition.x, params.cameraPosition.y, params.cameraPosition.z },
		params.waterHeightScale,
		params.waterHeightStart,
		params.dryingSpeed,
		params.resetState
	};

	static_assert(sizeof(CSParams) == 60, "CameraWaterStateCS constants must match HLSL cbuffer size (60 bytes)");

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(UINT), &data, 0);
	commandList->SetComputeRootUnorderedAccessView(1, stateBuffer->GetGPUVirtualAddress());
	commandList->Dispatch(1, 1, 1);
}