#include "Scene/SceneGraph.h"
#include "Scene/SceneManager.h"
#include <format>

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
			auto parent = getParentNode(d.id);
			parent->children.push_back({ d.id, std::format("{:08X}", d.id.value) });
		}
		else if (d.type == EntityChange::Delete)
		{
			removeNode(nodes, d.id);
		}
	}
}

bool SceneGraph::removeNode(std::vector<SceneGraphNode>& nodes, ObjectId id)
{
	bool removed = false;

	for (auto it = nodes.begin(); it != nodes.end(); it++)
	{
		if (it->id == id || (removeNode(it->children, id) && it->children.empty()))
		{
			nodes.erase(it);
			return true;
		}
	}

	return false;
}

static UINT lastGroupMask{};
static SceneGraphNode* lastGroupNode{};

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

	nodes.push_back({ group, "group" });
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
