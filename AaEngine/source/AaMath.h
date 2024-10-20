#pragma once

#include <SimpleMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

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
