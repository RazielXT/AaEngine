#include "SceneGraph.h"
#include "SceneManager.h"

SceneGraph::SceneGraph(SceneManager& sm) : sceneMgr(sm)
{
	reset();
}

void SceneGraph::updateEntity(const std::vector<EntityChangeDescritpion>& changes)
{
	if (changes.empty())
		return;

	for (auto& d : changes)
	{
		if (d.type == EntityChange::DeleteAll)
		{
			reset();
		}
		else if (d.type == EntityChange::Add)
		{
			auto id = d.entity->getGlobalId();
			auto parent = getParentNode(id);
			parent->children.push_back({ id, d.entity->name });
		}
		else if (d.type == EntityChange::Delete)
		{
			auto id = d.entity->getGlobalId();
			auto& nodes = getParentNode(id)->children;

			for (auto it = nodes.begin(); it != nodes.end(); it++)
			{
				if (it->id == id)
				{
					nodes.erase(it);
					break;
				}
			}
		}
	}
}

UINT lastGroupMask{};
SceneGraphNode* lastGroupNode{};

SceneGraphNode* SceneGraph::getParentNode(ObjectId id)
{
	auto group = id.getGroupMask();

	if (lastGroupNode && group == lastGroupMask)
		return lastGroupNode;

	for (auto& node : nodes)
	{
		if (node.id.value == group)
		{
			lastGroupMask = group;
			lastGroupNode = &node;
			return lastGroupNode;
		}
	}

	nodes.push_back({ group, sceneMgr.getEntityGroup(id.getGroupId()).c_str() });
	lastGroupMask = group;
	lastGroupNode = &nodes.back();
	return lastGroupNode;
}

void SceneGraph::reset()
{
	nodes.clear();

	lastGroupMask = {};
	lastGroupNode = {};
}
