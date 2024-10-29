#include "GrassArea.h"
#include "AaMath.h"

void GrassArea::initialize()
{
	count = 100;
	std::vector<Vector4> positions;
	positions.reserve(count * 2);

	for (UINT i = 0; i < count; i++)
	{
		positions.emplace_back(Vector4{ float(i * 10), 10, 0, 1 });
		positions.emplace_back(Vector4{ float(i * 10), 10, 4, 1 });
	}

	cbuffer = ShaderConstantBuffers::get().CreateCbufferResource(sizeof(Vector4) * positions.size(), "GrassPositionsBuffer");

	for (auto& b : cbuffer.data)
	{
		memcpy(b->Memory(), positions.data(), sizeof(Vector4) * positions.size());
	}

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
