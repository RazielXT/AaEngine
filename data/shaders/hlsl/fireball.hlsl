float4x4 WorldMatrix;
float4x4 ViewProjectionMatrix;
float Time;
float3 CameraPosition;
float3 WorldPosition;
uint TexIdFire;
float3 Direction;

static const float UVScale = 5;
static const float Speed = 1;
static const float FireDotMin = 0.375;
static const float FireDotPass = 0.5;

float3 TriplanarMap(
    Texture2D mapTex,
    SamplerState samp,
    float3 worldPos,
    float3 worldNormal,
    float  sharpness,
    float  scale,
    float3 offset
)
{
    float2 yUV = offset.xz + worldPos.xz / scale;
    float2 xUV = offset.zy + worldPos.zy / scale;
    float2 zUV = offset.xy + worldPos.xy / scale;

    float3 yDiff = mapTex.Sample(samp, yUV).rgb;
    float3 xDiff = mapTex.Sample(samp, xUV).rgb;
    float3 zDiff = mapTex.Sample(samp, zUV).rgb;

    float3 blendWeights = pow(abs(worldNormal), sharpness);
    blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z + 1e-5f);

    return xDiff * blendWeights.x +
           yDiff * blendWeights.y +
           zDiff * blendWeights.z;
}

float3 TriplanarMapB(
    Texture2D mapTex,
    SamplerState samp,
    float3 worldPos,
    float3 worldNormal,
    float3 offset,
    float  scale
)
{
    return TriplanarMap(mapTex, samp, worldPos, worldNormal, 3.0f, scale, offset);
}

//======================================================================
// Vertex shader
//======================================================================

struct VIn
{
    float4 p  : POSITION;
    float3 n  : NORMAL;
    float3 t  : TANGENT;
    float2 uv : TEXCOORD0;
};

struct VOut
{
    float4 p    : SV_POSITION;
    float4 uv   : TEXCOORD0;
    float3 wp   : TEXCOORD1;
    float3 n    : TEXCOORD2;
    float3 t    : TEXCOORD3;
    float3 b    : TEXCOORD4;
    float3 opos : TEXCOORD5;
};

VOut VSMain(VIn IN)
{
    VOut OUT;

    float4 worldPos = mul(IN.p, WorldMatrix);
    OUT.wp   = worldPos.xyz;
    OUT.p    = mul(worldPos, ViewProjectionMatrix);
    OUT.opos = IN.p.xyz;

    OUT.uv.xy = IN.uv.xy;
    OUT.uv.zw = OUT.p.zw;

    OUT.n = normalize(mul(IN.n, (float3x3)WorldMatrix));
    OUT.t = IN.t;
    OUT.b = normalize(cross(IN.t, IN.n));

    return OUT;
}

//======================================================================
// Pixel shader
//======================================================================

struct PSOut
{
	float4 albedo : SV_Target0;
	float4 normals : SV_Target1;
	float4 motionVectors : SV_Target2;
};

PSOut PSMain(VOut IN)
{
    PSOut OUT;

    float time = Time + (WorldPosition.x + WorldPosition.y + WorldPosition.z) / 10.f;
    time *= 0.13f * Speed;

    float3 fireDir = normalize(Direction);

	Texture2D gDiffuseMap = ResourceDescriptorHeap[TexIdFire];
	SamplerState gSampler = SamplerDescriptorHeap[0];

    float3 uvTex  = TriplanarMapB(gDiffuseMap, gSampler, IN.opos.xyz, IN.n.xyz, time,        UVScale);
    float3 uvTex2 = TriplanarMapB(gDiffuseMap, gSampler, IN.opos.xyz, IN.n.xyz, time * 0.5f, UVScale);
    float  fireTexturesDiff = 1.0f;

    float3 diffuseTex = TriplanarMapB(
        gDiffuseMap, gSampler,
        IN.opos.xyz + time,
        IN.n.xyz,
        time * fireDir + 0.05f * float3(uvTex.x, uvTex2.x, uvTex.y),
        UVScale
    );

    diffuseTex += TriplanarMapB(
        gDiffuseMap, gSampler,
        IN.opos.xyz + time,
        IN.n.xyz,
        time * fireDir * fireTexturesDiff + 0.05f * float3(uvTex.x, uvTex2.x, uvTex.y),
        UVScale
    );

    diffuseTex *= 0.5f;

    float fireAlpha = diffuseTex.r;

    float3 camDir = normalize(IN.wp.xyz - CameraPosition);
	float3 normal = IN.n;

    float rawProd = dot(camDir, normal);
    float dotProd = abs(rawProd);

    float fireFullDotMin = saturate(FireDotMin + FireDotPass);
    float adjMinFull     = max(fireFullDotMin - FireDotMin, 1e-5f);
    float adjDotProd     = saturate((dotProd - FireDotMin) / adjMinFull);

    if (adjDotProd < fireAlpha)
        discard;

    OUT.albedo.rgb = pow(diffuseTex.rgb * 2, 4.0f) * 2;
	//OUT.albedo.rgb = OUT.albedo.rgb * 0.001 + normal;
    OUT.albedo.a   = diffuseTex.r;

    OUT.normals = float4(normal, 1);
    OUT.motionVectors = float4(0, 0, 0, 1);

    return OUT;
}
