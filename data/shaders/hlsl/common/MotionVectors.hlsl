#pragma once

// Encodes a motion vector from clip-space previous/current positions and viewport size.
// Result is in pixel units, with Y flipped to match screen-space convention.
float4 EncodeMotionVector(float4 previousClip, float4 currentClip, uint2 viewportSize)
{
	float2 mv = (previousClip.xy / previousClip.w - currentClip.xy / currentClip.w) * viewportSize;
	mv *= 0.5;
	mv.y *= -1;
	return float4(mv, 0, 0);
}

// Returns a zeroed motion vector for objects that have no previous-frame position
// (e.g. particles, billboards, static geometry that doesn't output velocity).
float4 ZeroMotionVector()
{
	return float4(0, 0, 0, 0);
}
