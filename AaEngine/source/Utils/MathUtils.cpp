#include "Utils/MathUtils.h"
#include <random>
#include <cmath>

float getRandomAngleInRadians()
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0, 2.0 * 3.14);

	return static_cast<float>(dis(gen));
}

float getRandomFloat(float min, float max)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(min, max);

	return static_cast<float>(dis(gen));
}

float getRandomFloat01()
{
	return getRandomFloat(0.0f, 1.0f);
}

Quaternion getRandomQuaternion()
{
	float u1 = getRandomFloat01();
	float u2 = getRandomFloat01();
	float u3 = getRandomFloat01();

	float sqrt1 = sqrtf(1.0f - u1);
	float sqrt2 = sqrtf(u1);

	float theta1 = 2.0f * 3.14159265f * u2;
	float theta2 = 2.0f * 3.14159265f * u3;

	return Quaternion(
		cosf(theta2) * sqrt2,
		sinf(theta1) * sqrt1,
		cosf(theta1) * sqrt1,
		sinf(theta2) * sqrt2
	);
}
