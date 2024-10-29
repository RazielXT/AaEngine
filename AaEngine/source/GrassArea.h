#pragma once

#include "ShaderConstantBuffers.h"
#include <DirectXCollision.h>

class GrassArea
{
public:

	GrassArea() = default;

	void initialize();
	UINT getVertexCount() const;

	UINT count{};
	CbufferView cbuffer;
	DirectX::BoundingBox bbox;
};