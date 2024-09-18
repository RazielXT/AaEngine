#pragma once

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

inline Vector3 operator*(const Quaternion& q, const Vector3& v)
{
	return Vector3::Transform(v, q);
}
