#pragma once

float CatmullRom(float x)
{
	float ax = abs(x);

	if (ax >= 2.0)
		return 0.0;

	if (ax >= 1.0)
		return ((-0.5 * ax + 2.5) * ax - 4.0) * ax + 2.0;

	return ((1.5 * ax - 2.5) * ax) * ax + 1.0;
}

float SampleBicubic(Texture2D<float> tex, SamplerState samp, float2 uv, float2 texSize)
{
	// uv in [0,1], texSize = float2(width, height)
	float2 pixel = uv * texSize - 0.5;
	float2 base  = floor(pixel);
	float2 f     = pixel - base;

	float wx[4];
	float wy[4];

	[unroll]
	for (int i = 0; i < 4; ++i)
	{
		float x = (float(i) - 1.0) - f.x;   // i = 0..3 → x = -1-f, 0-f, 1-f, 2-f
		float y = (float(i) - 1.0) - f.y;

		wx[i] = CatmullRom(x);
		wy[i] = CatmullRom(y);
	}

	float h = 0.0;

	[unroll]
	for (int j = 0; j < 4; ++j)
	{
		[unroll]
		for (int i = 0; i < 4; ++i)
		{
			float2 coord = (base + float2(i - 1, j - 1) + 0.5) / texSize;
			float s = tex.SampleLevel(samp, coord, 0).r;
			h += s * wx[i] * wy[j];
		}
	}

	return h;
}

float CubicHermite(float A, float B, float C, float D, float t)
{
	float a = -A * 0.5 + B * 1.5 - C * 1.5 + D * 0.5;
	float b =  A       - B * 2.5 + C * 2.0 - D * 0.5;
	float c = -A * 0.5 + C * 0.5;
	float d =  B;

	return ((a * t + b) * t + c) * t + d;
}

float SampleFastBicubic(Texture2D<float> tex, SamplerState samp, float2 uv, float2 texSize)
{
	float2 pixel = uv * texSize - 0.5;

	float2 base = floor(pixel);
	float2 f = frac(pixel);

	float2 invSize = 1.0 / texSize;

	// 3x3 neighborhood
	float h00 = tex.SampleLevel(samp, (base + float2(-1,-1) + 0.5) * invSize, 0);
	float h10 = tex.SampleLevel(samp, (base + float2( 0,-1) + 0.5) * invSize, 0);
	float h20 = tex.SampleLevel(samp, (base + float2( 1,-1) + 0.5) * invSize, 0);

	float h01 = tex.SampleLevel(samp, (base + float2(-1, 0) + 0.5) * invSize, 0);
	float h11 = tex.SampleLevel(samp, (base + float2( 0, 0) + 0.5) * invSize, 0);
	float h21 = tex.SampleLevel(samp, (base + float2( 1, 0) + 0.5) * invSize, 0);

	float h02 = tex.SampleLevel(samp, (base + float2(-1, 1) + 0.5) * invSize, 0);
	float h12 = tex.SampleLevel(samp, (base + float2( 0, 1) + 0.5) * invSize, 0);
	float h22 = tex.SampleLevel(samp, (base + float2( 1, 1) + 0.5) * invSize, 0);

	// Horizontal cubic interpolation
	float row0 = CubicHermite(h00, h10, h20, h20, f.x);
	float row1 = CubicHermite(h01, h11, h21, h21, f.x);
	float row2 = CubicHermite(h02, h12, h22, h22, f.x);

	// Vertical cubic interpolation
	return CubicHermite(row0, row1, row2, row2, f.y);
}
