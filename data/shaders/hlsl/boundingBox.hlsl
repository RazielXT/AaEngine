float4x4 WorldViewProjectionMatrix;
float4x4 WorldMatrix;
float3 WorldPosition;
float3 CameraPosition;

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
	float distScale = length(WorldPosition - CameraPosition) / 5;

	float limit = 0.1 * min(0.8,  thickness / distScale);
	limit += 0.4;

	return limit;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	input.uv.x = abs(input.uv.x - 0.5);
	input.uv.y = abs(input.uv.y - 0.5);

	float limit = getLimit(input.thickness);

	if (input.uv.x < limit && input.uv.y < limit)
		discard;

	float shade = 1 - input.uv.x * input.uv.y * 2;
	float4 outColor = float4(1,1,1,1) * shade;

	return outColor;
}
