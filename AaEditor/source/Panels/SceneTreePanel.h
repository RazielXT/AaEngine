#pragma once

#include "Scene/SceneManager.h"
#include "imgui.h"

class EditorSelection;

class SceneTreePanel
{
public:

	SceneTreePanel(SceneManager& sceneMgr, EditorSelection& selection);

	void draw();

private:

	void drawNode(SceneGraphNode& node, ImGuiTextFilter& filter, ObjectId& selectedObjectId);

	SceneManager& sceneMgr;
	EditorSelection& selection;
};
