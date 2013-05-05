cbuffer PerObject : register(b1) {
    float4x4 wvpMat : packoffset(c0);
	float4x4 wMat : packoffset(c4);
};

cbuffer PerMaterial : register(b2) {
    float3 sceneCorner : packoffset(c0);
	float voxelSize : packoffset(c0.w);
};


struct VS_Input
{
	float4 p    : POSITION;
	float2 uv   : TEXCOORD0;
	float3 n    : NORMAL;
};

struct VS_Out
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 wp : TEXCOORD1;
	float3 n : TEXCOORD2;
};

struct PS_Input
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 wp : TEXCOORD1;
	float3 n : TEXCOORD2;
};

VS_Out VS_Main( VS_Input vin )
{
VS_Out vsOut = ( VS_Out )0;

vsOut.pos = mul( vin.p, wvpMat);
vsOut.uv = vin.uv;
vsOut.wp = mul(vin.p,wMat).xyz;
vsOut.n = mul(vin.n,wMat).xyz;

return vsOut;
}

void PS_Main( PS_Input pin,
			Texture2D colorMap : register(t0),
			Texture3D shadowVoxel : register(t1),
			SamplerState colorSampler : register(s0),
			RWTexture3D<float4> voxelMap : register(u0),
			RWTexture3D<float4> voxelNMap : register(u1)
		     )
{

float4 color=colorMap.Sample( colorSampler, pin.uv );

float3 posUV = (pin.wp.xyz-sceneCorner)*voxelSize;
float leng = 15;
float3 off = float3(0,10,-10);
float dist = saturate(leng + length(pin.wp.xyz-off)*-1)/leng;

float l = length(color.rgb);
float3 outCol = pow(color.rgb,1);

float ysep = (normalize(pin.n).x+1)/2;
float3 voxCol = shadowVoxel.Load(float4(posUV,0)).r*outCol;

voxelMap[posUV] = float4(voxCol*ysep,1);
voxelNMap[posUV] = float4(voxCol*(1-ysep),1);

}