#include "EditorSelection.h"
#include "Editor/EntityPicker.h"

void EditorSelection::select(ObjectId selectedId, bool multi, RenderWorld& renderWorld)
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
		if (auto obj = renderWorld.getObject(selectedId))
		{
			items.emplace_back(obj.getTransformation(), obj);
		}
	}

	refreshIds();
}

void EditorSelection::remove(RenderWorld& renderWorld)
{
	for (auto& s : items)
	{
		renderWorld.removeEntity(s.obj.entity);
	}

	clear();
}

void EditorSelection::removeNode(SceneGraphNode& node, RenderWorld& renderWorld)
{
	for (auto& child : node.children)
	{
		renderWorld.removeEntity(child.id);
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
