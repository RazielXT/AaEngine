#pragma once

#include "Camera.h"

struct ShadowMapCascade
{
	void update(Camera& light, Camera& viewer);

	DirectX::XMMATRIX m_matShadowProj[4];
	DirectX::XMMATRIX m_matShadowView;
};