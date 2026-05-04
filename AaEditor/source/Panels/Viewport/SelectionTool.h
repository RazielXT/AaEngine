#pragma once

#include "ViewportTool.h"
#include "imgui.h"
#include <vector>

class EditorSelection;
class ApplicationCore;

class SelectionTool : public ViewportTool
{
public:

	SelectionTool(ApplicationCore& app, EditorSelection& selection);

	void onPick(const EntityPicker::PickInfo& pickInfo, bool ctrlActive, Camera& camera) override;
	void draw(const ViewportToolContext& ctx) override;

	bool isInteracting() const override { return gizmoActive; }
	bool isOverlayActive() const override { return overlayActive; }
	void cancel() override { gizmoReset = true; }
	void reset() override;

	// Drawn always by ViewportPanel regardless of active tool
	bool drawModeIcon(bool primary);

	void setAssetDrop(const std::string& asset) { assetDrop = asset; }

private:

	ApplicationCore& app;
	EditorSelection& selection;

	bool gizmoActive = false;
	bool gizmoReset = false;
	bool gizmoResetCache = false;
	bool overlayActive = false;
	std::string assetDrop;
};
