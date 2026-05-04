#pragma once

#include "Scene/RenderObject.h"
#include "Editor/EntityPicker.h"
#include <map>
#include <string>

class Camera;

struct ViewportToolContext
{
	XMUINT2 viewportBounds[2];
	Camera& camera;
};

class ViewportTool
{
public:

	virtual ~ViewportTool() = default;

	// Called when a viewport click occurs with the pick result ready
	virtual void onPick(const EntityPicker::PickInfo& pickInfo, bool ctrlActive, Camera& camera) = 0;

	// Called every frame to draw toolbar and overlays inside the viewport
	virtual void draw(const ViewportToolContext& ctx) = 0;

	// Whether the tool is currently consuming mouse input (blocks camera/click)
	virtual bool isInteracting() const { return false; }

	// Whether mouse is over tool overlay (blocks viewport click)
	virtual bool isOverlayActive() const { return false; }

	// Whether to schedule entity picker on click
	virtual bool needsScenePick() const { return true; }

	// Called on right-click cancel
	virtual void cancel() {}

	// Called on reset
	virtual void reset() {}
};
