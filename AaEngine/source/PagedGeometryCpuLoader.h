#pragma once

#include "PagedGeometryLoader.h"
#include "RenderObject.h"
#include "GraphicsResources.h"

#include "SpatialGrid/SpatialGrid.h"
#include "SpatialGrid/GlobalBucketProvider.h"

class SceneManager;

class PagedGeometryCpuLoader : public PagedGeometryLoader
{
public:

	PagedGeometryCpuLoader(SceneManager& mgr);
	~PagedGeometryCpuLoader() = default;

	void initialize(GraphicsResources&, ResourceUploadBatch& batch);

	void clear();

	ObjectId addTree(const ObjectTransformation&);

	void updateEntity(ObjectId id, const ObjectTransformation& transform);

private:

	GlobalBucketProvider buckets;

	SpatialGrid grid;

	SceneManager& sceneMgr;
};
