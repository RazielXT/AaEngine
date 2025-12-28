#include "TextureToMeshCS.h"

void TextureToMeshCS::dispatch(ID3D12GraphicsCommandList* commandList, UINT terrain, UINT w, UINT h, float scale, ID3D12Resource* vertexBuffer)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct Input
	{
		UINT width;
		UINT height;
		float scale;
		UINT terrain;
	}
	data = { w, h, scale, terrain };

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);
	commandList->SetComputeRootUnorderedAccessView(1, vertexBuffer->GetGPUVirtualAddress());

	commandList->Dispatch(data.width / 8, data.height / 8, 1);
}

void WaterTextureToMeshCS::dispatch(ID3D12GraphicsCommandList* commandList, UINT water, UINT w, UINT h, ID3D12Resource* vertexBuffer)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct Input
	{
		UINT width;
		UINT height;
		UINT water;
	}
	data = { w, h, water };

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);
	commandList->SetComputeRootUnorderedAccessView(1, vertexBuffer->GetGPUVirtualAddress());

	commandList->Dispatch(data.width / 8, data.height / 8, 1);
}

void WaterTextureToTextureCS::dispatch(ID3D12GraphicsCommandList* commandList, UINT water, UINT terrain, UINT w, UINT h, D3D12_GPU_DESCRIPTOR_HANDLE output)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct Input
	{
		UINT width;
		UINT height;
		UINT water;
		UINT terrain;
	}
	data = { w, h, water };

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);
	commandList->SetComputeRootDescriptorTable(1, output);

	commandList->Dispatch(data.width / 8, data.height / 8, 1);
}

void GenerateHeightmapNormalsCS::dispatch(ID3D12GraphicsCommandList* commandList, UINT terrain, UINT normals, UINT w, UINT h, float heightScale, float worldSize)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct Input
	{
		UINT ResIdHeightMap;
		UINT ResIdNormalMap;
		float HeightScale;
		float UnitSize;
	}
	data = { terrain, normals, heightScale, worldSize / w };

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);

	commandList->Dispatch(w / 8, h / 8, 1);
}
