#include "GrassArea.h"
#include "AaMath.h"

void GrassArea::initialize(BoundingBoxVolume extends)
{
	const float width = 2;
	const float density = 1;
	const float halfWidth = density * width / 2;

	const float heightPosition = extends.max.y;
	const Vector2 areaSize = Vector2(extends.max.x, extends.max.z) - Vector2(extends.min.x, extends.min.z);
	const XMUINT2 areaCount = { UINT(areaSize.x / width), UINT(areaSize.y / width) };

	count = areaCount.x * areaCount.y;

	std::vector<Vector4> positions;
	positions.reserve(count * 2);

	for (UINT x = 0; x < areaCount.x; x++)
		for (UINT z = 0; z < areaCount.y; z++)
		{
			float xPos = extends.min.x + getRandomFloat(x * width - halfWidth, x * width + halfWidth);
			float zPos = extends.min.z + getRandomFloat(z * width - halfWidth, z * width + halfWidth);

			float angle = getRandomAngleInRadians();
			float xTrans = cos(angle) * halfWidth;
			float zTrans = sin(angle) * halfWidth;

			float x1 = xPos - xTrans, z1 = zPos - zTrans;
			float x2 = xPos + xTrans, z2 = zPos + zTrans;

			positions.emplace_back(Vector4{ x1, heightPosition, z1, 1 });
			positions.emplace_back(Vector4{ x2, heightPosition, z2, 1 });
		}

	gpuBuffer = ShaderConstantBuffers::get().CreateStructuredBuffer(positions.data(), sizeof(Vector4) * positions.size());

	BoundingBoxVolume volume;
	for (auto& p : positions)
	{
		volume.add({ p.x, p.y, p.z });
	}

	bbox = volume.createBbox();
}

UINT GrassArea::getVertexCount() const
{
	return count * 6;
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

void GrassManager::clear()
{
	for (auto g : grasses)
	{
		delete g;
	}
	grasses.clear();
}
