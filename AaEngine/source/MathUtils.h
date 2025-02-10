#pragma once

#include <SimpleMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

inline XMFLOAT3& operator*=(XMFLOAT3& v, const Vector3& v2)
{
	v.x *= v2.x;
	v.y *= v2.y;
	v.z *= v2.z;
	return v;
}

inline Vector3 operator*(const Quaternion& q, const Vector3& v)
{
	return Vector3::Transform(v, q);
}

inline XMFLOAT3& operator+=(XMFLOAT3& l, const XMFLOAT3& r)
{
	l.x += r.x;
	l.y += r.y;
	l.z += r.z;
	return l;
}

struct BoundingBoxVolume
{
	Vector3 min{};
	Vector3 max{};

	BoundingBoxVolume() = default;

	BoundingBoxVolume(const BoundingBox& bbox)
	{
		min = max = bbox.Center;
		min -= Vector3(bbox.Extents);
		max += Vector3(bbox.Extents);
	}

	BoundingBoxVolume(Vector3 _min, Vector3 _max)
	{
		min = _min;
		max = _max;
	}

	BoundingBoxVolume(Vector3 start)
	{
		min = max = start;
	}

	void add(Vector3 point)
	{
		min = Vector3::Min(min, point);
		max = Vector3::Max(max, point);
	}

	BoundingBox createBbox() const
	{
		BoundingBox bbox;
		bbox.Center = (max + min) / 2;
		bbox.Extents = (max - min) / 2;

		return bbox;
	}
};

float getRandomAngleInRadians();
float getRandomFloat(float min, float max);

template <typename T>
constexpr T constexpr_pow(T base, int exp)
{
	return (exp == 0) ? 1 :
		(exp % 2 == 0) ? constexpr_pow(base * base, exp / 2) :
		base * constexpr_pow(base * base, (exp - 1) / 2);
}
