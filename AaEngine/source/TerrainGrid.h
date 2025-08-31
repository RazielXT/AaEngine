#pragma once

#include "VertexBufferModel.h"
#include "Directx.h"

class SceneEntity;
class RenderSystem;
struct GraphicsResources;

struct TerrainChunk
{
	ComPtr<ID3D12Resource> vertexBuffer;
	VertexBufferModel model;
};

struct TerrainGrid
{
	void init(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, float worldWidth, float maxHeight);

	struct ChunkInfo
	{
		TerrainChunk* chunk{};
		SceneEntity* entity{};
	};

	struct ChunkLod
	{
		ChunkInfo data[4][4];
		bool fixBorderSeams = true;
	};

	static constexpr UINT LodsCount = 6;
	ChunkLod lods[LodsCount];

	static constexpr UINT BuildGroups = 8;
	static constexpr UINT BuildGroupVertices = 8; //last group 9
	static constexpr UINT ChunkWidthQuads = BuildGroupVertices * BuildGroups;
	static constexpr UINT ChunkWidthVertices = ChunkWidthQuads + 1; //keep odd count vertices so have even count quads
	static constexpr UINT IndexCount = 3 * 2 * (ChunkWidthVertices - 1) * (ChunkWidthVertices - 1);

private:

	std::vector<TerrainChunk> chunks;
	void createChunk(TerrainChunk& chunk, GraphicsResources& resources, const wchar_t* name, float width, float maxHeight);

	ComPtr<ID3D12Resource> indexBuffer;
	void createIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch);
};
