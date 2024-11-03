float4x4 WorldViewProjectionMatrix;
float4x4 WorldMatrix;

struct VSInput
{
    float4 position : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
	float2 uv : TEXCOORD0;
	float thickness : TEXCOORD1;
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

    result.position = mul(input.position, WorldViewProjectionMatrix);
    result.color = input.color;
	result.uv = input.uv;

	result.thickness = GetScaleFromWorldMatrix(WorldMatrix).x;

    return result;
}

struct PSOutput
{
    float4 target0 : SV_Target0;
    float4 target1 : SV_Target1;
};

float getLimit(float thickness)
{
	float limit = 0.1 * min(0.9, thickness / 30);
	limit += 0.4;

	return limit;
}

PSOutput PSMain(PSInput input)
{
	input.uv.x = input.uv.x - 0.5;
	input.uv.y = input.uv.y - 0.5;

	float limit = getLimit(input.thickness);

	if (abs(input.uv.x) < limit && abs(input.uv.y) < limit)
		discard;

	PSOutput output;
    output.target0 = input.color;
	output.target1 = input.color;

	return output;
}
