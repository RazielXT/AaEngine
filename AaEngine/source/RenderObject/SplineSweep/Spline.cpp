#include "RenderObject/SplineSweep/Spline.h"

#include <algorithm>
#include <cmath>

namespace
{
	Vector3 normalizeOrFallback(Vector3 value, Vector3 fallback)
	{
		if (value.LengthSquared() <= 0.000001f)
			return fallback;

		value.Normalize();
		return value;
	}

	float clamp01(float value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}
}

void Spline::addPoint(Vector3 position, float roll)
{
	points.push_back({ position, roll });
	invalidate();
}

void Spline::insertPoint(size_t index, Vector3 position, float roll)
{
	index = (std::min)(index, points.size());
	points.insert(points.begin() + index, { position, roll });
	invalidate();
}

void Spline::setPoint(size_t index, Vector3 position)
{
	points[index].position = position;
	invalidate();
}

void Spline::setPoint(size_t index, SplinePoint point)
{
	points[index] = point;
	invalidate();
}

void Spline::setPointRoll(size_t index, float roll)
{
	points[index].roll = roll;
	invalidate();
}

void Spline::setPointUpStabilization(size_t index, float upStabilization)
{
	points[index].upStabilization = clamp01(upStabilization);
	invalidate();
}

void Spline::removePoint(size_t index)
{
	points.erase(points.begin() + index);
	invalidate();
}

void Spline::clear()
{
	points.clear();
	invalidate();
}

void Spline::setClosed(bool value)
{
	if (closed == value)
		return;

	closed = value;
	invalidate();
}

void Spline::setInterpolationMode(SplineInterpolationMode value)
{
	if (interpolationMode == value)
		return;

	interpolationMode = value;
	invalidate();
}

void Spline::setTessellationSegments(size_t value)
{
	if (value < 1)
		value = 1;
	if (tessellationSegments == value)
		return;

	tessellationSegments = value;
	maxTessellationSegments = (std::max)(maxTessellationSegments, tessellationSegments);
	invalidate();
}

void Spline::setAdaptiveTessellation(float curvatureScale, size_t maxSegments, float rollScale)
{
	curvatureScale = (std::max)(0.0f, curvatureScale);
	rollScale = (std::max)(0.0f, rollScale);
	maxSegments = (std::max)(maxSegments, tessellationSegments);

	if (adaptiveTessellationCurvatureScale == curvatureScale && adaptiveTessellationRollScale == rollScale && maxTessellationSegments == maxSegments)
		return;

	adaptiveTessellationCurvatureScale = curvatureScale;
	adaptiveTessellationRollScale = rollScale;
	maxTessellationSegments = maxSegments;
	invalidate();
}

void Spline::setReferenceUp(Vector3 value)
{
	referenceUp = normalizeOrFallback(value, Vector3::UnitY);
	invalidate();
}

SplineSample Spline::evaluate(float normalizedT) const
{
	if (points.empty())
		return {};

	if (points.size() == 1)
		return createSample(points.front().position, Vector3::UnitZ, points.front().roll, points.front().upStabilization);

	const size_t segmentCount = getSegmentCount();
	if (!segmentCount)
		return createSample(points.front().position, Vector3::UnitZ, points.front().roll, points.front().upStabilization);

	normalizedT = clamp01(normalizedT);
	const float scaledT = normalizedT * static_cast<float>(segmentCount);
	const size_t segmentIndex = (std::min)(static_cast<size_t>(std::floor(scaledT)), segmentCount - 1);
	const float localT = segmentIndex == segmentCount - 1 && normalizedT >= 1.0f ? 1.0f : scaledT - static_cast<float>(segmentIndex);

	SplineSample sample = evaluateSegment(segmentIndex, localT);
	sample.normalizedT = normalizedT;
	sample.localT = localT;
	sample.segmentIndex = segmentIndex;
	return sample;
}

SplineSample Spline::evaluateByDistance(float distance) const
{
	rebuildSamples();

	if (samples.empty())
		return {};

	if (distance <= 0.0f)
		return samples.front();

	if (distance >= cachedLength)
		return samples.back();

	auto next = std::lower_bound(samples.begin(), samples.end(), distance, [](const SplineSample& sample, float value)
	{
		return sample.distance < value;
	});

	if (next == samples.begin())
		return *next;

	const auto previous = next - 1;
	const float spanLength = next->distance - previous->distance;
	const float spanT = spanLength > 0.000001f ? (distance - previous->distance) / spanLength : 0.0f;
	SplineSample sample;
	sample.position = Vector3::Lerp(previous->position, next->position, spanT);
	sample.tangent = normalizeOrFallback(Vector3::Lerp(previous->tangent, next->tangent, spanT), previous->tangent);
	sample.right = normalizeOrFallback(Vector3::Lerp(previous->right, next->right, spanT), previous->right);
	sample.up = normalizeOrFallback(sample.tangent.Cross(sample.right), previous->up);
	sample.normalizedT = previous->normalizedT + (next->normalizedT - previous->normalizedT) * spanT;
	sample.localT = previous->localT + (next->localT - previous->localT) * spanT;
	sample.distance = distance;
	sample.roll = previous->roll + (next->roll - previous->roll) * spanT;
	sample.upStabilization = previous->upStabilization + (next->upStabilization - previous->upStabilization) * spanT;
	sample.segmentIndex = previous->segmentIndex;
	return sample;
}

