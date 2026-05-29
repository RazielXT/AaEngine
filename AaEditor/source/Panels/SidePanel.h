#pragma once

#include "SidePanelSections/SkySection.h"
#include "SidePanelSections/SplineRoadSection.h"
#include "SidePanelSections/TextureOverlaySection.h"
#include "SidePanelSections/VctSection.h"

#include <memory>

class ApplicationCore;
class EditorSelection;
class SceneTreePanel;
class SplineRoadTool;
class ViewportPanel;
struct DebugState;
struct ObjectTransformation;

class SidePanel
{
public:

	SidePanel(ApplicationCore& app, EditorSelection& selection, DebugState& state, SceneTreePanel& sceneTree, ViewportPanel& viewportPanel);
	~SidePanel();

	void draw();

private:

	ApplicationCore& app;
	EditorSelection& selection;
	DebugState& state;
	SceneTreePanel& sceneTree;
	ViewportPanel& viewportPanel;

	TextureOverlaySection textureOverlaySection;
	VctSection vctSection;
	SkySection skySection;
	SplineRoadSection splineRoadSection;
	std::unique_ptr<SplineRoadTool> splineRoadTool;
};
