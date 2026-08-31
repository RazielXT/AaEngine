#include "AssetPlacementOperation.h"

#include "ApplicationCore.h"
#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "RenderObject/Prefab/PrefabLoader.h"
#include "Resources/Model/GltfLoader.h"
#include "Scene/Camera.h"
#include "Scene/Collection/SceneCollection.h"
#include "SceneParser.h"

#include <ResourceUploadBatch.h>

namespace
{
	void placeAsset(ApplicationCore& app, const std::string& assetPath, const EntityPicker::PickInfo& pickInfo, Camera& camera)
	{
		ObjectTransformation transform;
		if (pickInfo.id)
		{
			transform.position = pickInfo.position;
			transform.position.y += 2.0f;
		}
		else
		{
			transform.position = camera.getPosition() + camera.getCameraDirection() * 50.0f;
		}

		ResourceUploadBatch batch(app.renderSystem.core.device);
		batch.Begin();

		if (assetPath.ends_with(".gltf"))
		{
			SceneCollection::LoadCtx loadContext = { batch, app.renderWorld, app.renderSystem, app.resources, transform };
			auto scene = GltfLoader::load(assetPath, loadContext);
			SceneCollection::loadResource(scene, loadContext);
		}
		else if (assetPath.ends_with(".scene"))
		{
			SceneParser::load(assetPath, { batch, app.renderWorld, app.renderSystem, app.resources, app.physicsMgr, transform });
		}
		else if (assetPath.ends_with(".prefab"))
		{
			PrefabLoader::load(assetPath, { batch, app.renderWorld, app.renderSystem, app.resources, transform });
		}

		batch.End(app.renderSystem.core.commandQueue).wait();
		VoxelizeSceneTask::Get().revoxelize();
	}
}

ViewportPlacementRequest createAssetPlacementRequest(ApplicationCore& app, std::string assetPath)
{
	ViewportPlacementRequest request;
	request.hint = "Place asset";
	request.place = [&app, assetPath = std::move(assetPath)](const EntityPicker::PickInfo& pickInfo, Camera& camera)
	{
		placeAsset(app, assetPath, pickInfo, camera);
	};
	return request;
}