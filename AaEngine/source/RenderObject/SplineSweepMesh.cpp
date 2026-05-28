#include "RenderObject/SplineSweepMesh.h"

#include <algorithm>

namespace
{
	Vector3 normalizeOrFallback(Vector3 value, Vector3 fallback)
	{
		if (value.LengthSquared() <= 0.000001f)
			return fallback;

		value.Normalize();
		return value;
	}
}

void SplineSweepMesh::clear()
{
	vertices.clear();
	indices.clear();
	bounds = {};
	hasBounds = false;
}

SplineSweepMesh SplineSweepMeshGenerator::generate(const Spline& spline, const ShapeProfile2D& profile, const SplineSweepMeshSettings& settings)
{
	SplineSweepMesh mesh;
	const auto& samples = spline.getSamples();
	if (samples.size() < 2 || profile.empty())
		return mesh;

	for (const ShapeProfileContour& contour : profile.getContours())
		appendContour(mesh, samples, contour, settings);

	return mesh;
}

void SplineSweepMeshGenerator::createVertexBufferModel(VertexBufferModel& model, ID3D12Device* device, ResourceUploadBatch* uploadBatch, const SplineSweepMesh& mesh)
{
	resetVertexBufferModel(model);
	if (mesh.vertices.empty())
		return;

	model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION);
	model.addLayoutElement(DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD);
	model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL);
	model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::TANGENT);

	model.CreateVertexBuffer(device, uploadBatch, mesh.vertices.data(), static_cast<UINT>(mesh.vertices.size()), sizeof(SplineSweepVertex));
	if (!mesh.indices.empty())
		model.CreateIndexBuffer(device, uploadBatch, mesh.indices.data(), mesh.indices.size());
	if (mesh.hasBounds)
		model.bbox = mesh.bounds.createBbox();
}

void SplineSweepMeshGenerator::resetVertexBufferModel(VertexBufferModel& model)
{
	if (model.owner)
	{
		if (model.vertexBuffer)
			model.vertexBuffer->Release();
		if (model.indexBuffer)
			model.indexBuffer->Release();
	}

	model.vertexBuffer = nullptr;
	model.indexBuffer = nullptr;
	model.vertexBufferView = {};
	model.indexBufferView = {};
	model.vertexCount = 0;
	model.indexCount = 0;
	model.vertexLayout.clear();
	model.positions.clear();
	model.indices.clear();
	model.owner = true;
}

void SplineSweepMeshGenerator::appendContour(SplineSweepMesh& mesh, const std::vector<SplineSample>& samples, const ShapeProfileContour& contour, const SplineSweepMeshSettings& settings)
{
	if (contour.points.size() < 2)
		return;

	const uint32_t baseVertex = static_cast<uint32_t>(mesh.vertices.size());
	const uint32_t profileVertexCount = static_cast<uint32_t>(contour.points.size());
	const uint32_t pathVertexCount = static_cast<uint32_t>(samples.size());

	mesh.vertices.reserve(mesh.vertices.size() + samples.size() * contour.points.size());
	for (const SplineSample& sample : samples)
	{
		for (const ShapeProfilePoint& point : contour.points)
		{
			SplineSweepVertex vertex;
			vertex.position = sample.position + sample.right * point.position.x + sample.up * point.position.y;
			vertex.uv = { point.profileDistance * settings.profileUvScale, sample.distance * settings.pathUvScale };
			if (settings.generateNormals)
				vertex.normal = normalizeOrFallback(sample.right * point.normal.x + sample.up * point.normal.y, sample.up);
			if (settings.generateTangents)
				vertex.tangent = sample.tangent;

			addBoundsPoint(mesh, vertex.position);
			mesh.vertices.push_back(vertex);
		}
	}

	const uint32_t profileEdgeCount = contour.closed ? profileVertexCount : profileVertexCount - 1;
	mesh.indices.reserve(mesh.indices.size() + (pathVertexCount - 1) * profileEdgeCount * 6);

	for (uint32_t pathIndex = 0; pathIndex + 1 < pathVertexCount; ++pathIndex)
	{
		for (uint32_t profileIndex = 0; profileIndex < profileEdgeCount; ++profileIndex)
		{
			const uint32_t nextProfileIndex = (profileIndex + 1) % profileVertexCount;
			const uint32_t v00 = baseVertex + pathIndex * profileVertexCount + profileIndex;
			const uint32_t v01 = baseVertex + pathIndex * profileVertexCount + nextProfileIndex;
			const uint32_t v10 = baseVertex + (pathIndex + 1) * profileVertexCount + profileIndex;
			const uint32_t v11 = baseVertex + (pathIndex + 1) * profileVertexCount + nextProfileIndex;
			appendQuad(mesh, v00, v01, v10, v11);
		}
	}
}

void SplineSweepMeshGenerator::appendQuad(SplineSweepMesh& mesh, uint32_t v00, uint32_t v01, uint32_t v10, uint32_t v11)
{
	const Vector3 edge = mesh.vertices[v01].position - mesh.vertices[v00].position;
	const Vector3 path = mesh.vertices[v10].position - mesh.vertices[v00].position;
	Vector3 faceNormal = edge.Cross(path);
	faceNormal = normalizeOrFallback(faceNormal, mesh.vertices[v00].normal);

	const Vector3 desiredNormal = normalizeOrFallback(mesh.vertices[v00].normal + mesh.vertices[v01].normal + mesh.vertices[v10].normal + mesh.vertices[v11].normal, mesh.vertices[v00].normal);
	if (faceNormal.Dot(desiredNormal) >= 0.0f)
	{
		mesh.indices.push_back(v00);
		mesh.indices.push_back(v01);
		mesh.indices.push_back(v10);
		mesh.indices.push_back(v01);
		mesh.indices.push_back(v11);
		mesh.indices.push_back(v10);
	}
	else
	{
		mesh.indices.push_back(v00);
		mesh.indices.push_back(v10);
		mesh.indices.push_back(v01);
		mesh.indices.push_back(v01);
		mesh.indices.push_back(v10);
		mesh.indices.push_back(v11);
	}
}

void SplineSweepMeshGenerator::addBoundsPoint(SplineSweepMesh& mesh, Vector3 position)
{
	if (!mesh.hasBounds)
	{
		mesh.bounds = BoundingBoxVolume(position);
		mesh.hasBounds = true;
		return;
	}

	mesh.bounds.add(position);
}