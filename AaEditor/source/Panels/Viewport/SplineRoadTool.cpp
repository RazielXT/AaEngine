#include "SplineRoadTool.h"

#include "ApplicationCore.h"
#include "ViewportPanel.h"
#include "Scene/Camera.h"
#include "Scene/RenderEntity.h"
#include "Utils/Logger.h"
#include "../data/editor/fonts/IconsFontAwesome7.h"
#include "imgui.h"

#include <ResourceUploadBatch.h>
#include <algorithm>
#include <cmath>
#include <format>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;

	float degreesToRadians(float degrees)
	{
		return degrees * Pi / 180.0f;
	}

	void setToolbarTooltip(const char* text)
	{
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s", text);
	}

	const char* addPointModeName(SplineRoadTool::AddPointMode mode)
	{
		switch (mode)
		{
		case SplineRoadTool::AddPointMode::InsertAfter:
			return "Insert after";
		case SplineRoadTool::AddPointMode::InsertBefore:
			return "Insert before";
		case SplineRoadTool::AddPointMode::ExtendStart:
			return "Extend start";
		case SplineRoadTool::AddPointMode::ExtendEnd:
			return "Extend end";
		case SplineRoadTool::AddPointMode::Auto:
		default:
			return "Auto";
		}
	}
}

SplineRoadTool::SplineRoadTool(ApplicationCore& a, ViewportPanel& v)
	: app(a), viewport(v)
{
	viewport.registerSplineRoadTool(this);
	updateDebugSettings();
}

SplineRoadTool::~SplineRoadTool()
{
	for (auto& spline : splines)
		removeSplineFromWorld(*spline);
	splines.clear();
}

void SplineRoadTool::onPick(const EntityPicker::PickInfo& pickInfo, bool ctrlActive, Camera& camera)
{
	if (pickInfo.id && trySelectKnownSplinePart(pickInfo.id, pickInfo.position))
		return;

	if (!ctrlActive && pickInfo.id)
		Logger::log("Spline road tool ignored non-spline entity pick");
}

void SplineRoadTool::draw(const ViewportToolContext& ctx)
{
	const ImVec4 activeBg = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
	const ImVec4 inactiveBg = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
	const bool roadAvailable = hasRoad();
	const SplineConstruction* road = getRoad();
	const size_t pointCount = road ? road->spline.getPointCount() : 0;
	const bool pointSelected = selectedPointIndex != Spline::InvalidPointIndex && selectedPointIndex < pointCount;

	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

	if (ImGui::Button(ICON_FA_BEZIER_CURVE "##spline_create", ImVec2(40, 40)))
		createRoad();
	setToolbarTooltip("Create spline road");

	if (roadAvailable)
	{
		ImGui::Dummy(ImVec2(0.0f, 4.0f));

		ImGui::PushStyleColor(ImGuiCol_Button, pointGizmoMode == PointGizmoMode::Move ? activeBg : inactiveBg);
		if (ImGui::Button(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT "##spline_move_point", ImVec2(40, 40)))
		{
			pointGizmoMode = PointGizmoMode::Move;
			enterPointEditMode();
		}
		setToolbarTooltip(pointSelected ? "Move selected point" : "Enter point move mode");
		ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Button, pointGizmoMode == PointGizmoMode::Roll ? activeBg : inactiveBg);
		if (ImGui::Button(ICON_FA_ROTATE "##spline_roll_point", ImVec2(40, 40)))
		{
			pointGizmoMode = PointGizmoMode::Roll;
			enterPointEditMode();
		}
		setToolbarTooltip(pointSelected ? "Roll selected point" : "Enter point roll mode");
		ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Button, pointGizmoMode == PointGizmoMode::SplineTransform ? activeBg : inactiveBg);
		if (ImGui::Button(ICON_FA_OBJECT_GROUP "##spline_transform", ImVec2(40, 40)))
			enterSplineTransformMode();
		setToolbarTooltip("Transform entire spline");
		ImGui::PopStyleColor();

		if (isPointEditMode())
		{
			ImGui::Dummy(ImVec2(0.0f, 4.0f));

			if (ImGui::Button(ICON_FA_CIRCLE_PLUS "##spline_add_point", ImVec2(40, 40)))
				addPointForward();
			const std::string addTooltip = std::format("Add point ({})", addPointModeName(addPointMode));
			setToolbarTooltip(addTooltip.c_str());

			ImGui::BeginDisabled(!pointSelected);
			if (ImGui::Button(ICON_FA_CIRCLE_MINUS "##spline_remove_point", ImVec2(40, 40)))
				removeSelectedPoint();
			setToolbarTooltip("Remove selected point");
			ImGui::EndDisabled();

			const bool autoInserts = addPointMode == AddPointMode::Auto && pointSelected && selectedPointIndex > 0 && selectedPointIndex + 1 < pointCount;
			const bool usesPointSpacing = addPointMode != AddPointMode::InsertAfter && addPointMode != AddPointMode::InsertBefore && !autoInserts;
			if (usesPointSpacing)
			{
				ImGui::SetNextItemWidth(90);
				ImGui::DragFloat(ICON_FA_RULER "##SplinePointDistance", &addPointDistance, 0.5f, 1.0f, 100.0f, "%.1f m");
				setToolbarTooltip("Point spacing used when extending the spline");
			}
		}
	}

	ImGui::PopStyleVar();

	const bool previewCurrent = hasPreview() && !isPreviewDirty();
	if (roadAvailable && (!autoBuildPreview || !previewCurrent))
	{
		ImGui::Dummy(ImVec2(0.0f, 4.0f));
		const bool previewStale = hasPreview() && isPreviewDirty();
		if (previewStale)
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.38f, 0.10f, 1.0f));
		if (ImGui::Button(ICON_FA_HAMMER "##spline_build_preview", ImVec2(40, 40)))
			buildPreview();
		setToolbarTooltip(previewStale ? "Rebuild outdated preview" : hasPreview() ? "Rebuild preview" : "Build preview");
		if (previewStale)
			ImGui::PopStyleColor();
	}

	ImGui::EndGroup();
	overlayActive = ImGui::IsItemHovered();

	gizmoController.draw(*this, ctx);
}

