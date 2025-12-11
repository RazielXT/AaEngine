#pragma once

#include "MathUtils.h"
#include "RenderContext.h"

class AssignedMaterial;

struct ScreenQuad
{
	void Render(AssignedMaterial* material, const GpuTexture2D& target, const RenderProvider& provider, RenderContext& ctx, ID3D12GraphicsCommandList* commandList) const;

	enum RelativePosition { BottomLeft = 0, Top = 1, Right = 2, TopRight = Top | Right };
	void SetPosition(Vector2 offset, float size, RelativePosition relative, float aspectRatio);

	void SetPosition(Vector2 size);

	XM_ALIGNED_STRUCT(16) QuadData
	{
		Vector4 vertices[4] = { { 1.0f, 0.0f, 0.0f, 1.0f }, // Bottom-right
								{ 0.0f, 0.0f, 0.0f, 1.0f }, // Bottom-left
								{ 1.0f, 1.0f, 0.0f, 1.0f },  // Top-right
								{ 0.0f, 1.0f, 0.0f, 1.0f } };   // Top-left;

		UINT textureIndex{};
	}
	data;
};