#include "TerrainGrid.h"
#include "GraphicsResources.h"
#include "BufferHelpers.h"

struct Vertex
{
	Vector3 pos;
	Vector3 normal;
	Vector3 tangent;
	Vector4 normalLod;
	Vector3 tangentLod;
};

static bool isInnerChunk(int x, int y)
{
	return x != 0 && x != 3 && y != 0 && y != 3;
}

void TerrainGrid::init(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, float worldWidth, float maxHeight)
{
	createIndexBuffer(renderSystem, batch);

	chunks.resize(12 * LodsCount + 4); //outer 4x4 + closest lod inner

	int counter = 0;

	for (auto i = LodsCount; i > 0; i--)
	{
		size_t lodIdx = i - 1;
		const UINT GridWidthCount = 4;
		float worldPart = (GridWidthCount * pow(2, LodsCount - i)); // 4,8,16 etc
		float chunkWidth = worldWidth / worldPart;

		for (int x = 0; x < 4; x++)
			for (int y = 0; y < 4; y++)
			{
				if (lodIdx != 0 && isInnerChunk(x, y))
					continue;

				auto& chunk = chunks[counter++];
				createChunk(chunk, resources, L"TerrainChunkVB", chunkWidth, maxHeight);
				lods[lodIdx].data[x][y].chunk = &chunk;
			}
	}

//	lod4.fixBorderSeams = false;
}

void TerrainGrid::createChunk(TerrainChunk& chunk, GraphicsResources& resources, const wchar_t* name, float width, float maxHeight)
{
	auto& vertexBuffer = chunk.vertexBuffer;
	vertexBuffer = resources.shaderBuffers.CreateStructuredBuffer(sizeof(Vertex) * ChunkWidthVertices * ChunkWidthVertices);
	vertexBuffer->SetName(name);

	auto& model = chunk.model;
	model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION);
	model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL);
	model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::TANGENT);
	model.addLayoutElement(DXGI_FORMAT_R32G32B32A32_FLOAT, VertexElementSemantic::TEXCOORD, 1); //blend lod normal+height
	model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::TEXCOORD, 2); //blend lod tangent

	model.CreateVertexBuffer(vertexBuffer.Get(), ChunkWidthVertices * ChunkWidthVertices, sizeof(Vertex));
	model.CreateIndexBuffer(indexBuffer.Get(), IndexCount);

	model.bbox.Extents = { width / 2.f, maxHeight / 2.f, width / 2.f };
	model.bbox.Center = model.bbox.Extents;
}

void TerrainGrid::createIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	std::vector<uint16_t> data;
	data.resize(IndexCount);

	for (int x = 0; x < ChunkWidthVertices; x++)
		for (int y = 0; y < ChunkWidthVertices; y++)
		{
			if (x < (ChunkWidthVertices - 1) && y < (ChunkWidthVertices - 1))
			{
				int baseIndex = x + y * ChunkWidthVertices;
				int indexOffset = 6 * (x + y * (ChunkWidthVertices - 1));

				bool flip = (x + y) % 2 == 0;

				if (flip)
				{
					// Triangle pattern: |/
					data[indexOffset + 0] = (uint16_t)baseIndex;
					data[indexOffset + 1] = (uint16_t)baseIndex + ChunkWidthVertices + 1;
					data[indexOffset + 2] = (uint16_t)baseIndex + 1;

					data[indexOffset + 3] = (uint16_t)baseIndex;
					data[indexOffset + 4] = (uint16_t)baseIndex + ChunkWidthVertices;
					data[indexOffset + 5] = (uint16_t)baseIndex + ChunkWidthVertices + 1;
				}
				else
				{
					// Triangle pattern: |\,
					data[indexOffset + 0] = (uint16_t)baseIndex;
					data[indexOffset + 1] = (uint16_t)baseIndex + ChunkWidthVertices;
					data[indexOffset + 2] = (uint16_t)baseIndex + 1;

					data[indexOffset + 3] = (uint16_t)baseIndex + 1;
					data[indexOffset + 4] = (uint16_t)baseIndex + ChunkWidthVertices;
					data[indexOffset + 5] = (uint16_t)baseIndex + ChunkWidthVertices + 1;
				}
			}
		}

	CreateStaticBuffer(renderSystem.core.device, batch,	data, D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);
	indexBuffer->SetName(L"TerrainChunkIB");
}
