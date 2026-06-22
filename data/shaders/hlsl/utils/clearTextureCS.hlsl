RWTexture3D<float3> buffer : register(u0);

[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID)
{
	buffer[id] = float3(0,0,0);
}