void SplineRoadTool::onActivated()
{
	editModeActive = true;
	rebuildAllDebug();
}

void SplineRoadTool::onDeactivated()
{
	editModeActive = false;
	gizmoActive = false;
	rebuildAllDebug();
}

void SplineRoadTool::reset()
{
	overlayActive = false;
	gizmoActive = false;
}

void SplineRoadTool::createRoad()
{
	SplineRoadInstance& instance = createSplineInstance();
	SplineConstruction* construction = instance.construction.get();
	construction->spline.addPoint({ -12.0f, 1.0f, 0.0f });
	construction->spline.addPoint({ 0.0f, 1.0f, 12.0f });
	construction->spline.addPoint({ 12.0f, 1.0f, 0.0f });
	applySplineSamplingSettings();
	construction->profile = createPreviewProfile();
	construction->sweepSettings.pathUvScale = 0.25f;
	construction->sweepSettings.preventProfileFoldover = preventProfileFoldover;
	construction->sweepSettings.minProfileForwardScale = minProfileForwardScale;
	selectedPointIndex = construction->spline.getPointCount() - 1;
	instance.previewDirty = false;
	rebuildDebug();
}

void SplineRoadTool::clearRoad()
{
	SplineRoadInstance* instance = activeSpline();
	if (!instance)
		return;

	removeSplineFromWorld(*instance);
	splines.erase(splines.begin() + activeSplineIndex);
	if (splines.empty())
		activeSplineIndex = Spline::InvalidPointIndex;
	else
		activeSplineIndex = (std::min)(activeSplineIndex, splines.size() - 1);
	selectedPointIndex = Spline::InvalidPointIndex;
	rebuildAllDebug();
}

bool SplineRoadTool::hasPreview() const
{
	const SplineRoadInstance* instance = activeSpline();
	return instance && instance->previewBuilt;
}

bool SplineRoadTool::isPreviewDirty() const
{
	const SplineRoadInstance* instance = activeSpline();
	return instance && instance->previewDirty;
}

SplineConstruction* SplineRoadTool::getRoad()
{
	SplineRoadInstance* instance = activeSpline();
	return instance ? instance->construction.get() : nullptr;
}

