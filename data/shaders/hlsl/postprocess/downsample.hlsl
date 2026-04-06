#include "PostProcessCommon.hlsl"

float2 ViewportSizeInverse;
float DeltaTime;

Texture2D colorMap : register(t0);
Texture2D brightnessMap : register(t1);
SamplerState LinearSampler : register(s0);
SamplerState DepthSampler : register(s0);

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
        accum += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * texOffset[i]);
    }
    
	// take average of 9 samples
	accum *= 0.1111111111111111;
	return accum;
}

float calculateNextBrightness(float lastBrightness, float currentBrightness, float deltaTime)
{
    float maxChange = 0.4f * deltaTime;  // Maximum change allowed per frame
    float change = pow(currentBrightness, 1.0/2.233) - lastBrightness;

    if (abs(change) > maxChange) {
        change = sign(change) * maxChange;  // Limit the change to maxChange
    }

    return lastBrightness + change;  // Return the next brightness value
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
        accum += colorMap.Sample(LinearSampler, input.TexCoord + ViewportSizeInverse * texOffset[i]);
    }
    
	// take average of 9 samples
	accum *= 0.1111111111111111;
	
	float brightness = brightnessMap.Load(int3(0,0,0)).r;

	accum.r = calculateNextBrightness(brightness, accum.r, DeltaTime);
	
	/*float bt=accum.r;
	float bc=brightness;
	float next=bt-bc;
	float h=0.3*DeltaTime;
	float h2=-0.1*DeltaTime;
	if(next>h) next=h;
	if(next<h2) next=h2;
	accum=saturate(bc+next);*/
	
	return accum;
}
