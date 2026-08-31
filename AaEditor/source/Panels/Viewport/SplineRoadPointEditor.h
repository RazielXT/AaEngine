#pragma once

#include "Utils/MathUtils.h"

#include <cstddef>

class SplineRoadTool;

class SplineRoadPointEditor
{
public:
	void addPoint(SplineRoadTool& tool) const;
	void removeSelectedPoint(SplineRoadTool& tool) const;
	void selectPointByOffset(SplineRoadTool& tool, int offset) const;
	void setSelectedPointPosition(SplineRoadTool& tool, Vector3 position) const;
	void setSelectedPointRoll(SplineRoadTool& tool, float roll) const;
	void setSelectedPointUpStabilization(SplineRoadTool& tool, float upStabilization) const;

private:
	void insertPointBetween(SplineRoadTool& tool, size_t previousPointIndex, size_t nextPointIndex) const;
	bool addReplicatedCurvePoint(SplineRoadTool& tool, bool extendStart) const;
	bool shouldExtendStart(const SplineRoadTool& tool, size_t pointCount) const;
	Vector3 getForwardAddDirection(const SplineRoadTool& tool, bool extendStart) const;
};