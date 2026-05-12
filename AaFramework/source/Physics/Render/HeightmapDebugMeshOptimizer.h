#pragma once

#include <vector>
#include "Resources/Model/VertexBufferModel.h"
#include "BufferHelpers.h"

template <typename GridVertex, typename Triangle, UINT GridQuadsRow>
struct HeightmapDebugMeshOptimizer
{
	std::vector<GridVertex> verticesBuffer;
	ComPtr<ID3D12Resource> indexBuffer;

	void ReconstructGrid(ID3D12Device* device, ResourceUploadBatch& batch, VertexBufferModel& model, const Triangle* tris)
	{
		const UINT vertsRow = GridQuadsRow + 1;

		verticesBuffer.resize(vertsRow * vertsRow);

		auto V = [&](UINT x, UINT y) -> GridVertex&
			{
				return verticesBuffer[y * vertsRow + x];
			};

		UINT tri_index = 0;

		for (UINT y = 0; y < GridQuadsRow; ++y)
		{
			for (UINT x = 0; x < GridQuadsRow; ++x)
			{
				// tri0:
				// A = x,y
				// B = x,y+1
				// C = x+1,y+1
				// tri1:
				// A = x,y
				// C = x+1,y+1
				// D = x+1,y

				const auto& t0 = tris[tri_index++];
				const auto& t1 = tris[tri_index++];

				if (x == 0)
				{
					V(0, y) = t0.mV[0];
					V(0, y + 1) = t0.mV[1];
				}

				V(x + 1, y) = t1.mV[2];
				V(x + 1, y + 1) = t0.mV[2];
			}
		}

		PrepareIndexBuffer(device, batch);

		model.SetIndexBuffer(indexBuffer.Get(), 6 * GridQuadsRow * GridQuadsRow);
		model.CreateVertexBuffer(device, &batch, verticesBuffer.data(), (UINT)verticesBuffer.size(), sizeof(GridVertex), true);
	}

	void PrepareIndexBuffer(ID3D12Device* device, ResourceUploadBatch& batch)
	{
		if (indexBuffer)
			return;

		const UINT vertsRow = GridQuadsRow + 1;

		std::vector<uint16_t> indices;
		indices.reserve((vertsRow - 1) * (vertsRow - 1) * 6);

		for (UINT y = 0; y < vertsRow - 1; ++y)
		{
			for (UINT x = 0; x < vertsRow - 1; ++x)
			{
				uint16_t v0 = (uint16_t)y * vertsRow + x;
				uint16_t v1 = (uint16_t)v0 + 1;
				uint16_t v2 = (uint16_t)v0 + vertsRow;
				uint16_t v3 = (uint16_t)v2 + 1;

				// First triangle
				indices.push_back(v0);
				indices.push_back(v2);
				indices.push_back(v1);

				// Second triangle
				indices.push_back(v1);
				indices.push_back(v2);
				indices.push_back(v3);
			}
		}

		CreateStaticBuffer(device, batch, indices, D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);
		indexBuffer->SetName(L"HeightmapDebugMeshOptimizer");
	}
};
