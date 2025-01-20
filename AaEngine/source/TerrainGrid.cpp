#include "TerrainGrid.h"
#include "GraphicsResources.h"
#include "BufferHelpers.h"

struct Vertex
{
	Vector3 pos;
	Vector2 uv;
	Vector3 normal;
	Vector3 tangent;
};

static bool isInnerChunk(int x, int y)
{
	return x != 0 && x != 3 && y != 0 && y != 3;
}

void TerrainGrid::init(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	createIndexBuffer(renderSystem, batch);

	chunks.resize(16 + 12 + 12);
	float fullWidth = 1024.f;

	int i = 0;
	for (int x = 0; x < 4; x++)
		for (int y = 0; y < 4; y++)
		{
			auto& chunk = chunks[i++];
			createChunk(chunk, resources, L"TerrainChunkVB0", fullWidth / 16.f);
			lod0.data[x][y].chunk = &chunk;
		}

	for (int x = 0; x < 4; x++)
		for (int y = 0; y < 4; y++)
		{
			if (isInnerChunk(x, y))
				continue;

			auto& chunk = chunks[i++];
			createChunk(chunk, resources, L"TerrainChunkVB1", fullWidth / 8.f);
			lod1.data[x][y].chunk = &chunk;
		}

	for (int x = 0; x < 4; x++)
		for (int y = 0; y < 4; y++)
		{
			if (isInnerChunk(x, y))
				continue;

			auto& chunk = chunks[i++];
			createChunk(chunk, resources, L"TerrainChunkVB2", fullWidth / 4.f);
			lod2.data[x][y].chunk = &chunk;
		}

	lod2.borderSeamsFix = false;
}

void TerrainGrid::createChunk(TerrainChunk& chunk, GraphicsResources& resources, const wchar_t* name, float width)
{
	auto& vertexBuffer = chunk.vertexBuffer;
	vertexBuffer = resources.shaderBuffers.CreateStructuredBuffer(sizeof(Vertex) * ChunkSize * ChunkSize);
	vertexBuffer->SetName(name);

	auto& model = chunk.model;
	model.addLayoutElement(0, 0, DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION, 0);
	model.addLayoutElement(0, model.getLayoutVertexSize(0), DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD, 0);
	model.addLayoutElement(0, model.getLayoutVertexSize(0), DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL, 0);
	model.addLayoutElement(0, model.getLayoutVertexSize(0), DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::TANGENT, 0);

	model.CreateVertexBuffer(vertexBuffer.Get(), ChunkSize * ChunkSize, sizeof(Vertex));
	model.CreateIndexBuffer(indexBuffer.Get(), IndexCount);

	model.bbox.Extents = { width / 2.f, 360.f / 2.f, width / 2.f };
	model.bbox.Center = model.bbox.Extents;
}

void TerrainGrid::createIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	std::vector<uint16_t> data;
	data.resize(IndexCount);

	for (int x = 0; x < ChunkSize; x++)
		for (int y = 0; y < ChunkSize; y++)
		{
			if (x < (ChunkSize - 1) && y < (ChunkSize - 1))
			{
				int baseIndex = x + y * ChunkSize;
				int indexOffset = 6 * (x + y * (ChunkSize - 1));

				bool flip = (x + y) % 2 == 0;

				if (flip)
				{
					// Triangle pattern: |/
					data[indexOffset + 0] = (uint16_t)baseIndex;
					data[indexOffset + 1] = (uint16_t)baseIndex + ChunkSize + 1;
					data[indexOffset + 2] = (uint16_t)baseIndex + 1;

					data[indexOffset + 3] = (uint16_t)baseIndex;
					data[indexOffset + 4] = (uint16_t)baseIndex + ChunkSize;
					data[indexOffset + 5] = (uint16_t)baseIndex + ChunkSize + 1;
				}
				else
				{
					// Triangle pattern: |\,
					data[indexOffset + 0] = (uint16_t)baseIndex;
					data[indexOffset + 1] = (uint16_t)baseIndex + ChunkSize;
					data[indexOffset + 2] = (uint16_t)baseIndex + 1;

					data[indexOffset + 3] = (uint16_t)baseIndex + 1;
					data[indexOffset + 4] = (uint16_t)baseIndex + ChunkSize;
					data[indexOffset + 5] = (uint16_t)baseIndex + ChunkSize + 1;
				}
			}
		}

	CreateStaticBuffer(renderSystem.core.device, batch,	data, D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);
	indexBuffer->SetName(L"TerrainChunkIB");
}
