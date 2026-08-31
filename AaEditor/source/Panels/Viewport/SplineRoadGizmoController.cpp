#include "SplineRoadGizmoController.h"

#include "SplineRoadTool.h"
#include "Scene/Camera.h"
#include "imgui.h"
#include "ImGuizmo.h"

#include <cmath>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;

	void configureGizmo(const ViewportToolContext& context)
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(
			static_cast<float>(context.viewportBounds[0].x),
			static_cast<float>(context.viewportBounds[0].y),
			static_cast<float>(context.viewportBounds[1].x - context.viewportBounds[0].x),
			static_cast<float>(context.viewportBounds[1].y - context.viewportBounds[0].y));
	}

	void getCameraMatrices(const ViewportToolContext& context, XMFLOAT4X4& view, XMFLOAT4X4& projection)
	{
		XMStoreFloat4x4(&view, context.camera.getViewMatrix());
		XMStoreFloat4x4(&projection, context.camera.getProjectionMatrixNoReverse());
	}
}

void SplineRoadGizmoController::draw(SplineRoadTool& tool, const ViewportToolContext& context) const
{
	if (tool.pointGizmoMode == SplineRoadTool::PointGizmoMode::SplineTransform)
	{
		drawSplineTransformGizmo(tool, context);
		return;
	}

	SplineConstruction* construction = tool.getRoad();
	if (!construction || tool.selectedPointIndex == Spline::InvalidPointIndex || tool.selectedPointIndex >= construction->spline.getPointCount())
	{
		tool.gizmoActive = false;
		return;
	}

	if (tool.pointGizmoMode == SplineRoadTool::PointGizmoMode::Roll)
		drawPointRollGizmo(tool, context);
	else
		drawPointMoveGizmo(tool, context);
}

void SplineRoadGizmoController::drawPointMoveGizmo(SplineRoadTool& tool, const ViewportToolContext& context) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction)
		return;

	configureGizmo(context);
	XMFLOAT4X4 cameraView;
	XMFLOAT4X4 cameraProjection;
	getCameraMatrices(context, cameraView, cameraProjection);

	ObjectTransformation pointTransform;
	pointTransform.position = construction->spline.getPoint(tool.selectedPointIndex).position;

	XMFLOAT4X4 transform;
	XMStoreFloat4x4(&transform, pointTransform.createWorldMatrix());

	float snapValues[3] = { 0.5f, 0.5f, 0.5f };
	const bool manipulated = ImGuizmo::Manipulate(&cameraView._11, &cameraProjection._11, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, &transform._11, nullptr, snapValues);
	tool.gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
	if (!manipulated)
		return;

	ObjectTransformation updatedTransform;
	updatedTransform.fromWorldMatrix(XMLoadFloat4x4(&transform));
	const Vector3 oldPosition = construction->spline.getPoint(tool.selectedPointIndex).position;
	if ((updatedTransform.position - oldPosition).LengthSquared() > 0.000001f)
		tool.setSelectedPointPosition(updatedTransform.position);
}

void SplineRoadGizmoController::drawPointRollGizmo(SplineRoadTool& tool, const ViewportToolContext& context) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction)
		return;

	configureGizmo(context);
	XMFLOAT4X4 cameraView;
	XMFLOAT4X4 cameraProjection;
	getCameraMatrices(context, cameraView, cameraProjection);

	const SplinePoint point = construction->spline.getPoint(tool.selectedPointIndex);
	SplineSample sample = construction->spline.evaluate(construction->spline.getSegmentCount() > 0 ? static_cast<float>(tool.selectedPointIndex) / static_cast<float>(construction->spline.getSegmentCount()) : 0.0f);
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
	tool.gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
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
		tool.setSelectedPointRoll(point.roll + rollDelta);
}

void SplineRoadGizmoController::drawSplineTransformGizmo(SplineRoadTool& tool, const ViewportToolContext& context) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction || construction->spline.getPointCount() == 0)
	{
		tool.gizmoActive = false;
		return;
	}

	Vector3 center{};
	for (const SplinePoint& point : construction->spline.getPoints())
		center += point.position;
	center /= static_cast<float>(construction->spline.getPointCount());

	configureGizmo(context);
	XMFLOAT4X4 cameraView;
	XMFLOAT4X4 cameraProjection;
	getCameraMatrices(context, cameraView, cameraProjection);

	ObjectTransformation splineTransform;
	splineTransform.position = center;

	XMFLOAT4X4 transform;
	XMStoreFloat4x4(&transform, splineTransform.createWorldMatrix());
	XMFLOAT4X4 transformDelta{};

	float snapValues[3] = { 0.5f, 0.5f, 0.5f };
	const ImGuizmo::OPERATION operation = static_cast<ImGuizmo::OPERATION>(ImGuizmo::TRANSLATE | ImGuizmo::ROTATE);
	const bool manipulated = ImGuizmo::Manipulate(&cameraView._11, &cameraProjection._11, operation, ImGuizmo::WORLD, &transform._11, &transformDelta._11, snapValues);
	tool.gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
	if (!manipulated)
		return;

	XMVECTOR scale;
	XMVECTOR rotation;
	XMVECTOR translation;
	if (!XMMatrixDecompose(&scale, &rotation, &translation, XMLoadFloat4x4(&transformDelta)))
		return;

	Quaternion deltaRotation;
	XMStoreFloat4(&deltaRotation, rotation);
	Vector3 deltaTranslation;
	XMStoreFloat3(&deltaTranslation, translation);

	for (size_t i = 0; i < construction->spline.getPointCount(); ++i)
	{
		SplinePoint point = construction->spline.getPoint(i);
		point.position = center + deltaRotation * (point.position - center) + deltaTranslation;
		construction->spline.setPoint(i, point);
	}

	construction->spline.setReferenceUp(deltaRotation * construction->spline.getReferenceUp());
	tool.markConstructionChanged();
}