#include "SplineRoadTool.h"

#include "ApplicationCore.h"
#include "ViewportPanel.h"
#include "Scene/Camera.h"
#include "Scene/RenderEntity.h"
#include "Utils/Logger.h"
#include "../data/editor/fonts/IconsFontAwesome7.h"
#include "ImGuizmo.h"
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
}

SplineRoadTool::SplineRoadTool(ApplicationCore& a, ViewportPanel& v)
	: app(a), viewport(v)
{
	updateDebugSettings();
}

SplineRoadTool::~SplineRoadTool()
{
	clearRoad();
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
	auto activeBg = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
	auto inactiveBg = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];

	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

	if (ImGui::Button(ICON_FA_ROUTE " ##spline_create", ImVec2(40, 40)))
		createRoad();

	ImGui::PushStyleColor(ImGuiCol_Button, hasRoad() ? inactiveBg : ImVec4(inactiveBg.x, inactiveBg.y, inactiveBg.z, 0.45f));
	if (ImGui::Button(ICON_FA_CIRCLE_PLUS " ##spline_add_point", ImVec2(40, 40)) && hasRoad())
		addPointForward();
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Button, selectedPointIndex != Spline::InvalidPointIndex ? inactiveBg : ImVec4(inactiveBg.x, inactiveBg.y, inactiveBg.z, 0.45f));
	if (ImGui::Button(ICON_FA_CIRCLE_MINUS " ##spline_remove_point", ImVec2(40, 40)) && selectedPointIndex != Spline::InvalidPointIndex)
		removeSelectedPoint();
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Button, hasPreview() ? activeBg : inactiveBg);
	if (ImGui::Button(ICON_FA_HAMMER " ##spline_build_preview", ImVec2(40, 40)) && hasRoad())
		buildPreview();
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Button, pointGizmoMode == PointGizmoMode::Move ? activeBg : inactiveBg);
	if (ImGui::Button(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " ##spline_move_point", ImVec2(40, 40)))
		pointGizmoMode = PointGizmoMode::Move;
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Button, pointGizmoMode == PointGizmoMode::Roll ? activeBg : inactiveBg);
	if (ImGui::Button(ICON_FA_ARROWS_ROTATE " ##spline_roll_point", ImVec2(40, 40)))
		pointGizmoMode = PointGizmoMode::Roll;
	ImGui::PopStyleColor();

	ImGui::PopStyleVar();

	ImGui::SetNextItemWidth(90);
	ImGui::DragFloat("##SplinePointDistance", &addPointDistance, 0.5f, 1.0f, 100.0f, "%.1f m");

	ImGui::EndGroup();
	overlayActive = ImGui::IsItemHovered();

	drawSelectedPointGizmo(ctx);
}

void SplineRoadTool::onActivated()
{
	editModeActive = true;
	rebuildDebug();
}

void SplineRoadTool::onDeactivated()
{
	editModeActive = false;
	gizmoActive = false;
	debugVisualization.removeFromWorld(app.renderWorld);
}

void SplineRoadTool::reset()
{
	overlayActive = false;
	gizmoActive = false;
}

void SplineRoadTool::createRoad()
{
	clearRoad();
	construction = std::make_unique<SplineConstruction>();
	construction->spline.addPoint({ -12.0f, 1.0f, 0.0f });
	construction->spline.addPoint({ 0.0f, 1.0f, 12.0f });
	construction->spline.addPoint({ 12.0f, 1.0f, 0.0f });
	applySplineSamplingSettings();
	construction->profile = createPreviewProfile();
	construction->sweepSettings.pathUvScale = 0.25f;
	construction->sweepSettings.preventProfileFoldover = preventProfileFoldover;
	construction->sweepSettings.minProfileForwardScale = minProfileForwardScale;
	selectedPointIndex = construction->spline.getPointCount() - 1;
	previewDirty = false;
	rebuildDebug();
}

void SplineRoadTool::clearRoad()
{
	removePreview();
	debugVisualization.removeFromWorld(app.renderWorld);
	construction.reset();
	selectedPointIndex = Spline::InvalidPointIndex;
	previewDirty = false;
}

