#pragma once

#include "Directx.h"

namespace MeshUtils
{
	struct MeshBuffer
	{
		ComPtr<ID3D12Resource> vertexBuffer;
		ComPtr<ID3D12Resource> indexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vbView{};
		D3D12_INDEX_BUFFER_VIEW ibView{};
	};

	MeshBuffer CreateMeshBuffer(
		ID3D12Device* device,
		UINT vertexCount,
		UINT vertexStride,
		UINT indexCount,
		DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT,
		D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	MeshBuffer CreateGridMeshBuffer(
		ID3D12Device* device,
		UINT width,
		UINT height,
		UINT vertexStride,
		DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT,
		D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
};
