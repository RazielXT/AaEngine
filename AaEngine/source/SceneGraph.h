#pragma once

#include "SceneEntity.h"
#include "RenderQueue.h"

struct SceneGraphNode
{
	ObjectId id;
	const char* name;

	std::vector<SceneGraphNode> children;
};

class SceneManager;

class SceneGraph
{
public:

	SceneGraph(SceneManager& sceneMgr);

	void updateEntity(const std::vector<EntityChangeDescritpion>&);

	std::vector<SceneGraphNode> nodes;
	uint32_t version{};

protected:

	SceneGraphNode* getParentNode(ObjectId);

	void reset();

	SceneManager& sceneMgr;
};
