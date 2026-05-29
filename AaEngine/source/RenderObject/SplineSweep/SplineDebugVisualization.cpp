#include "RenderObject/SplineSweep/SplineDebugVisualization.h"

#include "Resources/GraphicsResources.h"
#include "Resources/Material/MaterialResources.h"
#include "Resources/Model/VertexBufferModelGarbageCollector.h"
#include "Scene/RenderEntity.h"
#include "Scene/RenderWorld.h"

namespace
{
	struct DebugVertex
	{
		Vector3 position{};
		Vector2 uv{};
		Vector3 normal = Vector3::UnitY;
		Vector3 tangent = Vector3::UnitX;
	};

	Vector3 normalizeOrFallback(Vector3 value, Vector3 fallback)
	{
		if (value.LengthSquared() <= 0.000001f)
			return fallback;

		value.Normalize();
		return value;
	}
}

void SplineDebugVisualization::rebuild(const Spline& spline, RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, DirectX::ResourceUploadBatch& uploadBatch, const SplineDebugVisualizationSettings& settings)
{
	removeFromWorld(renderWorld);

	if (settings.showPath)
		rebuildPath(spline, renderSystem, resources, renderWorld, uploadBatch, settings);
	if (settings.showPoints)
		rebuildPoints(spline, renderSystem, resources, renderWorld, uploadBatch, settings);
	if (settings.showActivePointFrame)
		rebuildActivePointFrame(spline, renderSystem, resources, renderWorld, uploadBatch, settings);
}

void SplineDebugVisualization::removeFromWorld(RenderWorld& renderWorld)
{
	if (pathEntity)
		renderWorld.removeEntity(pathEntity);
	pathEntity = nullptr;

	for (SplineDebugPointEntity& pointEntity : pointEntities)
		if (pointEntity.entity)
			renderWorld.removeEntity(pointEntity.entity);
	pointEntities.clear();

	for (RenderEntity*& entity : frameAxisEntities)
	{
		if (entity)
			renderWorld.removeEntity(entity);
		entity = nullptr;
	}
}

RenderEntity* SplineDebugVisualization::getPointEntity(size_t pointIndex) const
{
	for (const SplineDebugPointEntity& pointEntity : pointEntities)
		if (pointEntity.pointIndex == pointIndex)
			return pointEntity.entity;

	return nullptr;
}

size_t SplineDebugVisualization::getPointIndex(RenderEntity* entity) const
{
	for (const SplineDebugPointEntity& pointEntity : pointEntities)
		if (pointEntity.entity == entity)
			return pointEntity.pointIndex;

	return Spline::InvalidPointIndex;
}

void SplineDebugVisualization::rebuildPath(const Spline& spline, RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, DirectX::ResourceUploadBatch& uploadBatch, const SplineDebugVisualizationSettings& settings)
{
	if (spline.getPointCount() < 2)
		return;

	ShapeProfile2D profile = ShapeProfile2D::createCircle(settings.pathRadius, 8);
	SplineSweepMeshSettings meshSettings;
	meshSettings.generateEndCaps = false;
	SplineSweepMesh mesh = SplineSweepMeshGenerator::generate(spline, profile, meshSettings);
	if (mesh.empty())
		return;

	SplineSweepMeshGenerator::createVertexBufferModel(pathModel, renderSystem.core.device, &uploadBatch, mesh);
	pathEntity = renderWorld.createEntity();
	pathEntity->geometry.fromModel(pathModel);
	pathEntity->material = resources.materials.getMaterial(settings.pathMaterialName, uploadBatch);
	pathEntity->setBoundingBox(pathModel.bbox);
	pathEntity->setTransformation({}, true);
	pathEntity->Material().setParam("MaterialColor", Vector3{ 1.0f, 0.85f, 0.15f });
}

void SplineDebugVisualization::rebuildPointModel(RenderSystem& renderSystem, DirectX::ResourceUploadBatch& uploadBatch)
{
	resetModel(pointModel);

	std::vector<Vector3> positions = {
		{ -0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, -0.5f }, { -0.5f, 0.5f, -0.5f },
		{ -0.5f, -0.5f, 0.5f }, { 0.5f, -0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f }, { -0.5f, 0.5f, 0.5f },
	};

	std::vector<DebugVertex> vertices;
	vertices.reserve(positions.size());
	for (Vector3 position : positions)
		vertices.push_back({ position, {}, normalizeOrFallback(position, Vector3::UnitY), Vector3::UnitX });

	std::vector<uint32_t> indices = {
		0, 2, 1, 0, 3, 2,
		4, 5, 6, 4, 6, 7,
		0, 1, 5, 0, 5, 4,
		3, 6, 2, 3, 7, 6,
		1, 2, 6, 1, 6, 5,
		0, 4, 7, 0, 7, 3,
	};

	pointModel.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION);
	pointModel.addLayoutElement(DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD);
	pointModel.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL);
	pointModel.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::TANGENT);
	pointModel.CreateVertexBuffer(renderSystem.core.device, &uploadBatch, vertices.data(), static_cast<UINT>(vertices.size()), sizeof(DebugVertex));
	pointModel.CreateIndexBuffer(renderSystem.core.device, &uploadBatch, indices.data(), indices.size());
	pointModel.calculateBounds();
}

