Texture2D colorMap : register(t0);
Texture2D transparentMap : register(t1);

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

static const float samples[11] =
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

float4 getFilteredWater(int2 uv, int2 offset)
{
	float fill = transparentMap.Load(int3(uv,0)).a;
	float4 color = colorMap.Load(int3(uv,0));

	if (fill == 0)
	{
		color = 0;
		for (int x = -5; x <= 5; x++)
		{
			color += colorMap.Load(int3(uv + offset * x,0)) * samples[x + 5];
		}
	}

    return float4(color.rgb, 1);
}

float4 PSWaterPostX(VS_OUTPUT input) : SV_TARGET
{
	return getFilteredWater(input.Position.xy, int2(0,1));
}

float4 PSWaterPostY(VS_OUTPUT input) : SV_TARGET
{
	return getFilteredWater(input.Position.xy, int2(1,0));
}

float4 PSWaterApply(VS_OUTPUT input) : SV_TARGET
{
	float4 color = colorMap.Load(int3(input.Position.xy,0));
	float4 transparent = transparentMap.Load(int3(input.Position.xy,0));

    return lerp(color, transparent, transparent.a);
}
