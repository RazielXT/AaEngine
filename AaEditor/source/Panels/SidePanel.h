#pragma once

class ApplicationCore;
class EditorSelection;
class SceneTreePanel;
class ViewportPanel;
struct DebugState;
struct ObjectTransformation;

class SidePanel
{
public:

	SidePanel(ApplicationCore& app, EditorSelection& selection, DebugState& state, SceneTreePanel& sceneTree, ViewportPanel& viewportPanel);

	void draw();

private:

	ApplicationCore& app;
	EditorSelection& selection;
	DebugState& state;
	SceneTreePanel& sceneTree;
	ViewportPanel& viewportPanel;
};
