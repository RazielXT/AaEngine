#pragma once

class SplineRoadTool;
struct ViewportToolContext;

class SplineRoadGizmoController
{
public:
	void draw(SplineRoadTool& tool, const ViewportToolContext& context) const;

private:
	void drawPointMoveGizmo(SplineRoadTool& tool, const ViewportToolContext& context) const;
	void drawPointRollGizmo(SplineRoadTool& tool, const ViewportToolContext& context) const;
	void drawSplineTransformGizmo(SplineRoadTool& tool, const ViewportToolContext& context) const;
};