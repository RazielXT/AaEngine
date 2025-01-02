#include "MathUtils.h"
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