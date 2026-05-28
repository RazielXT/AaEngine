#include "RenderObject/ShapeProfile.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;

	Vector2 normalizeOrFallback(Vector2 value, Vector2 fallback)
	{
		if (value.LengthSquared() <= 0.000001f)
			return fallback;

		value.Normalize();
		return value;
	}

	Vector2 leftNormal(Vector2 value)
	{
		return { -value.y, value.x };
	}

	Vector2 rightNormal(Vector2 value)
	{
		return { value.y, -value.x };
	}

	float signedArea(const std::vector<ShapeProfilePoint>& points)
	{
		float area = 0.0f;
		for (size_t i = 0; i < points.size(); ++i)
		{
			const Vector2& a = points[i].position;
			const Vector2& b = points[(i + 1) % points.size()].position;
			area += a.x * b.y - b.x * a.y;
		}

		return area * 0.5f;
	}

	float clampPositive(float value)
	{
		return (std::max)(value, 0.0f);
	}
}

ShapeProfile2D ShapeProfile2D::createRoad(float width)
{
	ShapeProfile2D profile;
	width = clampPositive(width);
	profile.addContour({ { -width * 0.5f, 0.0f }, { width * 0.5f, 0.0f } }, false);
	return profile;
}

ShapeProfile2D ShapeProfile2D::createRectangle(float width, float height)
{
	ShapeProfile2D profile;
	width = clampPositive(width);
	height = clampPositive(height);

	profile.addContour({
		{ -width * 0.5f, -height * 0.5f },
		{ width * 0.5f, -height * 0.5f },
		{ width * 0.5f, height * 0.5f },
		{ -width * 0.5f, height * 0.5f },
	}, true);

	return profile;
}

ShapeProfile2D ShapeProfile2D::createCircle(float radius, size_t segments)
{
	ShapeProfile2D profile;
	radius = clampPositive(radius);
	if (segments < 3)
		segments = 3;

	std::vector<ShapeProfilePoint> points;
	points.reserve(segments);
	for (size_t i = 0; i < segments; ++i)
	{
		const float angle = static_cast<float>(i) / static_cast<float>(segments) * Pi * 2.0f;
		const Vector2 normal = { std::cos(angle), std::sin(angle) };
		points.push_back({ normal * radius, normal });
	}

	calculateProfileDistances(points, true);
	profile.addContour(std::move(points), true);
	return profile;
}

ShapeProfile2D ShapeProfile2D::createTube(float outerRadius, float innerRadius, size_t segments)
{
	ShapeProfile2D profile;
	outerRadius = clampPositive(outerRadius);
	innerRadius = std::clamp(innerRadius, 0.0f, outerRadius);
	if (segments < 3)
		segments = 3;

	std::vector<ShapeProfilePoint> outerPoints;
	std::vector<ShapeProfilePoint> innerPoints;
	outerPoints.reserve(segments);
	innerPoints.reserve(segments);

	for (size_t i = 0; i < segments; ++i)
	{
		const float angle = static_cast<float>(i) / static_cast<float>(segments) * Pi * 2.0f;
		const Vector2 normal = { std::cos(angle), std::sin(angle) };
		outerPoints.push_back({ normal * outerRadius, normal });
	}

	for (size_t i = 0; i < segments; ++i)
	{
		const float angle = -static_cast<float>(i) / static_cast<float>(segments) * Pi * 2.0f;
		const Vector2 radial = { std::cos(angle), std::sin(angle) };
		innerPoints.push_back({ radial * innerRadius, -radial });
	}

	calculateProfileDistances(outerPoints, true);
	calculateProfileDistances(innerPoints, true);
	profile.addContour(std::move(outerPoints), true);
	profile.addContour(std::move(innerPoints), true);
	return profile;
}

ShapeProfile2D ShapeProfile2D::createArc(float radius, float angleRadians, size_t segments)
{
	ShapeProfile2D profile;
	radius = clampPositive(radius);
	if (segments < 1)
		segments = 1;

	const float startAngle = -angleRadians * 0.5f;
	std::vector<ShapeProfilePoint> points;
	points.reserve(segments + 1);

	for (size_t i = 0; i <= segments; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(segments);
		const float angle = startAngle + angleRadians * t;
		const Vector2 normal = { std::cos(angle), std::sin(angle) };
		points.push_back({ normal * radius, normal });
	}

	calculateProfileDistances(points, false);
	profile.addContour(std::move(points), false);
	return profile;
}

void ShapeProfile2D::clear()
{
	contours.clear();
}

void ShapeProfile2D::addContour(const std::vector<Vector2>& positions, bool closed)
{
	contours.push_back({ buildContourPoints(positions, closed), closed });
}

void ShapeProfile2D::addContour(std::vector<ShapeProfilePoint> points, bool closed)
{
	calculateProfileDistances(points, closed);
	contours.push_back({ std::move(points), closed });
}

std::vector<ShapeProfilePoint> ShapeProfile2D::buildContourPoints(const std::vector<Vector2>& positions, bool closed)
{
	std::vector<ShapeProfilePoint> points;
	points.reserve(positions.size());
	for (const Vector2& position : positions)
		points.push_back({ position });

	calculateProfileDistances(points, closed);
	calculateContourNormals(points, closed);
	return points;
}

void ShapeProfile2D::calculateProfileDistances(std::vector<ShapeProfilePoint>& points, bool closed)
{
	if (points.empty())
		return;

	float distance = 0.0f;
	points.front().profileDistance = 0.0f;
	for (size_t i = 1; i < points.size(); ++i)
	{
		distance += (points[i].position - points[i - 1].position).Length();
		points[i].profileDistance = distance;
	}

	if (closed && points.size() > 1)
		distance += (points.front().position - points.back().position).Length();
}

void ShapeProfile2D::calculateContourNormals(std::vector<ShapeProfilePoint>& points, bool closed)
{
	if (points.size() < 2)
		return;

	const bool counterClockwise = closed && signedArea(points) > 0.0f;
	for (size_t i = 0; i < points.size(); ++i)
	{
		const size_t previousIndex = i > 0 ? i - 1 : (closed ? points.size() - 1 : i);
		const size_t nextIndex = i + 1 < points.size() ? i + 1 : (closed ? 0 : i);

		const Vector2 previousDirection = normalizeOrFallback(points[i].position - points[previousIndex].position, Vector2::UnitX);
		const Vector2 nextDirection = normalizeOrFallback(points[nextIndex].position - points[i].position, previousDirection);
		Vector2 previousNormal = counterClockwise ? rightNormal(previousDirection) : leftNormal(previousDirection);
		Vector2 nextNormal = counterClockwise ? rightNormal(nextDirection) : leftNormal(nextDirection);
		points[i].normal = normalizeOrFallback(previousNormal + nextNormal, nextNormal);
	}
}