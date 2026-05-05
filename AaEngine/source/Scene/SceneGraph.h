#pragma once

#include "Scene/RenderEntity.h"
#include "Scene/RenderQueue.h"

struct SceneGraphNode
{
	ObjectId id;
	std::string name;

	std::vector<SceneGraphNode> children;
};

class RenderWorld;

class SceneGraph
{
public:

	SceneGraph(RenderWorld& renderWorld);

	void updateEntity(const std::vector<EntityChangeDescritpion>&);

	std::vector<SceneGraphNode> nodes;
	uint32_t version{};

protected:

	bool removeNode(std::vector<SceneGraphNode>& nodes, ObjectId id);

	SceneGraphNode* getParentNode(ObjectId);

	void reset();

	RenderWorld& renderWorld;
};