void SplineRoadTool::addPointForward()
{
	ensureRoad();
	auto& spline = construction->spline;
	const size_t pointCount = spline.getPointCount();

	if (pointCount == 0)
	{
		spline.addPoint({});
		selectedPointIndex = 0;
		markConstructionChanged();
		return;
	}

	switch (addPointMode)
	{
	case AddPointMode::InsertAfter:
		if (selectedPointIndex != Spline::InvalidPointIndex && selectedPointIndex + 1 < pointCount)
		{
			insertPointBetween(selectedPointIndex, selectedPointIndex + 1);
			return;
		}
		break;
	case AddPointMode::InsertBefore:
		if (selectedPointIndex != Spline::InvalidPointIndex && selectedPointIndex > 0 && selectedPointIndex < pointCount)
		{
			insertPointBetween(selectedPointIndex - 1, selectedPointIndex);
			return;
		}
		break;
	case AddPointMode::ExtendStart:
		selectedPointIndex = 0;
		break;
	case AddPointMode::ExtendEnd:
		selectedPointIndex = pointCount - 1;
		break;
	case AddPointMode::Auto:
	default:
		if (selectedPointIndex > 0 && selectedPointIndex != Spline::InvalidPointIndex && selectedPointIndex + 1 < pointCount)
		{
			insertPointBetween(selectedPointIndex, selectedPointIndex + 1);
			return;
		}
		break;
	}

	bool extendStart = shouldExtendStart(pointCount);

	Vector3 direction = getForwardAddDirection(extendStart);
	if (extendStart)
	{
		Vector3 position = spline.getPoint(0).position + direction * addPointDistance;
		spline.insertPoint(0, position);
		selectedPointIndex = 0;
	}
	else
	{
		Vector3 position = spline.getPoint(pointCount - 1).position + direction * addPointDistance;
		spline.addPoint(position);
		selectedPointIndex = spline.getPointCount() - 1;
	}

	markConstructionChanged();
}

