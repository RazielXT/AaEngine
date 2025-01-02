float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float3 WorldPosition;
float3 CameraPosition;

struct VSInput
{
    float4 position : POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 normal : NORMAL;
	float2 uv : TEXCOORD0;
	float3 scale : TEXCOORD1;
};

float3 GetScaleFromWorldMatrix(float4x4 worldMatrix)
{
    float scaleX = length(worldMatrix[0].xyz);
    float scaleY = length(worldMatrix[1].xyz);
    float scaleZ = length(worldMatrix[2].xyz);

    return float3(scaleX, scaleY, scaleZ);
}

PSInput VSMain(VSInput input)
{
    PSInput result;

	float4 wp = mul(input.position, WorldMatrix);
    result.position = mul(wp, ViewProjectionMatrix);
    result.normal = input.normal;
	result.uv = input.uv;

	result.scale = GetScaleFromWorldMatrix(WorldMatrix);

    return result;
}

struct PSOutput
{
    float4 target0 : SV_Target0;
    float4 target1 : SV_Target1;
};

float2 getLimit(float2 scale)
{
	float distScale = length(WorldPosition - CameraPosition) / 10;

	float2 limit = 1 - distScale * 0.1 / scale;

	return limit;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	input.uv.x = abs(input.uv.x - 0.5) * 2;
	input.uv.y = abs(input.uv.y - 0.5) * 2;

	float2 scale = float2(1,1);

	if (abs(input.normal.x) > 0.5)
	{
		scale = input.scale.zy;
	}
	else if (abs(input.normal.y) > 0.5)
	{
		scale = input.scale.xz;
	}
	else //if (abs(input.normal.z) > 0.5)
	{
		scale = input.scale.xy;
	}

	float2 limit = getLimit(scale);

	if (input.uv.x < limit.x && input.uv.y < limit.y)
		discard;

	float shade = 1.5 - input.uv.x * input.uv.y * 0.9;
	
	if (shade > 1)
		discard;

	float4 color = float4(1,1,1,1) * (1.7 - shade);


	//color.rgb = input.normal.xyz;
    return color;
}
