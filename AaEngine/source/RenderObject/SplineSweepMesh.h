#pragma once

#include "RenderObject/ShapeProfile.h"
#include "RenderObject/Spline.h"
#include "Resources/Model/VertexBufferModel.h"
#include "Utils/MathUtils.h"

#include <cstdint>
#include <vector>

struct SplineSweepVertex
{
	Vector3 position{};
	Vector2 uv{};
	Vector3 normal = Vector3::UnitY;
	Vector3 tangent = Vector3::UnitZ;
};

struct SplineSweepMesh
{
	std::vector<SplineSweepVertex> vertices;
	std::vector<uint32_t> indices;
	BoundingBoxVolume bounds{};
	bool hasBounds = false;

	void clear();
	bool empty() const { return vertices.empty() || indices.empty(); }
};

struct SplineSweepMeshSettings
{
	float pathUvScale = 1.0f;
	float profileUvScale = 1.0f;
	float minProfileForwardScale = 0.08f;
	bool generateNormals = true;
	bool generateTangents = true;
	bool preventProfileFoldover = true;
};

class SplineSweepMeshGenerator
{
public:
	static SplineSweepMesh generate(const Spline& spline, const ShapeProfile2D& profile, const SplineSweepMeshSettings& settings = {});
	static void createVertexBufferModel(VertexBufferModel& model, ID3D12Device* device, ResourceUploadBatch* uploadBatch, const SplineSweepMesh& mesh);

private:
	static void resetVertexBufferModel(VertexBufferModel& model);
	static void appendContour(SplineSweepMesh& mesh, const std::vector<SplineSample>& samples, const ShapeProfileContour& contour, const SplineSweepMeshSettings& settings);
	static void appendQuad(SplineSweepMesh& mesh, uint32_t v00, uint32_t v01, uint32_t v10, uint32_t v11);
	static void addBoundsPoint(SplineSweepMesh& mesh, Vector3 position);
	static Vector2 preventFoldover(size_t sampleIndex, const std::vector<SplineSample>& samples, Vector2 profilePosition, const SplineSweepMeshSettings& settings);
	static float getSignedCurvature(size_t sampleIndex, const std::vector<SplineSample>& samples);
};