void SplineDebugVisualization::rebuildPoints(const Spline& spline, RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, DirectX::ResourceUploadBatch& uploadBatch, const SplineDebugVisualizationSettings& settings)
{
	if (!pointModel.vertexCount)
		rebuildPointModel(renderSystem, uploadBatch);

	pointEntities.reserve(spline.getPointCount());
	for (size_t i = 0; i < spline.getPointCount(); ++i)
	{
		RenderEntity* entity = renderWorld.createEntity();
		entity->geometry.fromModel(pointModel);
		entity->material = resources.materials.getMaterial(settings.pointMaterialName, uploadBatch);
		entity->setBoundingBox(pointModel.bbox);
		entity->setPosition(spline.getPoint(i).position);
		entity->setScale(Vector3(settings.pointSize));
		entity->Material().setParam("MaterialColor", Vector3{ 1.0f, 0.2f, 0.1f });
		pointEntities.push_back({ i, entity });
	}
}

void SplineDebugVisualization::rebuildActivePointFrame(const Spline& spline, RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, DirectX::ResourceUploadBatch& uploadBatch, const SplineDebugVisualizationSettings& settings)
{
	if (settings.activePointIndex == Spline::InvalidPointIndex || settings.activePointIndex >= spline.getPointCount())
		return;

	const float normalizedT = spline.getSegmentCount() > 0 ? static_cast<float>(settings.activePointIndex) / static_cast<float>(spline.getSegmentCount()) : 0.0f;
	const SplineSample sample = spline.evaluate(normalizedT);
	createAxisEntity(0, sample.position, sample.right, { 1.0f, 0.1f, 0.1f }, settings, renderSystem, resources, renderWorld, uploadBatch);
	createAxisEntity(1, sample.position, sample.up, { 0.1f, 1.0f, 0.1f }, settings, renderSystem, resources, renderWorld, uploadBatch);
	createAxisEntity(2, sample.position, sample.tangent, { 0.1f, 0.35f, 1.0f }, settings, renderSystem, resources, renderWorld, uploadBatch);
}

void SplineDebugVisualization::createAxisEntity(size_t axisIndex, Vector3 start, Vector3 direction, Vector3 color, const SplineDebugVisualizationSettings& settings, RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, DirectX::ResourceUploadBatch& uploadBatch)
{
	direction = normalizeOrFallback(direction, Vector3::UnitZ);

	Spline axisSpline;
	axisSpline.setInterpolationMode(SplineInterpolationMode::Linear);
	axisSpline.setTessellationSegments(1);
	axisSpline.addPoint(start);
	axisSpline.addPoint(start + direction * settings.frameAxisLength);

	SplineSweepMeshSettings meshSettings;
	meshSettings.generateEndCaps = true;
	SplineSweepMesh mesh = SplineSweepMeshGenerator::generate(axisSpline, ShapeProfile2D::createCircle(settings.frameAxisRadius, 8), meshSettings);
	if (mesh.empty())
		return;

	SplineSweepMeshGenerator::createVertexBufferModel(frameAxisModels[axisIndex], renderSystem.core.device, &uploadBatch, mesh);
	RenderEntity* entity = renderWorld.createEntity();
	entity->geometry.fromModel(frameAxisModels[axisIndex]);
	entity->material = resources.materials.getMaterial(settings.frameMaterialName, uploadBatch);
	entity->setBoundingBox(frameAxisModels[axisIndex].bbox);
	entity->setTransformation({}, true);
	entity->Material().setParam("MaterialColor", color);
	frameAxisEntities[axisIndex] = entity;
}

void SplineDebugVisualization::resetModel(VertexBufferModel& model)
{
	if (model.owner)
	{
		if (model.vertexBuffer)
			VertexBufferModelGarbageCollector::Get().enqueue(model.vertexBuffer);
		if (model.indexBuffer)
			VertexBufferModelGarbageCollector::Get().enqueue(model.indexBuffer);
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