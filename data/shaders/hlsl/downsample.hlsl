float2 ViewportSizeInverse;
float DeltaTime;

Texture2D colorMap : register(t0);
Texture2D brightnessMap : register(t1);
SamplerState colorSampler : register(s0);
SamplerState DepthSampler : register(s0);

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 PSLoadThrough(VS_OUTPUT input) : SV_TARGET
{
    return colorMap.SampleLevel(DepthSampler, input.TexCoord, 0);
}

float4 PSDownsample3x3(VS_OUTPUT input) : SV_TARGET
{
    float4 accum = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float2 texOffset[9] = {
		-1.0, -1.0,
		 0.0, -1.0,
		 1.0, -1.0,
		-1.0,  0.0,
		 0.0,  0.0,
		 1.0,  0.0,
		-1.0,  1.0,
		 0.0,  1.0,
		 1.0,  1.0
	};

	for( int i = 0; i < 9; i++ )
    {
        accum += colorMap.Sample(colorSampler, input.TexCoord + ViewportSizeInverse * texOffset[i]);
    }
    
	// take average of 9 samples
	accum *= 0.1111111111111111;
	return accum;
}

float4 PSDownsample3x3Exposure(VS_OUTPUT input) : SV_TARGET
{
    float4 accum = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float2 texOffset[9] = {
		-1.0, -1.0,
		 0.0, -1.0,
		 1.0, -1.0,
		-1.0,  0.0,
		 0.0,  0.0,
		 1.0,  0.0,
		-1.0,  1.0,
		 0.0,  1.0,
		 1.0,  1.0
	};

	for( int i = 0; i < 9; i++ )
    {
        accum += colorMap.Sample(colorSampler, input.TexCoord + ViewportSizeInverse * texOffset[i]);
    }
    
	// take average of 9 samples
	accum *= 0.1111111111111111;
	
	float brightness = brightnessMap.Load(int3(0,0,0)).r;
	
	accum.r = lerp(brightness, accum.r, DeltaTime);
	/*
	float bt=accum.r;
	float bc=brightness;
	float next=bt-bc;
	float h=0.3*DeltaTime;
	float h2=-0.1*DeltaTime;
	if(next>h) next=h;
	if(next<h2) next=h2;
	accum=clamp(0,0.89,bc+next);
	*/
	return accum;
}

static const float bloomSampleWeight[11] =
{
	0.01122447,
	0.03383468,
	0.05359061,
	0.10097757,
	0.16266632,

	0.37241265,

	0.16266632,
	0.10097757,
	0.05359061,
	0.03383468,
	0.01122447
};

static const float bloomOffset[11] =
{
	-1.5,
	-1.1,
	-0.7,
	-0.4,
	-0.25,
	 0,
	 0.25,
	 0.4,
	 0.7,
	 1.1,
	 1.5
};

#define bloomRadius 15


float4 PSBloomBlurX(VS_OUTPUT input) : SV_TARGET
{
	float brightness = 1 - brightnessMap.Load(int3(0,0,0)).r;
	float bloomSize = brightness * bloomRadius + 0.1;
	float2 offsetSize = ViewportSizeInverse * bloomSize;

    float4 output = 0;
	for (int i = 0; i < 11; i++)
	{
		output += colorMap.Sample(colorSampler, input.TexCoord + offsetSize * float2(bloomOffset[i],0)) * bloomSampleWeight[i];
	}

	return output;
}

float4 PSBloomBlurY(VS_OUTPUT input) : SV_TARGET
{
	float brightness = 1 - brightnessMap.Load(int3(0,0,0)).r;
	float bloomSize = brightness * bloomRadius + 0.1;
	float2 offsetSize = ViewportSizeInverse * bloomSize;

    float4 output = 0;
	for (int i = 0; i < 11; i++)
	{
		output += colorMap.Sample(colorSampler, input.TexCoord + offsetSize * float2(0,bloomOffset[i])) * bloomSampleWeight[i];
	}

	return output;
}
