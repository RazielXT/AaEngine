/*
// Deterministic scalar random from 2D input using stable irrational constants.
float RandomFrom2D(float2 p)
{
	const float2 k = float2(
		23.14069263277926,
		2.665144142690225
	);
	return frac(cos(dot(p, k)) * 12345.6789);
}

float2 Random2DFrom2D(float2 p)
{
	float n = sin(dot(p, float2(41.0, 289.0)));
	return frac(float2(262144.0, 32768.0) * n);
}
*/

// Time-varying hash used for jitter/noise patterns in screen-space passes.
float RandomFrom2DTime(float2 p, float time)
{
	return frac(sin(dot(p + time * 0.000001, float2(12.9898, 78.233))) * 43758.5453);
}

float RandomFrom2D(float2 p)
{
	p = frac(p * float2(443.8975, 441.423) * 0.3183099);
	p += dot(p, p + 31.32);

	return frac(p.x * p.y + p.x);
}

float2 Random2DFrom2D(float2 p)
{
	p = frac(p * float2(443.8975, 441.423) * 0.3183099);
	p += dot(p, p + 31.32);

	return frac(float2(p.x * p.y + p.x, p.x * p.y + p.y));
}
