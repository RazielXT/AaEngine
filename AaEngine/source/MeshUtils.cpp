#include "MeshUtils.h"
#include "directx\d3dx12.h"

MeshUtils::MeshBuffer MeshUtils::CreateMeshBuffer(ID3D12Device* device, UINT vertexCount, UINT vertexStride, UINT indexCount, DXGI_FORMAT indexFormat, D3D12_RESOURCE_STATES state)
{
	MeshBuffer mesh{};

	// Vertex buffer
	UINT vbSize = vertexCount * vertexStride;
	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

	device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&vbDesc,
		state, // ready for compute shader write
		nullptr,
		IID_PPV_ARGS(&mesh.vertexBuffer));

	mesh.vbView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
	mesh.vbView.SizeInBytes = vbSize;
	mesh.vbView.StrideInBytes = vertexStride;

	// Index buffer
	UINT ibSize = indexCount * (indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4);
	CD3DX12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

	device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&ibDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, // compute shader will fill
		nullptr,
		IID_PPV_ARGS(&mesh.indexBuffer));

	mesh.ibView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
	mesh.ibView.SizeInBytes = ibSize;
	mesh.ibView.Format = indexFormat;

	return mesh;
}

MeshUtils::MeshBuffer MeshUtils::CreateGridMeshBuffer(ID3D12Device* device, UINT width, UINT height, UINT vertexStride, DXGI_FORMAT indexFormat, D3D12_RESOURCE_STATES state)
{
	// Vertex count
	UINT vertexCount = width * height;

	// Index count
	UINT quadCount = (width - 1) * (height - 1);
	UINT indexCount = quadCount * 6;

	return CreateMeshBuffer(device, vertexCount, vertexStride, indexCount, indexFormat, state);
}
