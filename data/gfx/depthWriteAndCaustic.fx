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
float2 PSMain (VS_OUTPUT Input,
				Texture3D colorMap : register(t0),
			    SamplerState colorSampler : register(s0),
				RWTexture3D<float> voxelMap : register(u1)) : SV_TARGET 
{
	float3 posUV = (Input.wp-sceneCorner)*voxelSize;
	float3 sampleUV = (Input.wp+30)/60;
	float3 origUV = posUV;
	
	float3 wn = mul( normalize(Input.n), (float3x3)wMat);
	
	float light = max(0,dot(-sunDir,wn).x);
	
	
	float3 dir = normalize(-wn+sunDir);
	float3 vstep = dir/128;
	int steps = 100;
	float o = 0;
	
	while(steps>0)
	{
		posUV+=vstep*128;
		sampleUV+=vstep;
		
		o = colorMap.Load( float4(posUV,0) ).a;
		
		if(o>0)
			steps = 0;
			
		steps--;
	}
	//posUV+=dir*8;
	
	//InterlockedAdd(voxelMap[posUV].r,255);
	float v = voxelMap[posUV];//voxelMap.Load(uint4(posUV*128,0));
	
	//o = colorMap.Sample( colorSampler, posUV ).r;
	
	//if(o>0)
	voxelMap[posUV]=v+0.05;//colorMap.Load(float4(posUV,0)).a;//colorMap.Load( int4(sampleUV*128,0) );//v+0.5;
	/*
	posUV = origUV;
	
	
	dir = normalize(wn);
	vstep = dir/128;
	steps = 100;
	o = 0;
	
	while(steps>0)
	{
		posUV+=vstep*128;
		sampleUV+=vstep;
		
		o = colorMap.Load( float4(posUV,0) ).a;
		
		if(o>0)
			steps = 0;
			
		steps--;
	}
	//posUV+=dir*8;
	
	//InterlockedAdd(voxelMap[posUV].r,255);
	v = voxelMap[posUV].r;//voxelMap.Load(uint4(posUV*128,0));
	
	//o = colorMap.Sample( colorSampler, posUV ).r;
	
	//if(o>0)
	voxelMap[posUV]=v+0.05;//colorMap.Load(float4(posUV,0)).a;//colorMap.Load( int4(sampleUV*128,0) );//v+0.5;
	*/
	
    float2 rt = float2(Input.vPosition.z,Input.vPosition.z*Input.vPosition.z);
    return rt;
}