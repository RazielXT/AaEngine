#include "EditorSelection.h"
#include "Editor/EntityPicker.h"

void EditorSelection::select(ObjectId selectedId, bool multi, SceneManager& sceneMgr)
{
	bool add = true;

	if (!multi)
	{
		items.clear();
	}
	else
	{
		for (auto it = items.begin(); it != items.end(); it++)
		{
			if (it->obj.id == selectedId)
			{
				items.erase(it);
				add = false;
				break;
			}
		}
	}

	if (add)
	{
		if (auto obj = sceneMgr.getObject(selectedId))
		{
			items.emplace_back(obj.getTransformation(), obj);
		}
	}

	refreshIds();
}

void EditorSelection::remove(SceneManager& sceneMgr)
{
	for (auto& s : items)
	{
		sceneMgr.removeEntity(s.obj.entity);
	}

	clear();
}

void EditorSelection::removeNode(SceneGraphNode& node, SceneManager& sceneMgr)
{
	for (auto& child : node.children)
	{
		sceneMgr.removeEntity(child.id);
	}

	clear();
}

void EditorSelection::clear()
{
	items.clear();
	refreshIds();
}

void EditorSelection::refreshIds()
{
	ids.clear();
	for (auto& s : items)
	{
		ids.push_back(s.obj.id);
	}

	EntityPicker::Get().active = ids;
}