const SplineConstruction* SplineRoadTool::getRoad() const
{
	const SplineRoadInstance* instance = activeSpline();
	return instance ? instance->construction.get() : nullptr;
}

uint32_t SplineRoadTool::getActiveSplineId() const
{
	const SplineRoadInstance* instance = activeSpline();
	return instance ? instance->id : 0;
}

const char* SplineRoadTool::getActiveSplineName() const
{
	const SplineRoadInstance* instance = activeSpline();
	return instance ? instance->name.c_str() : "None";
}

const char* SplineRoadTool::getSplineName(size_t index) const
{
	return index < splines.size() ? splines[index]->name.c_str() : "";
}

void SplineRoadTool::selectSplineByIndex(size_t index, bool movementMode)
{
	if (index >= splines.size())
		return;

	activeSplineIndex = index;
	selectedPointIndex = Spline::InvalidPointIndex;
	if (movementMode)
		enterSplineTransformMode();
	else
		rebuildAllDebug();
}

void SplineRoadTool::enterPointEditMode()
{
	if (pointGizmoMode == PointGizmoMode::SplineTransform)
		pointGizmoMode = PointGizmoMode::Move;
	rebuildAllDebug();
}

void SplineRoadTool::enterSplineTransformMode()
{
	selectedPointIndex = Spline::InvalidPointIndex;
	pointGizmoMode = PointGizmoMode::SplineTransform;
	rebuildAllDebug();
}

void SplineRoadTool::selectPreviousPoint()
{
	pointEditor.selectPointByOffset(*this, -1);
}

void SplineRoadTool::selectNextPoint()
{
	pointEditor.selectPointByOffset(*this, 1);
}

SplineRoadTool::SplineRoadInstance* SplineRoadTool::activeSpline()
{
	return activeSplineIndex < splines.size() ? splines[activeSplineIndex].get() : nullptr;
}

const SplineRoadTool::SplineRoadInstance* SplineRoadTool::activeSpline() const
{
	return activeSplineIndex < splines.size() ? splines[activeSplineIndex].get() : nullptr;
}

SplineRoadTool::SplineRoadInstance* SplineRoadTool::findSplineByPathEntity(RenderEntity* entity)
{
	for (auto& spline : splines)
		if (spline->debugVisualization.getPathEntity() == entity)
			return spline.get();

	return nullptr;
}

SplineRoadTool::SplineRoadInstance& SplineRoadTool::createSplineInstance()
{
	auto instance = std::make_unique<SplineRoadInstance>();
	instance->id = nextSplineId++;
	instance->name = std::format("Spline {}", instance->id);
	instance->construction = std::make_unique<SplineConstruction>();
	splines.push_back(std::move(instance));
	activeSplineIndex = splines.size() - 1;
	selectedPointIndex = Spline::InvalidPointIndex;
	return *splines.back();
}

void SplineRoadTool::removeSplineFromWorld(SplineRoadInstance& instance)
{
	if (instance.construction)
		instance.construction->removeFromWorld(app.renderWorld, &app.physicsMgr);
	instance.debugVisualization.removeFromWorld(app.renderWorld);
	instance.previewBuilt = false;
	instance.previewDirty = false;
}

void SplineRoadTool::addPointForward()
{
	pointEditor.addPoint(*this);
}

void SplineRoadTool::removeSelectedPoint()
{
	pointEditor.removeSelectedPoint(*this);
}

