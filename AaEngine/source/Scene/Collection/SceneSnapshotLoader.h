#pragma once

#include "Scene/Collection/SceneSnapshot.h"
#include "Scene/Collection/SceneCollection.h"

namespace SceneSnapshotLoader
{
	void load(const SceneSnapshot& snapshot, SceneCollection::LoadCtx ctx);
};
