#pragma once

#include "RenderObject/SplineSweep/Spline.h"
#include "RenderObject/SplineSweep/SplineSweepMesh.h"
#include "Resources/Model/VertexBufferModel.h"
#include "Utils/MathUtils.h"

#include <array>
#include <string>
#include <vector>

struct GraphicsResources;
class RenderEntity;
class RenderWorld;
class RenderSystem;

namespace DirectX
{
	class ResourceUploadBatch;
}

struct SplineDebugVisualizationSettings
{
	float pathRadius = 0.12f;
	float pointSize = 0.8f;
	float frameAxisLength = 2.0f;
	float frameAxisRadius = 0.06f;
	std::string pathMaterialName = "General";
	std::string pointMaterialName = "General";
	std::string frameMaterialName = "General";
	bool showPath = true;
	bool showPoints = true;
	bool showActivePointFrame = true;
	size_t activePointIndex = Spline::InvalidPointIndex;
};

struct SplineDebugPointEntity
{
	size_t pointIndex = Spline::InvalidPointIndex;
	RenderEntity* entity{};
};

class SplineDebugVisualization
{
public:
	void rebuild(const Spline& spline, RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, DirectX::ResourceUploadBatch& uploadBatch, const SplineDebugVisualizationSettings& settings = {});
	void removeFromWorld(RenderWorld& renderWorld);

	RenderEntity* getPathEntity() const { return pathEntity; }
	const std::vector<SplineDebugPointEntity>& getPointEntities() const { return pointEntities; }
	RenderEntity* getPointEntity(size_t pointIndex) const;
	size_t getPointIndex(RenderEntity* entity) const;

private:
	void rebuildPath(const Spline& spline, RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, DirectX::ResourceUploadBatch& uploadBatch, const SplineDebugVisualizationSettings& settings);
	void rebuildPointModel(RenderSystem& renderSystem, DirectX::ResourceUploadBatch& uploadBatch);
	void rebuildPoints(const Spline& spline, RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, DirectX::ResourceUploadBatch& uploadBatch, const SplineDebugVisualizationSettings& settings);
	void rebuildActivePointFrame(const Spline& spline, RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, DirectX::ResourceUploadBatch& uploadBatch, const SplineDebugVisualizationSettings& settings);
	void createAxisEntity(size_t axisIndex, Vector3 start, Vector3 direction, Vector3 color, const SplineDebugVisualizationSettings& settings, RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, DirectX::ResourceUploadBatch& uploadBatch);
	void resetModel(VertexBufferModel& model);

	VertexBufferModel pathModel;
	VertexBufferModel pointModel;
	std::array<VertexBufferModel, 3> frameAxisModels;
	RenderEntity* pathEntity{};
	std::vector<SplineDebugPointEntity> pointEntities;
	std::array<RenderEntity*, 3> frameAxisEntities{};
};