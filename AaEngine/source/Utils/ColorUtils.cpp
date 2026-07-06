#include "ColorUtils.h"

float toSrgb(float v)
{
	return pow(v, 2.233333333);
}

float fromSrgb(float v)
{
	return std::max(1.055f * powf(v, 0.416666667) - 0.055f, 0.f);
}

Vector3 toSrgb(Vector3 v)
{
	return { toSrgb(v.x), toSrgb(v.y), toSrgb(v.z) };
}

Vector3 getRandomSrgbColor()
{
	return toSrgb({ getRandomFloat(0.2f, 1.0f), getRandomFloat(0.2f, 1.0f), getRandomFloat(0.2f, 1.0f) });
}
