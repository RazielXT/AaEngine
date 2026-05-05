#pragma once

#include "Scene/RenderWorld.h"
#include "imgui.h"

class EditorSelection;

class SceneTreePanel
{
public:

	SceneTreePanel(RenderWorld& renderWorld, EditorSelection& selection);

	void draw();

private:

	void drawNode(SceneGraphNode& node, ImGuiTextFilter& filter, ObjectId& selectedObjectId);

	RenderWorld& renderWorld;
	EditorSelection& selection;
};
