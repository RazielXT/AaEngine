#pragma once

#include "Utils/MathUtils.h"

#include <cstddef>
#include <vector>

struct ShapeProfilePoint
{
	Vector2 position{};
	Vector2 normal = Vector2::UnitY;
	float profileDistance = 0.0f;
};

struct ShapeProfileContour
{
	std::vector<ShapeProfilePoint> points;
	bool closed = false;
};

class ShapeProfile2D
{
public:
	static ShapeProfile2D createRoad(float width);
	static ShapeProfile2D createRectangle(float width, float height);
	static ShapeProfile2D createCircle(float radius, size_t segments);
	static ShapeProfile2D createTube(float outerRadius, float innerRadius, size_t segments);
	static ShapeProfile2D createArc(float radius, float angleRadians, size_t segments);

	void clear();
	void addContour(const std::vector<Vector2>& positions, bool closed);
	void addContour(std::vector<ShapeProfilePoint> points, bool closed);

	const std::vector<ShapeProfileContour>& getContours() const { return contours; }
	const ShapeProfileContour& getContour(size_t index) const { return contours[index]; }
	size_t getContourCount() const { return contours.size(); }
	bool empty() const { return contours.empty(); }

private:
	static std::vector<ShapeProfilePoint> buildContourPoints(const std::vector<Vector2>& positions, bool closed);
	static void calculateProfileDistances(std::vector<ShapeProfilePoint>& points, bool closed);
	static void calculateContourNormals(std::vector<ShapeProfilePoint>& points, bool closed);

	std::vector<ShapeProfileContour> contours;
};