void SplineRoadTool::buildPreview()
{
	SplineRoadInstance* instance = activeSpline();
	SplineConstruction* construction = getRoad();
	if (!construction)
		return;

	applySplineSamplingSettings();
	construction->profile = createPreviewProfile();
	construction->sweepSettings.preventProfileFoldover = preventProfileFoldover;
	construction->sweepSettings.minProfileForwardScale = minProfileForwardScale;

	DirectX::ResourceUploadBatch batch(app.renderSystem.core.device);
	batch.Begin();

	SplineConstructionCreateParams params;
	params.materialName = "General";
	params.createMeshPhysics = previewPhysics;
	construction->regenerate(app.renderSystem, app.resources, app.renderWorld, &app.physicsMgr, batch, params);

	auto uploadResourcesFinished = batch.End(app.renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();

	instance->previewBuilt = construction->entity != nullptr;
	instance->previewDirty = false;
	rebuildDebug();
}

void SplineRoadTool::bakePreview()
{
	SplineRoadInstance* instance = activeSpline();
	SplineConstruction* construction = getRoad();
	if (!construction || !construction->entity)
		return;

	instance->debugVisualization.removeFromWorld(app.renderWorld);
	bakedConstructions.push_back(std::move(instance->construction));
	splines.erase(splines.begin() + activeSplineIndex);
	if (splines.empty())
		activeSplineIndex = Spline::InvalidPointIndex;
	else
		activeSplineIndex = (std::min)(activeSplineIndex, splines.size() - 1);
	selectedPointIndex = Spline::InvalidPointIndex;
	rebuildAllDebug();
	Logger::log("Spline road preview baked into a regular render entity");
}

void SplineRoadTool::copyActiveSpline()
{
	const SplineRoadInstance* source = activeSpline();
	if (!source || !source->construction)
		return;

	SplineRoadInstance& copy = createSplineInstance();
	copy.name = std::format("Spline {} copy", source->id);
	copy.construction->spline = source->construction->spline;
	copy.construction->profile = source->construction->profile;
	copy.construction->sweepSettings = source->construction->sweepSettings;
	copy.previewDirty = true;
	selectedPointIndex = copy.construction->spline.getPointCount() ? copy.construction->spline.getPointCount() - 1 : Spline::InvalidPointIndex;
	if (autoBuildPreview)
		buildPreview();
	else
		rebuildAllDebug();
}

void SplineRoadTool::saveSpline()
{
	persistence.saveSpline(*this);
}

void SplineRoadTool::loadSpline()
{
	persistence.loadSpline(*this);
}

void SplineRoadTool::bakeAndSaveModel()
{
	persistence.bakeAndSaveModel(*this);
}

void SplineRoadTool::setSelectedPointPosition(Vector3 position)
{
	pointEditor.setSelectedPointPosition(*this, position);
}

void SplineRoadTool::setSelectedPointRoll(float roll)
{
	pointEditor.setSelectedPointRoll(*this, roll);
}

void SplineRoadTool::setSelectedPointUpStabilization(float upStabilization)
{
	pointEditor.setSelectedPointUpStabilization(*this, upStabilization);
}

void SplineRoadTool::onAutoBuildPreviewChanged()
{
	if (autoBuildPreview && getRoad() && (isPreviewDirty() || !hasPreview()))
		buildPreview();
}

void SplineRoadTool::onProfileSettingsChanged()
{
	markPreviewSettingsChanged();
}

void SplineRoadTool::onSplineSamplingSettingsChanged()
{
	if (!getRoad())
		return;

	applySplineSamplingSettings();
	markConstructionChanged();
}

void SplineRoadTool::refreshDebugVisualization()
{
	rebuildAllDebug();
}

bool SplineRoadTool::trySelectKnownSplinePart(ObjectId id, Vector3 pickedPosition)
{
	auto object = app.renderWorld.getObject(id);
	if (!object.entity)
		return false;

	SplineRoadInstance* pathInstance = findSplineByPathEntity(object.entity);
	if (pathInstance)
	{
		if (editModeActive)
			return true;

		for (size_t i = 0; i < splines.size(); ++i)
		{
			if (splines[i].get() == pathInstance)
			{
				activeSplineIndex = i;
				break;
			}
		}
		selectedPointIndex = Spline::InvalidPointIndex;
		enterSplineTransformMode();
		Logger::log(std::format("Selected {} for spline movement", pathInstance->name));
		return true;
	}

	SplineRoadInstance* instance = activeSpline();
	SplineConstruction* construction = getRoad();
	if (!instance || !construction)
		return false;

	size_t pointIndex = instance->debugVisualization.getPointIndex(object.entity);
	if (pointIndex != Spline::InvalidPointIndex)
	{
		selectedPointIndex = pointIndex;
		enterPointEditMode();
		if (autoBuildPreview && (instance->previewDirty || !instance->previewBuilt))
			buildPreview();
		Logger::log(std::format("Selected spline point {}", pointIndex));
		return true;
	}

	return false;
}

bool SplineRoadTool::selectPathForMovement(ObjectId id, Vector3 pickedPosition)
{
	return id && trySelectKnownSplinePart(id, pickedPosition) && pointGizmoMode == PointGizmoMode::SplineTransform;
}

void SplineRoadTool::rebuildDebug()
{
	SplineRoadInstance* instance = activeSpline();
	if (!instance)
		return;

	rebuildDebug(*instance);
}

void SplineRoadTool::rebuildDebug(SplineRoadInstance& instance)
{
	if (!instance.construction)
		return;

	updateDebugSettings();
	debugSettings.showPath = true;
	debugSettings.showPoints = editModeActive && &instance == activeSpline();
	debugSettings.showActivePointFrame = editModeActive && &instance == activeSpline() && showActiveFrame;
	debugSettings.activePointIndex = &instance == activeSpline() ? selectedPointIndex : Spline::InvalidPointIndex;

	DirectX::ResourceUploadBatch batch(app.renderSystem.core.device);
	batch.Begin();
	instance.debugVisualization.rebuild(instance.construction->spline, app.renderSystem, app.resources, app.renderWorld, batch, debugSettings);
	auto uploadResourcesFinished = batch.End(app.renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();
}

void SplineRoadTool::rebuildAllDebug()
{
	for (auto& instance : splines)
		rebuildDebug(*instance);
}

void SplineRoadTool::removePreview()
{
	SplineRoadInstance* instance = activeSpline();
	if (!instance)
		return;

	if (instance->construction)
		instance->construction->removeFromWorld(app.renderWorld, &app.physicsMgr);
	instance->previewBuilt = false;
	instance->previewDirty = false;
}

void SplineRoadTool::markConstructionChanged()
{
	SplineRoadInstance* instance = activeSpline();
	if (instance && instance->construction && instance->construction->entity)
		instance->previewDirty = true;
	else
		if (instance)
			instance->previewDirty = false;

	if (autoBuildPreview && getRoad())
		buildPreview();
	else
		rebuildAllDebug();
}

void SplineRoadTool::markPreviewSettingsChanged()
{
	SplineRoadInstance* instance = activeSpline();
	if (instance && instance->construction && instance->construction->entity)
		instance->previewDirty = true;

	if (autoBuildPreview && getRoad())
		buildPreview();
}

void SplineRoadTool::updateDebugSettings()
{
	debugSettings.showPath = showDebugPath;
	debugSettings.showPoints = showDebugPoints;
	debugSettings.showActivePointFrame = showActiveFrame;
	debugSettings.activePointIndex = selectedPointIndex;
}

void SplineRoadTool::applySplineSamplingSettings()
{
	SplineConstruction* construction = getRoad();
	if (!construction)
		return;

	construction->spline.setAdaptiveTessellation(curvatureTessellationScale, static_cast<size_t>((std::max)(maxTessellationSegments, 1)), rollTessellationScale);
}

void SplineRoadTool::ensureRoad()
{
	if (!getRoad())
		createRoad();
}

ShapeProfile2D SplineRoadTool::createPreviewProfile() const
{
	const size_t segments = static_cast<size_t>((std::max)(profileSegments, 3));
	const float width = (std::max)(profileWidth, 0.1f);
	const float height = (std::max)(profileHeight, 0.1f);
	const float radius = (std::max)(profileRadius, 0.1f);
	const float innerRadius = (std::min)((std::max)(profileInnerRadius, 0.01f), radius * 0.95f);
	const float arcAngle = degreesToRadians((std::max)(profileArcAngleDegrees, 1.0f));

	switch (previewProfile)
	{
	case PreviewProfile::Rectangle:
		return ShapeProfile2D::createRectangle(width, height);
	case PreviewProfile::Circle:
		return ShapeProfile2D::createCircle(radius, segments);
	case PreviewProfile::Tube:
		return ShapeProfile2D::createTube(radius, innerRadius, segments);
	case PreviewProfile::Arc:
		return ShapeProfile2D::createArc(radius, arcAngle, segments);
	case PreviewProfile::FilledArc:
		return ShapeProfile2D::createFilledArc(radius, arcAngle, segments);
	case PreviewProfile::Road:
	default:
		return ShapeProfile2D::createRoad(width, static_cast<size_t>((std::max)(roadWidthSegments, 1)));
	}
}
