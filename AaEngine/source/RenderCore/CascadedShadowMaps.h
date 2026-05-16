#pragma once

#include "Scene/Camera.h"

struct ShadowMapCascade
{
	void update(Camera& light, Camera& viewer, float extends, Vector2& nearFarClip, float shadowMapSize);

	DirectX::XMMATRIX matShadowProj[4];
	DirectX::XMMATRIX matShadowView;

	const float cascadePartitionsZeroToOne[4] = { 50 / 8.f, 200 / 8.f, 1000 / 8.f, 6000 / 8.f };
	const int cascadesCount = _countof(cascadePartitionsZeroToOne);
};