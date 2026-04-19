#pragma once

#include "Scene/SceneEntity.h"
#include "Scene/RenderQueue.h"

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

	bool removeNode(std::vector<SceneGraphNode>& nodes, ObjectId id);

	SceneGraphNode* getParentNode(ObjectId);

	void reset();

	SceneManager& sceneMgr;
};
