#pragma once

#include "Utils/MathUtils.h"

class MovableBody
{
public:

	virtual ~MovableBody() = default;

	virtual Vector3 getPosition() const = 0;
	virtual Vector3 getDirection() const = 0;
	virtual Vector3 getVelocity() const = 0;
};
