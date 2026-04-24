#pragma once

class ApplicationCore;
class EditorSelection;
class SceneTreePanel;
struct DebugState;
struct ObjectTransformation;

class SidePanel
{
public:

	SidePanel(ApplicationCore& app, EditorSelection& selection, DebugState& state, SceneTreePanel& sceneTree);

	void draw(const ObjectTransformation& objTransformation);

private:

	ApplicationCore& app;
	EditorSelection& selection;
	DebugState& state;
	SceneTreePanel& sceneTree;
};
