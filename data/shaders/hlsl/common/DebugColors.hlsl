#pragma once

static const uint DEBUG_COLOR_COUNT = 19;

static const float3 DebugColors[DEBUG_COLOR_COUNT] = {
	float3(1.00, 0.00, 0.00), // 0  Red
	float3(0.00, 0.60, 0.10), // 1  Green
	float3(0.00, 0.20, 1.00), // 2  Blue
	float3(1.00, 0.80, 0.00), // 3  Gold
	float3(0.20, 0.00, 1.00), // 4  Indigo
	float3(1.00, 0.20, 0.60), // 5  Hot Pink
	float3(0.60, 0.30, 0.00), // 6  Brown
	float3(0.00, 1.00, 0.40), // 7  Mint
	float3(0.80, 0.00, 0.80), // 8  Purple
	float3(1.00, 1.00, 1.00), // 9  White
	float3(0.00, 0.80, 0.80), // 10 Cyan
	float3(1.00, 0.50, 0.00), // 11 Orange
	float3(0.50, 1.00, 0.00), // 12 Lime
	float3(0.40, 0.00, 0.20), // 13 Maroon
	float3(1.00, 1.00, 0.30), // 14 Yellow
	float3(0.00, 0.40, 0.60), // 15 Teal
	float3(1.00, 0.40, 0.40), // 16 Salmon
	float3(0.60, 0.60, 1.00), // 17 Lavender
	float3(0.00, 0.00, 0.50), // 18 Navy
};

float3 GetDebugColor(uint index)
{
	return DebugColors[index % DEBUG_COLOR_COUNT];
}
