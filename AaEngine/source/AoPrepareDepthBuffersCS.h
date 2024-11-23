#pragma once

#include "ComputeShader.h"

class ShaderTextureView;

class AoPrepareDepthBuffersCS : public ComputeShader
{
public:

	struct
	{
		float ZMagic;
		UINT ResIdLinearZ;
		UINT ResIdDS2x;
		UINT ResIdDS2xAtlas;
		UINT ResIdDS4x;
		UINT ResIdDS4xAtlas;
	}
	data;

	void dispatch(UINT width, UINT height, const ShaderTextureView& input, ID3D12GraphicsCommandList* commandList);
};

class AoPrepareDepthBuffers2CS : public ComputeShader
{
public:

	struct
	{
		UINT ResIdDS8x;
		UINT ResIdDS8xAtlas;
		UINT ResIdDS16x;
		UINT ResIdDS16xAtlas;
	}
	data;

	void dispatch(UINT width, UINT height, const ShaderTextureView& input, ID3D12GraphicsCommandList* commandList);
};
