#include "SplineRoadSection.h"

#include "Panels/Viewport/SplineRoadTool.h"
#include "Panels/Viewport/ViewportPanel.h"
#include "imgui.h"

void SplineRoadSection::draw(SplineRoadTool& tool, ViewportPanel& viewportPanel)
{
	bool splineModeActive = viewportPanel.getActiveTool() == &tool;
	if (ImGui::Checkbox("Spline road tool active", &splineModeActive))
		viewportPanel.setActiveTool(splineModeActive ? &tool : nullptr);

	if (ImGui::Button("Create road"))
	{
		tool.createRoad();
		viewportPanel.setActiveTool(&tool);
	}

	ImGui::SameLine();
	if (ImGui::Button("Clear") && tool.hasRoad())
		tool.clearRoad();
	ImGui::SameLine();
	if (ImGui::Button("Load spline"))
		tool.loadSpline();

	if (tool.getSplineCount() > 0)
	{
		int activeSplineIndex = static_cast<int>(tool.getActiveSplineListIndex());
		if (ImGui::BeginCombo("Active spline", tool.getActiveSplineName()))
		{
			for (size_t i = 0; i < tool.getSplineCount(); ++i)
			{
				const bool selected = activeSplineIndex == static_cast<int>(i);
				if (ImGui::Selectable(tool.getSplineName(i), selected))
					tool.selectSplineByIndex(i);
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::Text("Spline id %u", tool.getActiveSplineId());

		bool pointEditMode = tool.isPointEditMode();
		if (ImGui::RadioButton("Edit points", pointEditMode))
		{
			viewportPanel.setActiveTool(&tool);
			tool.enterPointEditMode();
		}
		ImGui::SameLine();
		bool splineTransformMode = tool.isSplineTransformMode();
		if (ImGui::RadioButton("Transform spline", splineTransformMode))
		{
			viewportPanel.setActiveTool(&tool);
			tool.enterSplineTransformMode();
		}

		if (ImGui::Button("Copy spline"))
			tool.copyActiveSpline();
	}

	ImGui::BeginDisabled(!tool.hasRoad());
	if (ImGui::Checkbox("Auto build preview", &tool.autoBuildPreview))
		tool.onAutoBuildPreviewChanged();
	bool physicsChanged = ImGui::Checkbox("Preview physics", &tool.previewPhysics);
	if (physicsChanged && tool.hasPreview())
		tool.buildPreview();
	if (tool.hasPreview())
		ImGui::Text("Preview %s", tool.isPreviewDirty() ? "stale" : "current");
	else
		ImGui::Text("Preview not built");

	const char* addModeNames[] = { "Auto", "Insert after", "Insert before", "Extend start", "Extend end" };
	int addModeIndex = static_cast<int>(tool.addPointMode);
	if (ImGui::Combo("Add mode", &addModeIndex, addModeNames, std::size(addModeNames)))
		tool.addPointMode = static_cast<SplineRoadTool::AddPointMode>(addModeIndex);

	const char* gizmoModeNames[] = { "Point move", "Point roll" };
	int gizmoModeIndex = static_cast<int>(tool.pointGizmoMode);
	if (gizmoModeIndex > 1)
		gizmoModeIndex = 0;
	if (ImGui::Combo("Point gizmo", &gizmoModeIndex, gizmoModeNames, std::size(gizmoModeNames)))
	{
		tool.pointGizmoMode = static_cast<SplineRoadTool::PointGizmoMode>(gizmoModeIndex);
		tool.enterPointEditMode();
	}

	bool profileChanged = false;
	const char* profileNames[] = { "Road", "Rectangle", "Circle", "Tube", "Arc", "Filled arc" };
	int profileIndex = static_cast<int>(tool.previewProfile);
	if (ImGui::Combo("Shape", &profileIndex, profileNames, std::size(profileNames)))
	{
		tool.previewProfile = static_cast<SplineRoadTool::PreviewProfile>(profileIndex);
		profileChanged = true;
	}

	if (tool.previewProfile == SplineRoadTool::PreviewProfile::Road)
	{
		profileChanged |= ImGui::DragFloat("Width", &tool.profileWidth, 0.25f, 1.0f, 40.0f, "%.1f m");
		profileChanged |= ImGui::DragInt("Width segments", &tool.roadWidthSegments, 1.0f, 1, 32);
	}
	else if (tool.previewProfile == SplineRoadTool::PreviewProfile::Rectangle)
	{
		profileChanged |= ImGui::DragFloat("Width", &tool.profileWidth, 0.25f, 0.1f, 40.0f, "%.1f m");
		profileChanged |= ImGui::DragFloat("Height", &tool.profileHeight, 0.1f, 0.1f, 20.0f, "%.1f m");
	}
	else
	{
		profileChanged |= ImGui::DragFloat("Radius", &tool.profileRadius, 0.25f, 0.1f, 40.0f, "%.1f m");
		if (tool.previewProfile == SplineRoadTool::PreviewProfile::Tube)
			profileChanged |= ImGui::DragFloat("Inner radius", &tool.profileInnerRadius, 0.25f, 0.01f, 40.0f, "%.1f m");
		if (tool.previewProfile == SplineRoadTool::PreviewProfile::Arc || tool.previewProfile == SplineRoadTool::PreviewProfile::FilledArc)
			profileChanged |= ImGui::DragFloat("Arc angle", &tool.profileArcAngleDegrees, 1.0f, 1.0f, 360.0f, "%.0f deg");
		profileChanged |= ImGui::DragInt("Segments", &tool.profileSegments, 1.0f, 3, 64);
	}
	if (profileChanged)
		tool.onProfileSettingsChanged();

	ImGui::DragFloat("Add distance", &tool.addPointDistance, 0.5f, 1.0f, 100.0f, "%.1f m");
	ImGui::Checkbox("Replicate curve on add", &tool.replicateCurveOnAdd);
	const char* replicateModeNames[] = { "Drift", "Loop" };
	int replicateModeIndex = static_cast<int>(tool.replicateCurveMode);
	if (ImGui::Combo("Replicate mode", &replicateModeIndex, replicateModeNames, std::size(replicateModeNames)))
		tool.replicateCurveMode = static_cast<SplineRoadTool::ReplicateCurveMode>(replicateModeIndex);
	bool foldoverChanged = false;
	foldoverChanged |= ImGui::Checkbox("Prevent foldover", &tool.preventProfileFoldover);
	if (tool.preventProfileFoldover)
		foldoverChanged |= ImGui::DragFloat("Min inner forward", &tool.minProfileForwardScale, 0.01f, 0.01f, 0.95f, "%.2f");
	if (foldoverChanged)
		tool.onProfileSettingsChanged();

	bool samplingChanged = false;
	samplingChanged |= ImGui::DragFloat("Curve tessellation", &tool.curvatureTessellationScale, 0.1f, 0.0f, 16.0f, "%.1f");
	samplingChanged |= ImGui::DragFloat("Roll tessellation", &tool.rollTessellationScale, 0.1f, 0.0f, 32.0f, "%.1f");
	samplingChanged |= ImGui::DragInt("Max path segments", &tool.maxTessellationSegments, 1.0f, 1, 128);
	if (samplingChanged)
		tool.onSplineSamplingSettingsChanged();

	if (ImGui::Button("Add/insert point"))
		tool.addPointForward();
	ImGui::SameLine();
	if (ImGui::Button("Remove point"))
		tool.removeSelectedPoint();

	const bool pointSelected = tool.getSelectedPointIndex() != Spline::InvalidPointIndex;
	ImGui::BeginDisabled(!pointSelected);
	ImGui::SameLine();
	if (ImGui::Button("Previous point"))
		tool.selectPreviousPoint();
	ImGui::SameLine();
	if (ImGui::Button("Next point"))
		tool.selectNextPoint();
	ImGui::EndDisabled();

	if (ImGui::Button("Build preview"))
		tool.buildPreview();
	ImGui::SameLine();
	if (ImGui::Button("Bake"))
		tool.bakePreview();
	if (ImGui::Button("Save spline"))
		tool.saveSpline();
	if (ImGui::Button("Bake and save"))
		tool.bakeAndSaveModel();

	bool debugChanged = false;
	debugChanged |= ImGui::Checkbox("Show path", &tool.showDebugPath);
	debugChanged |= ImGui::Checkbox("Show points", &tool.showDebugPoints);
	debugChanged |= ImGui::Checkbox("Show active frame", &tool.showActiveFrame);
	if (debugChanged)
		tool.refreshDebugVisualization();

	if (auto* road = tool.getRoad())
	{
		ImGui::Text("Points %d", (int)road->spline.getPointCount());
		if (tool.getSelectedPointIndex() != Spline::InvalidPointIndex && tool.getSelectedPointIndex() < road->spline.getPointCount())
		{
			auto point = road->spline.getPoint(tool.getSelectedPointIndex());
			float position[3] = { point.position.x, point.position.y, point.position.z };
			if (ImGui::DragFloat3("Point position", position, 0.25f))
				tool.setSelectedPointPosition({ position[0], position[1], position[2] });

			float roll = point.roll;
			if (ImGui::DragFloat("Point roll", &roll, 0.01f, -6.283185f, 6.283185f))
				tool.setSelectedPointRoll(roll);

			float upStabilization = point.upStabilization;
			if (ImGui::DragFloat("Up stabilize", &upStabilization, 0.01f, 0.0f, 1.0f, "%.2f"))
				tool.setSelectedPointUpStabilization(upStabilization);
		}
		else
		{
			ImGui::Text("No point selected");
		}
	}
	ImGui::EndDisabled();
}
