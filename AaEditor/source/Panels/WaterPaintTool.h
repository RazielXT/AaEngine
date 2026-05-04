#pragma once

#include "Viewport/ViewportTool.h"

class ViewportPanel;
class ApplicationCore;

class WaterPaintTool : public ViewportTool
{
public:

	WaterPaintTool(ApplicationCore& app, ViewportPanel& viewport);

	enum class Mode { Add, Remove };

	void onPick(const EntityPicker::PickInfo& pickInfo, bool ctrlActive, Camera& camera) override;
	void draw(const ViewportToolContext& ctx) override;

	bool isOverlayActive() const override { return toolbarHovered; }
	bool needsScenePick() const override { return true; }

	Mode getMode() const { return mode; }

private:

	void adjustWaterLevel(const Vector3& position);

	Mode mode = Mode::Add;
	bool toolbarHovered = false;
	float brushRadius = 10.0f;
	float brushStrength = 3.0f;

	ApplicationCore& app;
	ViewportPanel& viewport;
};
