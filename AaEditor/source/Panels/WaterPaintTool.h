#pragma once

#include "Viewport/ViewportTool.h"

class WaterPaintTool : public ViewportTool
{
public:

	enum class Mode { Add, Remove };

	void onPick(const EntityPicker::PickInfo& pickInfo, bool ctrlActive, Camera& camera) override;
	void draw(const ViewportToolContext& ctx) override;

	bool isOverlayActive() const override { return toolbarHovered; }
	bool needsScenePick() const override { return true; }

	Mode getMode() const { return mode; }

private:

	Mode mode = Mode::Add;
	bool toolbarHovered = false;
	float brushRadius = 5.0f;
};
