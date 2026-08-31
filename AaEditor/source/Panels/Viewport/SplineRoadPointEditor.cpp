#include "SplineRoadPointEditor.h"

#include "SplineRoadTool.h"
#include "Utils/Logger.h"

#include <algorithm>
#include <cmath>
#include <format>

void SplineRoadPointEditor::addPoint(SplineRoadTool& tool) const
{
	tool.ensureRoad();
	SplineConstruction* construction = tool.getRoad();
	auto& spline = construction->spline;
	const size_t pointCount = spline.getPointCount();

	if (pointCount == 0)
	{
		spline.addPoint({});
		tool.selectedPointIndex = 0;
		tool.markConstructionChanged();
		return;
	}

	switch (tool.addPointMode)
	{
	case SplineRoadTool::AddPointMode::InsertAfter:
		if (tool.selectedPointIndex != Spline::InvalidPointIndex && tool.selectedPointIndex + 1 < pointCount)
		{
			insertPointBetween(tool, tool.selectedPointIndex, tool.selectedPointIndex + 1);
			return;
		}
		break;
	case SplineRoadTool::AddPointMode::InsertBefore:
		if (tool.selectedPointIndex != Spline::InvalidPointIndex && tool.selectedPointIndex > 0 && tool.selectedPointIndex < pointCount)
		{
			insertPointBetween(tool, tool.selectedPointIndex - 1, tool.selectedPointIndex);
			return;
		}
		break;
	case SplineRoadTool::AddPointMode::ExtendStart:
		tool.selectedPointIndex = 0;
		break;
	case SplineRoadTool::AddPointMode::ExtendEnd:
		tool.selectedPointIndex = pointCount - 1;
		break;
	case SplineRoadTool::AddPointMode::Auto:
	default:
		if (tool.selectedPointIndex > 0 && tool.selectedPointIndex != Spline::InvalidPointIndex && tool.selectedPointIndex + 1 < pointCount)
		{
			insertPointBetween(tool, tool.selectedPointIndex, tool.selectedPointIndex + 1);
			return;
		}
		break;
	}

	const bool extendStart = shouldExtendStart(tool, pointCount);
	if (tool.replicateCurveOnAdd && addReplicatedCurvePoint(tool, extendStart))
	{
		tool.markConstructionChanged();
		return;
	}

	const Vector3 direction = getForwardAddDirection(tool, extendStart);
	if (extendStart)
	{
		const Vector3 position = spline.getPoint(0).position + direction * tool.addPointDistance;
		spline.insertPoint(0, position);
		tool.selectedPointIndex = 0;
	}
	else
	{
		const Vector3 position = spline.getPoint(pointCount - 1).position + direction * tool.addPointDistance;
		spline.addPoint(position);
		tool.selectedPointIndex = spline.getPointCount() - 1;
	}

	tool.markConstructionChanged();
}

void SplineRoadPointEditor::removeSelectedPoint(SplineRoadTool& tool) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction || tool.selectedPointIndex == Spline::InvalidPointIndex || tool.selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.removePoint(tool.selectedPointIndex);
	const size_t pointCount = construction->spline.getPointCount();
	tool.selectedPointIndex = pointCount ? (std::min)(tool.selectedPointIndex, pointCount - 1) : Spline::InvalidPointIndex;
	tool.markConstructionChanged();
}

void SplineRoadPointEditor::selectPointByOffset(SplineRoadTool& tool, int offset) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction)
		return;

	const size_t pointCount = construction->spline.getPointCount();
	if (pointCount == 0 || tool.selectedPointIndex == Spline::InvalidPointIndex || tool.selectedPointIndex >= pointCount)
		return;

	const int currentIndex = static_cast<int>(tool.selectedPointIndex);
	const int count = static_cast<int>(pointCount);
	tool.selectedPointIndex = static_cast<size_t>((currentIndex + offset + count) % count);
	tool.enterPointEditMode();
	Logger::log(std::format("Selected spline point {}", tool.selectedPointIndex));
}

void SplineRoadPointEditor::setSelectedPointPosition(SplineRoadTool& tool, Vector3 position) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction || tool.selectedPointIndex == Spline::InvalidPointIndex || tool.selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.setPoint(tool.selectedPointIndex, position);
	tool.markConstructionChanged();
}

