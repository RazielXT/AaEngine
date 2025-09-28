#include "PagedGeometryCpuLoader.h"
#include "Directx.h"

PagedGeometryCpuLoader::PagedGeometryCpuLoader(SceneManager& mgr) : grid(buckets, 100.f, 1024), sceneMgr(mgr)
{

}

ObjectId PagedGeometryCpuLoader::addTree(const ObjectTransformation& tr)
{
	// get grid bucket

	const UINT MaxItemsPerDraw = 100;

	// if more than threshold or empty create another
	//		create entity

	// add tree, schedule update

	return {};
}

XM_ALIGNED_STRUCT(16) InstanceData
{
	XMFLOAT4X4 transform;
};
