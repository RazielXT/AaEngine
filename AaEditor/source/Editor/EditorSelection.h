#pragma once

#include "Scene/SceneManager.h"
#include <vector>

class EditorSelection
{
public:

	struct Entry
	{
		ObjectTransformation origin;
		SceneObject obj;
	};

	void select(ObjectId id, bool multi, SceneManager& sceneMgr);
	void remove(SceneManager& sceneMgr);
	void removeNode(SceneGraphNode& node, SceneManager& sceneMgr);
	void clear();

	bool empty() const { return items.empty(); }
	size_t size() const { return items.size(); }

	Entry& back() { return items.back(); }
	const Entry& back() const { return items.back(); }

	auto begin() { return items.begin(); }
	auto end() { return items.end(); }
	auto begin() const { return items.begin(); }
	auto end() const { return items.end(); }

	const std::vector<ObjectId>& getIds() const { return ids; }

private:

	std::vector<Entry> items;
	std::vector<ObjectId> ids;

	void refreshIds();
};
