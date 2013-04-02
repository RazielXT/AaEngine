#ifndef _SHADER_CONSTS_
#define _SHADER_CONSTS_

#include <xnamath.h>

//#pragma pack(16)

struct PerFrameConstants
{
	XMFLOAT4X4 viewProjectionMatrix;			//0-3

	XMFLOAT4X4 directional_light_VPMatrix;			//4-7

	__declspec(align(16)) XMFLOAT3 cameraPos;						//8

	__declspec(align(16)) XMFLOAT3 ambientColour;					//9

	__declspec(align(16)) XMFLOAT3 directional_light_direction;	//10

	__declspec(align(16)) XMFLOAT3 directional_light_color;		//11

	
	float time;								//12.x
	float time_delta;						//12.y
	float padding4[2];						
};


struct PerObjectConstants
{
	XMFLOAT4X4 worldViewProjectionMatrix;	//0-3
	XMFLOAT4X4 worldMatrix;					//4-7
	XMFLOAT3 worldPosition;					//8-8.z
	float padding;
};

//#pragma pack()

//#pragma pack(pop,identifier)


#endif