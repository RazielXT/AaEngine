//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
cbuffer PerFrame : register(b0) {
    float3 sunDir : packoffset(c10);
};

cbuffer PerObject : register(b1) {
    float4x4 wvpMat : packoffset(c0);
    float4x4 wMat : packoffset(c4);
};

cbuffer PerMaterial : register(b2) {
    float3 sceneCorner : packoffset(c0);
	float voxelSize : packoffset(c0.w);
};

//--------------------------------------------------------------------------------------
// Input / Output structures
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
	float4 vPosition	: POSITION;
	float4 vNormal	: NORMAL;
};

struct VS_OUTPUT
{
	float4 vPosition	: SV_POSITION;
	float3 wp : TEXCOORD0;
	float3 n : TEXCOORD1;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VSMain( VS_INPUT Input )
{
	VS_OUTPUT Output;
		
	Output.vPosition = mul( Input.vPosition, wvpMat);
	Output.n = Input.vNormal.xyz;
	//Output.vPosition.z -=0.001;
	Output.wp = mul( Input.vPosition, wMat).xyz;
	
	return Output;
}

[earlydepthstencil]
float2 PSMain (VS_OUTPUT Input, RWTexture3D<float> voxelMap : register(u1)) : SV_TARGET 
{
	float3 posUV = (Input.wp-sceneCorner)*voxelSize;
	float light = max(0,dot(-sunDir,mul( normalize(Input.n), (float3x3)wMat)).x);
	voxelMap[posUV]=1;//light*0.5+0.5;
	
    float2 rt = float2(Input.vPosition.z,Input.vPosition.z*Input.vPosition.z);
    return rt;
}