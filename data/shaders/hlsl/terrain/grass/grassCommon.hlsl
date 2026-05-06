struct GrassInfo
{
	float3 position;
	float rotation;
	float scale;
};

struct RenderGrassInfo
{
	float3 position;
	float scale;
	float cosYaw;
	float sinYaw;

	float3x3 RotationMatrix()
	{
		return float3x3(
			cosYaw, 0, sinYaw,
			0, 1, 0,
			-sinYaw, 0, cosYaw
		);
	}
};
