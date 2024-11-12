#include "GrassArea.h"
#include "AaMath.h"
#include "AaEntity.h"

void GrassAreaDescription::initialize(BoundingBoxVolume extends)
{
	const Vector2 areaSize = Vector2(extends.max.x, extends.max.z) - Vector2(extends.min.x, extends.min.z);
	areaCount = { UINT(areaSize.x / spacing), UINT(areaSize.y / spacing) };

	count = areaCount.x * areaCount.y;
	vertexCount = count * 6;

// 	struct Vertex
// 	{
// 		Vector3 pos;
// 		Vector3 color;
// 	};
// 
// 	gpuBuffer = ShaderConstantBuffers::get().CreateStructuredBuffer(sizeof(Vertex) * count * 2);
// 	gpuBuffer->SetName(L"GrassArea");
// 
// 	vertexCounter = ShaderConstantBuffers::get().CreateStructuredBuffer(sizeof(UINT));
// 	vertexCounter->SetName(L"GrassAreaVertexCounter");	
// 	vertexCounterRead = ShaderConstantBuffers::get().CreateStructuredBuffer(sizeof(UINT), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
// 	vertexCounterRead->SetName(L"GrassAreaVertexCounterRead");

	bbox = extends.createBbox();
}

GrassManager::GrassManager(AaRenderSystem* rs)
{
	grassCS.init(rs->device, "grassInit");
}

GrassManager::~GrassManager()
{
	clear();
}

GrassArea* GrassManager::createGrass(const GrassAreaDescription& desc)
{
	auto grass = new GrassArea();

	struct Vertex
	{
		Vector3 pos;
		Vector3 color;
	};

	grass->gpuBuffer = ShaderConstantBuffers::get().CreateStructuredBuffer(sizeof(Vertex) * desc.count * 2);
	grass->gpuBuffer->SetName(L"GrassArea");

	grass->vertexCounter = ShaderConstantBuffers::get().CreateStructuredBuffer(sizeof(UINT));
	grass->vertexCounter->SetName(L"GrassAreaVertexCounter");
	grass->vertexCounterRead = ShaderConstantBuffers::get().CreateStructuredBuffer(sizeof(UINT), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
	grass->vertexCounterRead->SetName(L"GrassAreaVertexCounterRead");

	return grasses.emplace_back(grass);
}

void GrassManager::initializeGrassBuffer(GrassArea& grass, GrassAreaDescription& desc, XMMATRIX invView, UINT colorTex, UINT depthTex, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	grassCS.dispatch(commandList, desc, invView, colorTex, depthTex, grass.gpuBuffer.Get(), grass.vertexCounter.Get(), frameIndex);
}

void GrassManager::clear()
{
	for (auto g : grasses)
	{
		delete g;
	}
	grasses.clear();
}
