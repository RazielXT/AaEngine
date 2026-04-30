#include "SceneSnapshotLoader.h"
#include "Resources/Model/GltfLoader.h"
#include "App/Directories.h"

void SceneSnapshotLoader::load(const SceneSnapshot& snapshot, SceneCollection::LoadCtx ctx)
{
	std::vector<SceneCollection::ResourceData> allResources;

	for (auto& snapshotResource : snapshot.resources)
	{
		auto resourceData = GltfLoader::load(SCENE_DIRECTORY + snapshotResource.file, ctx);

		for (auto& entity : resourceData.entities)
		{
			for (auto& snapshotEntity : snapshotResource.entities)
			{
				if (entity.name == snapshotEntity.name)
				{
					entity.tr = snapshotEntity.tr;
					break;
				}
			}
		}

		allResources.push_back(std::move(resourceData));
	}

	SceneCollection::loadResources(allResources, ctx);
}
