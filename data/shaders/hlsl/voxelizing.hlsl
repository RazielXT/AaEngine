float4x4 WorldViewProjectionMatrix;
float4x4 WorldMatrix;

cbuffer SceneVoxelInfo : register(b0)
{
	float3 sceneCorner : packoffset(c0);
	float voxelSize : packoffset(c0.w);
    float2 middleCone : packoffset(c1);
    float2 sideCone : packoffset(c1.z);
    float radius : packoffset(c2);
};

struct VS_Input
{
    float4 p : POSITION;
    float2 uv : TEXCOORD0;
    float3 n : NORMAL;
    float3 tangent : TANGENT;
};

struct VS_Out
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 wp : TEXCOORD1;
    float3 n : TEXCOORD2;
    float3 tangent : TANGENT;
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 wp : TEXCOORD1;
    float3 n : TEXCOORD2;
    float3 tangent : TANGENT;
};

VS_Out VS_Main(VS_Input vin)
{
    VS_Out vsOut;

    vsOut.pos = mul(vin.p, WorldViewProjectionMatrix);
    vsOut.uv = vin.uv;
    vsOut.wp = mul(vin.p, WorldMatrix).xyz;

    return vsOut;
}

RWTexture3D<float4> SceneVoxel : register(u0);

float4 PS_Main(PS_Input pin) : SV_TARGET
{
    float3 posUV = (pin.wp.xyz - sceneCorner) * voxelSize;

    SceneVoxel[posUV] = float4(1, 0, 0, 1);
	
	return float4(1, 0, 0, 1);
}
