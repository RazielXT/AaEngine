#pragma once

#include "Scene/SceneEntity.h"
#include "Scene/SceneManager.h"

class TerrainScanScheduler
{
public:

	struct TerrainScanTask
	{
		SceneEntity* terrain{};
		BoundingBox bbox;
	};

	void scheduleScan(TerrainScanTask task, ID3D12GraphicsCommandList* commandList, const FrameParameters& frame, GraphicsResources& resources, SceneManager& sceneMgr);
};