void SplineRoadPointEditor::setSelectedPointRoll(SplineRoadTool& tool, float roll) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction || tool.selectedPointIndex == Spline::InvalidPointIndex || tool.selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.setPointRoll(tool.selectedPointIndex, roll);
	tool.markConstructionChanged();
}

void SplineRoadPointEditor::setSelectedPointUpStabilization(SplineRoadTool& tool, float upStabilization) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction || tool.selectedPointIndex == Spline::InvalidPointIndex || tool.selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.setPointUpStabilization(tool.selectedPointIndex, upStabilization);
	tool.markConstructionChanged();
}

void SplineRoadPointEditor::insertPointBetween(SplineRoadTool& tool, size_t previousPointIndex, size_t nextPointIndex) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction)
		return;

	auto& spline = construction->spline;
	const auto& previousPoint = spline.getPoint(previousPointIndex);
	const auto& nextPoint = spline.getPoint(nextPointIndex);
	SplinePoint insertedPoint;
	insertedPoint.position = (previousPoint.position + nextPoint.position) * 0.5f;
	insertedPoint.roll = (previousPoint.roll + nextPoint.roll) * 0.5f;
	insertedPoint.upStabilization = (previousPoint.upStabilization + nextPoint.upStabilization) * 0.5f;
	spline.insertPoint(nextPointIndex, insertedPoint.position, insertedPoint.roll);
	spline.setPoint(nextPointIndex, insertedPoint);
	tool.selectedPointIndex = nextPointIndex;
	tool.markConstructionChanged();
}

bool SplineRoadPointEditor::addReplicatedCurvePoint(SplineRoadTool& tool, bool extendStart) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction)
		return false;

	auto& spline = construction->spline;
	const size_t pointCount = spline.getPointCount();
	if (pointCount < 3)
		return false;

	const size_t i0 = extendStart ? 2 : pointCount - 3;
	const size_t i1 = extendStart ? 1 : pointCount - 2;
	const size_t i2 = extendStart ? 0 : pointCount - 1;
	const SplinePoint p0 = spline.getPoint(i0);
	const SplinePoint p1 = spline.getPoint(i1);
	const SplinePoint p2 = spline.getPoint(i2);

	Vector3 previous = p1.position - p0.position;
	Vector3 current = p2.position - p1.position;
	if (previous.LengthSquared() <= 0.000001f || current.LengthSquared() <= 0.000001f)
		return false;

	previous.Normalize();
	current.Normalize();
	Vector3 axis = previous.Cross(current);
	const float axisLength = axis.Length();
	Quaternion replicatedRotation = Quaternion::Identity;
	if (axisLength > 0.000001f)
	{
		axis /= axisLength;
		replicatedRotation = Quaternion::CreateFromAxisAngle(axis, std::atan2(axisLength, std::clamp(previous.Dot(current), -1.0f, 1.0f)));
	}

	Vector3 offset = replicatedRotation * (p2.position - p1.position);
	SplinePoint newPoint = p2;
	newPoint.position = p2.position + offset;
	newPoint.roll = p2.roll + (p2.roll - p1.roll);
	newPoint.upStabilization = std::clamp(p2.upStabilization + (p2.upStabilization - p1.upStabilization), 0.0f, 1.0f);

	if (extendStart)
	{
		newPoint.position = p2.position - offset;
		spline.insertPoint(0, newPoint.position, newPoint.roll);
		spline.setPoint(0, newPoint);
		tool.selectedPointIndex = 0;
	}
	else
	{
		spline.addPoint(newPoint.position, newPoint.roll);
		spline.setPoint(spline.getPointCount() - 1, newPoint);
		tool.selectedPointIndex = spline.getPointCount() - 1;
	}

	return true;
}

bool SplineRoadPointEditor::shouldExtendStart(const SplineRoadTool& tool, size_t pointCount) const
{
	if (tool.addPointMode == SplineRoadTool::AddPointMode::ExtendStart)
		return true;
	if (tool.addPointMode == SplineRoadTool::AddPointMode::ExtendEnd)
		return false;

	return tool.selectedPointIndex == 0 && pointCount > 1;
}

Vector3 SplineRoadPointEditor::getForwardAddDirection(const SplineRoadTool& tool, bool extendStart) const
{
	const SplineConstruction* construction = tool.getRoad();
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