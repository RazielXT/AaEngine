float3 SrgbToLinear(float3 srgbColor)
{
	return pow(srgbColor, 2.233333333);
}