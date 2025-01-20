#pragma once

#include "VertexBufferModel.h"
#include "Directx.h"

class SceneManager;
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
	void init(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);

	struct ChunkInfo
	{
		TerrainChunk* chunk{};
		SceneEntity* entity{};
	};

	struct ChunkLod
	{
		ChunkInfo data[4][4];
		bool borderSeamsFix = true;
	};

	ChunkLod lod0;
	ChunkLod lod1;
	ChunkLod lod2;

	const UINT BuildThreads = 16;
	const UINT ChunkSize = BuildThreads * 4 - 1;
	const UINT IndexCount = 3 * 2 * (ChunkSize - 1) * (ChunkSize - 1);

private:

	std::vector<TerrainChunk> chunks;
	void createChunk(TerrainChunk& chunk, GraphicsResources& resources, const wchar_t* name, float width);

	ComPtr<ID3D12Resource> indexBuffer;
	void createIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch);
};
