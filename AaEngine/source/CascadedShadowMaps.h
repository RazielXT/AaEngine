#pragma once

#include "Camera.h"

struct ShadowMapCascade
{
	void update(Camera& light, Camera& viewer, float extends, Vector2& nearFarClip);

	DirectX::XMMATRIX matShadowProj[4];
	DirectX::XMMATRIX matShadowView;

	const int cascadePartitionsZeroToOne[3] = { 50, 200, 1000 };
	const int cascadesCount = _countof(cascadePartitionsZeroToOne);
};