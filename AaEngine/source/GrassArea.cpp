#include "GrassArea.h"
#include "AaMath.h"
#include "AaEntity.h"

void GrassArea::initialize(BoundingBoxVolume extends)
{
	width = 2;
//	const float density = 1;
//	const float halfWidth = density * width / 2;

	const float heightPosition = extends.max.y;
	const Vector2 areaSize = Vector2(extends.max.x, extends.max.z) - Vector2(extends.min.x, extends.min.z);
	areaCount = { UINT(areaSize.x / width), UINT(areaSize.y / width) };

	count = areaCount.x * areaCount.y;
	vertexCount = count * 6;

	struct Vertex
	{
		Vector3 pos;
		Vector3 color;
	};
// 	std::vector<Vertex> positions;
// 	positions.reserve(count * 2);
// 
// 	for (UINT x = 0; x < areaCount.x; x++)
// 		for (UINT z = 0; z < areaCount.y; z++)
// 		{
// 			float xPos = extends.min.x + getRandomFloat(x * width - halfWidth, x * width + halfWidth);
// 			float zPos = extends.min.z + getRandomFloat(z * width - halfWidth, z * width + halfWidth);
// 
// 			float angle = getRandomAngleInRadians();
// 			float xTrans = cos(angle) * halfWidth;
// 			float zTrans = sin(angle) * halfWidth;
// 
// 			float x1 = xPos - xTrans, z1 = zPos - zTrans;
// 			float x2 = xPos + xTrans, z2 = zPos + zTrans;
// 
// 			positions.emplace_back(Vector3{ x1, heightPosition, z1 });
// 			positions.emplace_back(Vector3{ x2, heightPosition, z2 });
// 		}

	gpuBuffer = ShaderConstantBuffers::get().CreateStructuredBuffer(sizeof(Vertex) * count * 2);
	gpuBuffer->SetName(L"GrassArea");

	vertexCounter = ShaderConstantBuffers::get().CreateStructuredBuffer(sizeof(UINT));
	vertexCounter->SetName(L"GrassAreaVertexCounter");	
	vertexCounterRead = ShaderConstantBuffers::get().CreateStructuredBuffer(sizeof(UINT), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
	vertexCounterRead->SetName(L"GrassAreaVertexCounterRead");

// 	BoundingBoxVolume volume(Vector3(positions.front().pos.x, positions.front().pos.y, positions.front().pos.z));
// 
// 	for (auto& p : positions)
// 	{
// 		volume.add({ p.pos.x, p.pos.y, p.pos.z });
// 	}
// 
// 	bbox = volume.createBbox();

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

GrassArea* GrassManager::addGrass(BoundingBoxVolume extends)
{
	auto grass = new GrassArea();
	grass->initialize(extends);

	return grasses.emplace_back(grass);
}

void GrassManager::bakeTerrain(GrassArea& grass, AaEntity* terrain, XMMATRIX invVP, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	grassCS.dispatch(commandList, grass, 24, 26, invVP, ResourcesManager::get(), frameIndex);
}

void GrassManager::clear()
{
	for (auto g : grasses)
	{
		delete g;
	}
	grasses.clear();
}
