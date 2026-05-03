#pragma once

#include "MathUtils.h"

float toSrgb(float v);
float fromSrgb(float v);

Vector3 toSrgb(Vector3);

Vector3 getRandomSrgbColor();