#include "SplineRoadSection.h"

#include "../data/editor/fonts/IconsFontAwesome7.h"
#include "Panels/Viewport/SplineRoadTool.h"
#include "Panels/Viewport/ViewportPanel.h"
#include "imgui.h"

void SplineRoadSection::draw(SplineRoadTool& tool, ViewportPanel& viewportPanel)
{
	ImGui::SeparatorText("Road");

	if (tool.getSplineCount() > 0)
	{
		const int activeSplineIndex = static_cast<int>(tool.getActiveSplineListIndex());
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
	}

	const bool placingRoad = viewportPanel.isPlacingFor(&tool);
	const char* newRoadLabel = placingRoad ? "Cancel placement" : ICON_FA_PLUS " New";
	if (ImGui::Button(newRoadLabel))
	{
		tool.createRoad();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(placingRoad);
	if (ImGui::Button(ICON_FA_FOLDER_OPEN " Load"))
		tool.loadSpline();
	ImGui::EndDisabled();

	if (placingRoad)
	{
		ImGui::TextDisabled("Click in the viewport to place the road. Right-click to cancel.");
		return;
	}

	if (!tool.hasRoad())
	{
		ImGui::TextDisabled("Create or load a spline road to begin.");
		return;
	}

	if (ImGui::Button(ICON_FA_COPY " Duplicate"))
		tool.copyActiveSpline();
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_TRASH " Delete"))
		tool.clearRoad();

	if (!tool.hasRoad())
		return;

	const SplineConstruction* road = tool.getRoad();
	const size_t pointCount = road->spline.getPointCount();
	const char* previewStatus = !tool.hasPreview() ? "Not built" : tool.isPreviewDirty() ? "Needs rebuild" : "Current";
	ImGui::TextDisabled("%zu points  |  Preview: %s", pointCount, previewStatus);

	bool splineModeActive = viewportPanel.getActiveTool() == &tool;
	if (ImGui::Checkbox("Edit in viewport", &splineModeActive))
		viewportPanel.setActiveTool(splineModeActive ? &tool : nullptr);

	ImGui::SeparatorText("Edit");
	if (ImGui::RadioButton("Points", tool.isPointEditMode()))
	{
		viewportPanel.setActiveTool(&tool);
		tool.enterPointEditMode();
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Transform spline", tool.isSplineTransformMode()))
	{
		viewportPanel.setActiveTool(&tool);
		tool.enterSplineTransformMode();
	}

	if (tool.isPointEditMode())
	{
		ImGui::SeparatorText("Point");

		const size_t selectedPointIndex = tool.getSelectedPointIndex();
		const bool pointSelected =
			selectedPointIndex != Spline::InvalidPointIndex &&
			selectedPointIndex < pointCount;
		ImGui::BeginDisabled(!pointSelected);
		if (ImGui::Button(ICON_FA_CHEVRON_LEFT "##previous_point"))
			tool.selectPreviousPoint();
		ImGui::SetItemTooltip("Previous point");
		ImGui::SameLine();
		if (pointSelected)
			ImGui::Text("Point %zu / %zu", selectedPointIndex + 1, pointCount);
		else
			ImGui::TextDisabled("No point selected");
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_CHEVRON_RIGHT "##next_point"))
			tool.selectNextPoint();
		ImGui::SetItemTooltip("Next point");
		ImGui::EndDisabled();

		if (ImGui::RadioButton(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Move",
							   tool.pointGizmoMode ==
								   SplineRoadTool::PointGizmoMode::Move))
			tool.pointGizmoMode = SplineRoadTool::PointGizmoMode::Move;
		ImGui::SameLine();
		if (ImGui::RadioButton(ICON_FA_ARROWS_ROTATE " Roll",
							   tool.pointGizmoMode ==
								   SplineRoadTool::PointGizmoMode::Roll))
			tool.pointGizmoMode = SplineRoadTool::PointGizmoMode::Roll;

		if (pointSelected)
		{
			const SplinePoint point = road->spline.getPoint(selectedPointIndex);
			float position[3] = {point.position.x, point.position.y,
								 point.position.z};
			if (ImGui::DragFloat3("Position", position, 0.25f))
				tool.setSelectedPointPosition({position[0], position[1], position[2]});

			float roll = point.roll;
			if (ImGui::DragFloat("Roll", &roll, 0.01f, -6.283185f, 6.283185f))
				tool.setSelectedPointRoll(roll);

			float upStabilization = point.upStabilization;
			if (ImGui::DragFloat("Up stabilization", &upStabilization, 0.01f, 0.0f,
								 1.0f, "%.2f"))
				tool.setSelectedPointUpStabilization(upStabilization);
		}
		else
		{
			ImGui::TextDisabled(
				"Select a point in the viewport to edit its properties.");
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("Add point");
		const char* addModeNames[] = {"Auto", "Insert after", "Insert before",
									  "Extend start", "Extend end"};
		int addModeIndex = static_cast<int>(tool.addPointMode);
		if (ImGui::Combo("Mode##add_point", &addModeIndex, addModeNames,
						 std::size(addModeNames)))
			tool.addPointMode =
				static_cast<SplineRoadTool::AddPointMode>(addModeIndex);
		ImGui::DragFloat("Distance##add_point", &tool.addPointDistance, 0.5f, 1.0f,
						 100.0f, "%.1f m");
		ImGui::Checkbox("Replicate curve", &tool.replicateCurveOnAdd);

		if (ImGui::Button(ICON_FA_PLUS " Add point"))
			tool.addPointForward();
		if (pointSelected)
		{
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TRASH " Remove point"))
				tool.removeSelectedPoint();
		}
	}

	ImGui::SeparatorText("Shape");

	bool profileChanged = false;
	const char* profileNames[] = {"Road", "Rectangle", "Circle",
								  "Tube", "Arc", "Filled arc"};
	int profileIndex = static_cast<int>(tool.previewProfile);
	if (ImGui::Combo("Profile", &profileIndex, profileNames,
					 std::size(profileNames)))
	{
		tool.previewProfile =
			static_cast<SplineRoadTool::PreviewProfile>(profileIndex);
		profileChanged = true;
	}

	if (tool.previewProfile == SplineRoadTool::PreviewProfile::Road)
	{
		profileChanged |= ImGui::DragFloat("Width", &tool.profileWidth, 0.25f, 1.0f,
										   40.0f, "%.1f m");
		profileChanged |=
			ImGui::DragInt("Width segments", &tool.roadWidthSegments, 1.0f, 1, 32);
	}
	else if (tool.previewProfile == SplineRoadTool::PreviewProfile::Rectangle)
	{
		profileChanged |= ImGui::DragFloat("Width", &tool.profileWidth, 0.25f, 0.1f,
										   40.0f, "%.1f m");
		profileChanged |= ImGui::DragFloat("Height", &tool.profileHeight, 0.1f,
										   0.1f, 20.0f, "%.1f m");
	}
	else
	{
		profileChanged |= ImGui::DragFloat("Radius", &tool.profileRadius, 0.25f,
										   0.1f, 40.0f, "%.1f m");
		if (tool.previewProfile == SplineRoadTool::PreviewProfile::Tube)
			profileChanged |=
				ImGui::DragFloat("Inner radius", &tool.profileInnerRadius, 0.25f,
								 0.01f, 40.0f, "%.1f m");
		if (tool.previewProfile == SplineRoadTool::PreviewProfile::Arc ||
			tool.previewProfile == SplineRoadTool::PreviewProfile::FilledArc)
			profileChanged |=
				ImGui::DragFloat("Arc angle", &tool.profileArcAngleDegrees, 1.0f,
								 1.0f, 360.0f, "%.0f deg");
		profileChanged |=
			ImGui::DragInt("Segments", &tool.profileSegments, 1.0f, 3, 64);
	}
	if (profileChanged)
		tool.onProfileSettingsChanged();

	bool foldoverChanged = false;
	foldoverChanged |=
		ImGui::Checkbox("Prevent foldover", &tool.preventProfileFoldover);
	if (tool.preventProfileFoldover)
		foldoverChanged |=
			ImGui::DragFloat("Min inner forward", &tool.minProfileForwardScale,
							 0.01f, 0.01f, 0.95f, "%.2f");
	if (foldoverChanged)
		tool.onProfileSettingsChanged();

	ImGui::SeparatorText("Preview");
	ImGui::TextDisabled("Status: %s", previewStatus);
	if (ImGui::Checkbox("Auto rebuild", &tool.autoBuildPreview))
		tool.onAutoBuildPreviewChanged();
	const bool physicsChanged = ImGui::Checkbox("Physics##spline_preview", &tool.previewPhysics);
	if (physicsChanged && tool.hasPreview())
		tool.buildPreview();

	if (!tool.autoBuildPreview || !tool.hasPreview() || tool.isPreviewDirty())
	{
		const char* buildLabel = tool.hasPreview()
									 ? ICON_FA_HAMMER " Rebuild preview"
									 : ICON_FA_HAMMER " Build preview";
		if (ImGui::Button(buildLabel))
			tool.buildPreview();
	}
	if (tool.hasPreview() && !tool.isPreviewDirty())
	{
		if (!tool.autoBuildPreview)
			ImGui::SameLine();
		if (ImGui::Button(ICON_FA_CUBE " Bake to scene"))
			tool.bakePreview();
	}

	ImGui::SeparatorText("Export");
	if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save spline"))
		tool.saveSpline();
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_FILE_EXPORT " Export model"))
		tool.bakeAndSaveModel();
	ImGui::SetItemTooltip(
		"Save the generated model and bake the preview into the scene");

	if (ImGui::CollapsingHeader("Advanced##spline_road"))
	{
		ImGui::TextUnformatted("Path sampling");
		bool samplingChanged = false;
		samplingChanged |=
			ImGui::DragFloat("Curve tessellation", &tool.curvatureTessellationScale,
							 0.1f, 0.0f, 16.0f, "%.1f");
		samplingChanged |=
			ImGui::DragFloat("Roll tessellation", &tool.rollTessellationScale, 0.1f,
							 0.0f, 32.0f, "%.1f");
		samplingChanged |= ImGui::DragInt(
			"Max path segments", &tool.maxTessellationSegments, 1.0f, 1, 128);
		if (samplingChanged)
			tool.onSplineSamplingSettingsChanged();

		ImGui::Spacing();
		ImGui::TextUnformatted("Debug display");
		bool debugChanged = false;
		debugChanged |= ImGui::Checkbox("Show path", &tool.showDebugPath);
		debugChanged |= ImGui::Checkbox("Show points", &tool.showDebugPoints);
		debugChanged |= ImGui::Checkbox("Show active frame", &tool.showActiveFrame);
		if (debugChanged)
			tool.refreshDebugVisualization();
	}
}
