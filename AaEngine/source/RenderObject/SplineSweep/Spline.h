#pragma once

#include "Utils/MathUtils.h"

#include <cstddef>
#include <vector>

enum class SplineInterpolationMode
{
	Linear,
	CatmullRom,
};

struct SplinePoint
{
	Vector3 position{};
	float roll = 0.0f;
};

struct SplineSample
{
	Vector3 position{};
	Vector3 tangent = Vector3::UnitZ;
	Vector3 right = Vector3::UnitX;
	Vector3 up = Vector3::UnitY;
	float normalizedT = 0.0f;
	float localT = 0.0f;
	float distance = 0.0f;
	float roll = 0.0f;
	size_t segmentIndex = 0;
};

class Spline
{
public:
	void addPoint(Vector3 position, float roll = 0.0f);
	void insertPoint(size_t index, Vector3 position, float roll = 0.0f);
	void setPoint(size_t index, Vector3 position);
	void setPoint(size_t index, SplinePoint point);
	void setPointRoll(size_t index, float roll);
	void removePoint(size_t index);
	void clear();

	const std::vector<SplinePoint>& getPoints() const { return points; }
	const SplinePoint& getPoint(size_t index) const { return points[index]; }
	size_t getPointCount() const { return points.size(); }

	void setClosed(bool value);
	bool isClosed() const { return closed; }

	void setInterpolationMode(SplineInterpolationMode value);
	SplineInterpolationMode getInterpolationMode() const { return interpolationMode; }

	void setTessellationSegments(size_t value);
	size_t getTessellationSegments() const { return tessellationSegments; }
	void setAdaptiveTessellation(float curvatureScale, size_t maxSegments);
	float getAdaptiveTessellationCurvatureScale() const { return adaptiveTessellationCurvatureScale; }
	size_t getMaxTessellationSegments() const { return maxTessellationSegments; }

	void setReferenceUp(Vector3 value);
	Vector3 getReferenceUp() const { return referenceUp; }

	SplineSample evaluate(float normalizedT) const;
	Vector3 evaluatePosition(float normalizedT) const { return evaluate(normalizedT).position; }
	SplineSample evaluateByDistance(float distance) const;

	const std::vector<SplineSample>& getSamples() const;
	float getLength() const;
	size_t getSegmentCount() const;
	bool isDirty() const { return dirty; }
	void rebuildSamples() const;

private:
	SplineSample evaluateSegment(size_t segmentIndex, float localT) const;
	SplineSample createSample(Vector3 position, Vector3 tangent, float roll) const;
	float evaluateRoll(size_t segmentIndex, float localT) const;
	size_t getSegmentTessellation(size_t segmentIndex) const;
	float getSegmentCurvature(size_t segmentIndex) const;
	const SplinePoint& getSegmentPoint(int index) const;
	void invalidate();

	std::vector<SplinePoint> points;
	SplineInterpolationMode interpolationMode = SplineInterpolationMode::CatmullRom;
	size_t tessellationSegments = 8;
	size_t maxTessellationSegments = 8;
	float adaptiveTessellationCurvatureScale = 0.0f;
	Vector3 referenceUp = Vector3::UnitY;
	bool closed = false;

	mutable std::vector<SplineSample> samples;
	mutable float cachedLength = 0.0f;
	mutable bool dirty = true;
};