const std::vector<SplineSample>& Spline::getSamples() const
{
	rebuildSamples();
	return samples;
}

float Spline::getLength() const
{
	rebuildSamples();
	return cachedLength;
}

size_t Spline::getSegmentCount() const
{
	if (points.size() < 2)
		return 0;

	return closed ? points.size() : points.size() - 1;
}

size_t Spline::getClosestPointIndex(Vector3 position, float* distanceSquared) const
{
	if (points.empty())
	{
		if (distanceSquared)
			*distanceSquared = 0.0f;
		return InvalidPointIndex;
	}

	size_t closestIndex = 0;
	float closestDistanceSquared = (points.front().position - position).LengthSquared();
	for (size_t i = 1; i < points.size(); ++i)
	{
		const float currentDistanceSquared = (points[i].position - position).LengthSquared();
		if (currentDistanceSquared < closestDistanceSquared)
		{
			closestDistanceSquared = currentDistanceSquared;
			closestIndex = i;
		}
	}

	if (distanceSquared)
		*distanceSquared = closestDistanceSquared;
	return closestIndex;
}

void Spline::rebuildSamples() const
{
	if (!dirty)
		return;

	samples.clear();
	cachedLength = 0.0f;

	if (points.empty())
	{
		dirty = false;
		return;
	}

	const size_t segmentCount = getSegmentCount();
	if (!segmentCount)
	{
		samples.push_back(createSample(points.front().position, Vector3::UnitZ, points.front().roll, points.front().upStabilization));
		dirty = false;
		return;
	}

	samples.reserve(segmentCount * maxTessellationSegments + 1);
	SplineSample previous = evaluate(0.0f);
	samples.push_back(previous);

	for (size_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
	{
		const size_t segmentTessellation = getSegmentTessellation(segmentIndex);
		for (size_t step = 1; step <= segmentTessellation; ++step)
		{
			const float localT = static_cast<float>(step) / static_cast<float>(segmentTessellation);
			const float normalizedT = (static_cast<float>(segmentIndex) + localT) / static_cast<float>(segmentCount);
			SplineSample sample = evaluate(normalizedT);
			blendSampleWithTransportedFrame(sample, previous);
			cachedLength += (sample.position - previous.position).Length();
			sample.distance = cachedLength;
			samples.push_back(sample);
			previous = sample;
		}
	}

	dirty = false;
}

SplineSample Spline::evaluateSegment(size_t segmentIndex, float localT) const
{
	const Vector3 p0 = getSegmentPoint(static_cast<int>(segmentIndex) - 1).position;
	const Vector3 p1 = getSegmentPoint(static_cast<int>(segmentIndex)).position;
	const Vector3 p2 = getSegmentPoint(static_cast<int>(segmentIndex) + 1).position;
	const Vector3 p3 = getSegmentPoint(static_cast<int>(segmentIndex) + 2).position;
	const float roll = evaluateRoll(segmentIndex, localT);
	const float upStabilization = evaluateUpStabilization(segmentIndex, localT);

	if (interpolationMode == SplineInterpolationMode::Linear)
	{
		return createSample(Vector3::Lerp(p1, p2, localT), normalizeOrFallback(p2 - p1, Vector3::UnitZ), roll, upStabilization);
	}

	const float localT2 = localT * localT;
	const float localT3 = localT2 * localT;

	const Vector3 position = (p1 * 2.0f + (p2 - p0) * localT + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * localT2 + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * localT3) * 0.5f;
	const Vector3 tangent = (p2 - p0 + (p0 * 4.0f - p1 * 10.0f + p2 * 8.0f - p3 * 2.0f) * localT + (p1 * 9.0f - p0 * 3.0f - p2 * 9.0f + p3 * 3.0f) * localT2) * 0.5f;

	return createSample(position, normalizeOrFallback(tangent, normalizeOrFallback(p2 - p1, Vector3::UnitZ)), roll, upStabilization);
}

SplineSample Spline::createSample(Vector3 position, Vector3 tangent, float roll, float upStabilization) const
{
	tangent = normalizeOrFallback(tangent, Vector3::UnitZ);
	upStabilization = clamp01(upStabilization);

	Vector3 frameRight = referenceUp.Cross(tangent);
	if (frameRight.LengthSquared() <= 0.000001f)
	{
		const Vector3 fallbackUp = std::abs(tangent.Dot(Vector3::UnitY)) > 0.95f ? Vector3::UnitX : Vector3::UnitY;
		frameRight = fallbackUp.Cross(tangent);
	}

	frameRight = normalizeOrFallback(frameRight, Vector3::UnitX);
	Vector3 frameUp = normalizeOrFallback(tangent.Cross(frameRight), referenceUp);

	if (std::abs(roll) > 0.000001f)
	{
		const Quaternion rollRotation = Quaternion::CreateFromAxisAngle(tangent, roll);
		frameRight = rollRotation * frameRight;
		frameUp = rollRotation * frameUp;
	}

	return { position, tangent, frameRight, frameUp, 0.0f, 0.0f, 0.0f, roll, upStabilization, 0 };
}

void Spline::blendSampleWithTransportedFrame(SplineSample& sample, const SplineSample& previous) const
{
	Vector3 transportedRight = previous.right;
	Vector3 transportedUp = previous.up;
	const Vector3 rotationAxis = previous.tangent.Cross(sample.tangent);
	const float axisLength = rotationAxis.Length();
	if (axisLength > 0.000001f)
	{
		const float dot = std::clamp(previous.tangent.Dot(sample.tangent), -1.0f, 1.0f);
		const Quaternion tangentRotation = Quaternion::CreateFromAxisAngle(rotationAxis / axisLength, std::atan2(axisLength, dot));
		transportedRight = tangentRotation * transportedRight;
		transportedUp = tangentRotation * transportedUp;
	}

	const float rollDelta = sample.roll - previous.roll;
	if (std::abs(rollDelta) > 0.000001f)
	{
		const Quaternion rollRotation = Quaternion::CreateFromAxisAngle(sample.tangent, rollDelta);
		transportedRight = rollRotation * transportedRight;
		transportedUp = rollRotation * transportedUp;
	}

	const float stabilization = clamp01(sample.upStabilization);
	Vector3 blendedRight = transportedRight * (1.0f - stabilization) + sample.right * stabilization;
	blendedRight -= sample.tangent * blendedRight.Dot(sample.tangent);
	if (blendedRight.LengthSquared() <= 0.000001f)
		blendedRight = transportedUp.Cross(sample.tangent);

	sample.right = normalizeOrFallback(blendedRight, sample.right);
	sample.up = normalizeOrFallback(sample.tangent.Cross(sample.right), sample.up);
}

float Spline::evaluateRoll(size_t segmentIndex, float localT) const
{
	const float startRoll = getSegmentPoint(static_cast<int>(segmentIndex)).roll;
	const float endRoll = getSegmentPoint(static_cast<int>(segmentIndex) + 1).roll;
	return startRoll + (endRoll - startRoll) * localT;
}

float Spline::evaluateUpStabilization(size_t segmentIndex, float localT) const
{
	const float startStabilization = getSegmentPoint(static_cast<int>(segmentIndex)).upStabilization;
	const float endStabilization = getSegmentPoint(static_cast<int>(segmentIndex) + 1).upStabilization;
	return clamp01(startStabilization + (endStabilization - startStabilization) * localT);
}

size_t Spline::getSegmentTessellation(size_t segmentIndex) const
{
	if ((adaptiveTessellationCurvatureScale <= 0.0f && adaptiveTessellationRollScale <= 0.0f) || maxTessellationSegments <= tessellationSegments)
		return tessellationSegments;

	const float curvature = getSegmentCurvature(segmentIndex);
	const float rollDelta = getSegmentRollDelta(segmentIndex);
	const size_t extraSegments = static_cast<size_t>(std::ceil(curvature * adaptiveTessellationCurvatureScale + rollDelta * adaptiveTessellationRollScale));
	return (std::min)(maxTessellationSegments, tessellationSegments + extraSegments);
}

float Spline::getSegmentCurvature(size_t segmentIndex) const
{
	const Vector3 p0 = getSegmentPoint(static_cast<int>(segmentIndex) - 1).position;
	const Vector3 p1 = getSegmentPoint(static_cast<int>(segmentIndex)).position;
	const Vector3 p2 = getSegmentPoint(static_cast<int>(segmentIndex) + 1).position;
	const Vector3 p3 = getSegmentPoint(static_cast<int>(segmentIndex) + 2).position;

	const Vector3 incoming = normalizeOrFallback(p1 - p0, normalizeOrFallback(p2 - p1, Vector3::UnitZ));
	const Vector3 current = normalizeOrFallback(p2 - p1, incoming);
	const Vector3 outgoing = normalizeOrFallback(p3 - p2, current);

	const float startDot = std::clamp(incoming.Dot(current), -1.0f, 1.0f);
	const float endDot = std::clamp(current.Dot(outgoing), -1.0f, 1.0f);
	return std::acos(startDot) + std::acos(endDot);
}

float Spline::getSegmentRollDelta(size_t segmentIndex) const
{
	const float startRoll = getSegmentPoint(static_cast<int>(segmentIndex)).roll;
	const float endRoll = getSegmentPoint(static_cast<int>(segmentIndex) + 1).roll;
	return std::abs(endRoll - startRoll);
}

const SplinePoint& Spline::getSegmentPoint(int index) const
{
	if (closed)
	{
		const int pointCount = static_cast<int>(points.size());
		index %= pointCount;
		if (index < 0)
			index += pointCount;

		return points[index];
	}

	index = std::clamp(index, 0, static_cast<int>(points.size()) - 1);
	return points[index];
}

void Spline::invalidate()
{
	dirty = true;
}