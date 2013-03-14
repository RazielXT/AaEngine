#ifndef _SHADER_CONSTS_
#define _SHADER_CONSTS_

#include <xnamath.h>

struct PerFrameConstants
{
	XMFLOAT4X4 viewProjectionMatrix;
	XMFLOAT3 cameraPos;
	XMFLOAT3 ambientColour;
	XMFLOAT3 directional_light_direction;
	XMFLOAT3 directional_light_color;
	float time;
	float time_delta;
	float padding[2];
};


struct PerObjectConstants
{
	XMFLOAT4X4 worldViewProjectionMatrix;
	XMFLOAT3 worldPosition;
	float padding;
};



#endif