void SplineRoadTool::removeSelectedPoint()
{
	if (!construction || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.removePoint(selectedPointIndex);
	const size_t pointCount = construction->spline.getPointCount();
	selectedPointIndex = pointCount ? (std::min)(selectedPointIndex, pointCount - 1) : Spline::InvalidPointIndex;
	markConstructionChanged();
}

void SplineRoadTool::buildPreview()
{
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

	previewBuilt = construction->entity != nullptr;
	previewDirty = false;
	rebuildDebug();
}

void SplineRoadTool::bakePreview()
{
	if (!construction || !construction->entity)
		return;

	debugVisualization.removeFromWorld(app.renderWorld);
	bakedConstructions.push_back(std::move(construction));
	selectedPointIndex = Spline::InvalidPointIndex;
	previewBuilt = false;
	previewDirty = false;
	Logger::log("Spline road preview baked into a regular render entity");
}

void SplineRoadTool::setSelectedPointPosition(Vector3 position)
{
	if (!construction || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.setPoint(selectedPointIndex, position);
	markConstructionChanged();
}

void SplineRoadTool::setSelectedPointRoll(float roll)
{
	if (!construction || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.setPointRoll(selectedPointIndex, roll);
	markConstructionChanged();
}

void SplineRoadTool::setSelectedPointUpStabilization(float upStabilization)
{
	if (!construction || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.setPointUpStabilization(selectedPointIndex, upStabilization);
	markConstructionChanged();
}

void SplineRoadTool::onAutoBuildPreviewChanged()
{
	if (autoBuildPreview && construction && (previewDirty || !previewBuilt))
		buildPreview();
}

void SplineRoadTool::onProfileSettingsChanged()
{
	markPreviewSettingsChanged();
}

void SplineRoadTool::onSplineSamplingSettingsChanged()
{
	if (!construction)
		return;

	applySplineSamplingSettings();
	markConstructionChanged();
}

void SplineRoadTool::refreshDebugVisualization()
{
	rebuildDebug();
}

bool SplineRoadTool::trySelectKnownSplinePart(ObjectId id, Vector3 pickedPosition)
{
	if (!construction)
		return false;

	auto object = app.renderWorld.getObject(id);
	if (!object.entity)
		return false;

	size_t pointIndex = debugVisualization.getPointIndex(object.entity);
	if (pointIndex != Spline::InvalidPointIndex)
	{
		selectedPointIndex = pointIndex;
		if (autoBuildPreview && (previewDirty || !previewBuilt))
			buildPreview();
		else
			rebuildDebug();
		Logger::log(std::format("Selected spline point {}", pointIndex));
		return true;
	}

	if (object.entity == debugVisualization.getPathEntity())
	{
		selectedPointIndex = construction->spline.getClosestPointIndex(pickedPosition);
		if (autoBuildPreview && (previewDirty || !previewBuilt))
			buildPreview();
		else
			rebuildDebug();
		Logger::log(std::format("Selected nearest spline point {}", selectedPointIndex));
		return true;
	}

	return false;
}

void SplineRoadTool::rebuildDebug()
{
	if (!construction || !editModeActive)
		return;

	updateDebugSettings();

	DirectX::ResourceUploadBatch batch(app.renderSystem.core.device);
	batch.Begin();
	debugVisualization.rebuild(construction->spline, app.renderSystem, app.resources, app.renderWorld, batch, debugSettings);
	auto uploadResourcesFinished = batch.End(app.renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();
}

void SplineRoadTool::removePreview()
{
	if (construction)
		construction->removeFromWorld(app.renderWorld, &app.physicsMgr);
	previewBuilt = false;
	previewDirty = false;
}

void SplineRoadTool::markConstructionChanged()
{
	if (construction && construction->entity)
		previewDirty = true;
	else
		previewDirty = false;

	if (autoBuildPreview && construction)
		buildPreview();
	else
		rebuildDebug();
}

void SplineRoadTool::markPreviewSettingsChanged()
{
	if (construction && construction->entity)
		previewDirty = true;

	if (autoBuildPreview && construction)
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
	if (!construction)
		return;

	construction->spline.setAdaptiveTessellation(curvatureTessellationScale, static_cast<size_t>((std::max)(maxTessellationSegments, 1)), rollTessellationScale);
}

void SplineRoadTool::ensureRoad()
{
	if (!construction)
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

void SplineRoadTool::drawSelectedPointGizmo(const ViewportToolContext& ctx)
{
	if (!construction || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= construction->spline.getPointCount())
	{
		gizmoActive = false;
		return;
	}

	if (pointGizmoMode == PointGizmoMode::Roll)
		drawSelectedPointRollGizmo(ctx);
	else
		drawSelectedPointMoveGizmo(ctx);
}

void SplineRoadTool::drawSelectedPointMoveGizmo(const ViewportToolContext& ctx)
{
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect((float)ctx.viewportBounds[0].x, (float)ctx.viewportBounds[0].y, (float)ctx.viewportBounds[1].x - ctx.viewportBounds[0].x, (float)ctx.viewportBounds[1].y - ctx.viewportBounds[0].y);

	XMFLOAT4X4 cameraView;
	XMStoreFloat4x4(&cameraView, ctx.camera.getViewMatrix());
	XMFLOAT4X4 cameraProjection;
	XMStoreFloat4x4(&cameraProjection, ctx.camera.getProjectionMatrixNoReverse());

	ObjectTransformation pointTransform;
	pointTransform.position = construction->spline.getPoint(selectedPointIndex).position;

	XMFLOAT4X4 transform;
	XMStoreFloat4x4(&transform, pointTransform.createWorldMatrix());

	float snapValues[3] = { 0.5f, 0.5f, 0.5f };
	const bool manipulated = ImGuizmo::Manipulate(&cameraView._11, &cameraProjection._11, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, &transform._11, nullptr, snapValues);
	gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();

	if (!manipulated)
		return;

	ObjectTransformation updatedTransform;
	updatedTransform.fromWorldMatrix(XMLoadFloat4x4(&transform));

	const Vector3 oldPosition = construction->spline.getPoint(selectedPointIndex).position;
	if ((updatedTransform.position - oldPosition).LengthSquared() > 0.000001f)
		setSelectedPointPosition(updatedTransform.position);
}

void SplineRoadTool::drawSelectedPointRollGizmo(const ViewportToolContext& ctx)
{
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect((float)ctx.viewportBounds[0].x, (float)ctx.viewportBounds[0].y, (float)ctx.viewportBounds[1].x - ctx.viewportBounds[0].x, (float)ctx.viewportBounds[1].y - ctx.viewportBounds[0].y);

	XMFLOAT4X4 cameraView;
	XMStoreFloat4x4(&cameraView, ctx.camera.getViewMatrix());
	XMFLOAT4X4 cameraProjection;
	XMStoreFloat4x4(&cameraProjection, ctx.camera.getProjectionMatrixNoReverse());

	const SplinePoint point = construction->spline.getPoint(selectedPointIndex);
	SplineSample sample = construction->spline.evaluate(construction->spline.getSegmentCount() > 0 ? static_cast<float>(selectedPointIndex) / static_cast<float>(construction->spline.getSegmentCount()) : 0.0f);
	float closestDistanceSquared = (sample.position - point.position).LengthSquared();
	for (const SplineSample& candidate : construction->spline.getSamples())
	{
		const float distanceSquared = (candidate.position - point.position).LengthSquared();
		if (distanceSquared < closestDistanceSquared)
		{
			closestDistanceSquared = distanceSquared;
			sample = candidate;
		}
	}
	ObjectTransformation pointTransform;
	pointTransform.position = point.position;
	pointTransform.orientation = Quaternion::CreateFromRotationMatrix(Matrix::CreateWorld(Vector3::Zero, sample.tangent, sample.up));

	XMFLOAT4X4 transform;
	XMStoreFloat4x4(&transform, pointTransform.createWorldMatrix());
	XMFLOAT4X4 transformDelta{};

	const bool manipulated = ImGuizmo::Manipulate(&cameraView._11, &cameraProjection._11, ImGuizmo::ROTATE_Z, ImGuizmo::LOCAL, &transform._11, &transformDelta._11, nullptr);
	gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();

	if (!manipulated)
		return;

	XMVECTOR scale;
	XMVECTOR rotation;
	XMVECTOR translation;
	if (!XMMatrixDecompose(&scale, &rotation, &translation, XMLoadFloat4x4(&transformDelta)))
		return;

	Quaternion deltaRotation;
	XMStoreFloat4(&deltaRotation, rotation);
	Vector3 axis(deltaRotation.x, deltaRotation.y, deltaRotation.z);
	const float axisLength = axis.Length();
	if (axisLength <= 0.000001f)
		return;

	axis /= axisLength;
	float deltaAngle = 2.0f * std::atan2(axisLength, deltaRotation.w);
	if (deltaAngle > Pi)
		deltaAngle -= Pi * 2.0f;
	if (deltaAngle < -Pi)
		deltaAngle += Pi * 2.0f;

	const float rollDelta = deltaAngle * axis.Dot(sample.tangent);
	if (std::abs(rollDelta) > 0.00001f)
		setSelectedPointRoll(point.roll + rollDelta);
}

void SplineRoadTool::insertPointBetween(size_t previousPointIndex, size_t nextPointIndex)
{
	auto& spline = construction->spline;
	const auto& previousPoint = spline.getPoint(previousPointIndex);
	const auto& nextPoint = spline.getPoint(nextPointIndex);
	SplinePoint insertedPoint;
	insertedPoint.position = (previousPoint.position + nextPoint.position) * 0.5f;
	insertedPoint.roll = (previousPoint.roll + nextPoint.roll) * 0.5f;
	insertedPoint.upStabilization = (previousPoint.upStabilization + nextPoint.upStabilization) * 0.5f;
	spline.insertPoint(nextPointIndex, insertedPoint.position, insertedPoint.roll);
	spline.setPoint(nextPointIndex, insertedPoint);
	selectedPointIndex = nextPointIndex;
	markConstructionChanged();
}

bool SplineRoadTool::shouldExtendStart(size_t pointCount) const
{
	if (addPointMode == AddPointMode::ExtendStart)
		return true;
	if (addPointMode == AddPointMode::ExtendEnd)
		return false;

	return selectedPointIndex == 0 && pointCount > 1;
}

Vector3 SplineRoadTool::getForwardAddDirection(bool extendStart) const
{
	if (!construction || construction->spline.getPointCount() < 2)
		return Vector3::UnitZ;

	const auto& spline = construction->spline;
	Vector3 direction;
	if (extendStart)
		direction = spline.getPoint(0).position - spline.getPoint(1).position;
	else
		direction = spline.getPoint(spline.getPointCount() - 1).position - spline.getPoint(spline.getPointCount() - 2).position;

	if (direction.LengthSquared() < 0.0001f)
		return Vector3::UnitZ;

	direction.Normalize();
	return direction;
}
