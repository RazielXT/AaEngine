#include "hlsl/grid/heightmapGridReconstruction.hlsl"

float4x4 ViewProjectionMatrix;
float3 WorldPosition;
uint TexIdHeightmap;
uint TexIdNormalmap;
uint TexIdDiffuse;

// =========================================================================
// Resources & Structures
// =========================================================================

struct GridInstanceInfo
{
	float3 WorldPosition;
	float2 WorldSize;    // e.g., 100x100 units
};

// SRV holding info for our 2 grids
StructuredBuffer<GridInstanceInfo> GeometryBuffer : register(t0);

SamplerState LinearSampler : register(s0);

// Data passed from Task Shader -> Mesh Shader
struct MeshletPayload
{
	uint GridIndex; // Is this grid 0 or grid 1?
};

// Vertex structure going to the Rasterizer/Pixel Shader
struct VertexOut
{
	float4 Position : SV_Position;
	float2 UV : TEXCOORD0;
	float3 WorldPos : TEXCOORD1;
};

// Constants defining our topology structure
#define MESHLET_gridSize 4     // 4x4 meshlets per grid
#define VTX_gridSize 8         // 8x8 vertices per meshlet
#define NUM_VERTS (VTX_gridSize * VTX_gridSize) // 64 vertices
// Triangles in an 8x8 grid = 7 * 7 quads * 2 triangles/quad = 98
#define NUM_TRIS ((VTX_gridSize - 1) * (VTX_gridSize - 1) * 2)

// =========================================================================
// 1. The Task Shader (The Manager)
// =========================================================================
// We launch 2 Task groups from host CPU: DispatchMesh(2, 1, 1)
// Each group runs this shader once.
[numthreads(1, 1, 1)]
void ASMain(
	uint gid : SV_GroupID // This will be 0 for the first grid, 1 for the second
)
{
	// 1. Pass necessary info to children Mesh Shaders
	MeshletPayload s_Payload;
	s_Payload.GridIndex = gid;

	// 2. Launch the Mesh Shaders for this specific grid.
	// We need a 4x4 grid of meshlets.
	DispatchMesh(MESHLET_gridSize, MESHLET_gridSize, 1, s_Payload);
}

// =========================================================================
// 2. The Mesh Shader (The Generator)
// =========================================================================
// This runs once per meshlet defined in the Task shader's DispatchMesh call.
// We use 8x8 threads to map 1:1 with the vertices we want to generate.
[numthreads(VTX_gridSize, VTX_gridSize, 1)]
[outputtopology("triangle")]
void MSMain(
	uint2 gtid : SV_GroupThreadID, // Local vertex coordinate (0..7, 0..7)
	uint2 gid : SV_GroupID,        // Meshlet coordinate (0..3, 0..3)
	in payload MeshletPayload inputPayload, // Data from parent Task Shader
	out vertices VertexOut outVerts[NUM_VERTS],
	out indices uint3 outIndices[NUM_TRIS]
)
{
	// --- SETUP ---
	// Define the total number of vertices and primitives this meshlet outputs.
	// This must be called before writing to output arrays.
	SetMeshOutputCounts(NUM_VERTS, NUM_TRIS);

	// Grab grid info based on payload
	GridInstanceInfo gridInfo = GeometryBuffer[inputPayload.GridIndex];

	// Helper to flatten 2D thread ID into 1D index for array writes
	uint flatVertexIndex = gtid.y * VTX_gridSize + gtid.x;

	// --- VERTEX GENERATION (Running on all 64 threads) ---

	// Calculate global "integer" coordinates across the whole 4x4 meshlet grid.
	// IMPORTANT: Neighboring meshlets must share edge vertices to avoid cracks.
	// Meshlet 0 covers vertices 0-7. Meshlet 1 covers vertices 7-14.
	// We multiply by (VTX_gridSize - 1) which is 7.
	uint2 globalStep = gid * (VTX_gridSize - 1) + gtid;

	// Total steps across one side of the whole grid = 4 meshlets * 7 steps = 28 steps.
	float totalSteps = MESHLET_gridSize * (VTX_gridSize - 1); // 28.0f

	// Calculate UV coordinate (0.0 to 1.0 relative to the whole grid)
	float2 localUV = (float2)globalStep / totalSteps;

	// Sample heightmap
	Texture2D<float> heightmap = ResourceDescriptorHeap[TexIdHeightmap];
	float heightValue = heightmap.SampleLevel(LinearSampler, localUV, 0);

	// Calculate final local 3D position (assuming Y is up)
	// Center the grid around (0,0) in XZ plane before scaling
	float3 localPos;
	localPos.x = (localUV.x - 0.5f) * gridInfo.WorldSize.x;
	localPos.y = heightValue * 150.0f; // Arbitrary height scale
	localPos.z = (localUV.y - 0.5f) * gridInfo.WorldSize.y;

	// Apply world position offset
	float3 worldPos = localPos + gridInfo.WorldPosition + WorldPosition;

	outVerts[flatVertexIndex].Position = mul(float4(worldPos, 1.0), ViewProjectionMatrix);
	outVerts[flatVertexIndex].UV = localUV;
	outVerts[flatVertexIndex].WorldPos = worldPos;

	// --- INDEX GENERATION (Running on subset of threads) ---
    // Only threads 0..6 (on both axes) generate triangles for the 7x7 quads
    if (gtid.x < (VTX_gridSize - 1) && gtid.y < (VTX_gridSize - 1))
    {
        // Calculate the absolute quad position in the entire terrain
        uint quadX = gid.x * (VTX_gridSize - 1) + gtid.x;
        uint quadY = gid.y * (VTX_gridSize - 1) + gtid.y;

        uint flatQuadIndex = gtid.y * (VTX_gridSize - 1) + gtid.x;
        uint triOutputIdx = flatQuadIndex * 2;

        // Local vertex indices for this quad
        uint vTL = flatVertexIndex;                  // Top Left
        uint vTR = flatVertexIndex + 1;              // Top Right
        uint vBL = flatVertexIndex + VTX_gridSize;   // Bottom Left
        uint vBR = flatVertexIndex + VTX_gridSize + 1; // Bottom Right

        bool flip = (quadX + quadY) % 2 == 0;

        if (flip)
        {
            // Pattern: |/ (Diagonal from Bottom-Left to Top-Right)
            // Triangle 1: Top-Left, Bottom-Left, Top-Right
            outIndices[triOutputIdx]     = uint3(vTL, vBL, vTR);
            // Triangle 2: Bottom-Left, Bottom-Right, Top-Right
            outIndices[triOutputIdx + 1] = uint3(vBL, vBR, vTR);
        }
        else
        {
            // Pattern: |\ (Diagonal from Top-Left to Bottom-Right)
            // Triangle 1: Top-Left, Bottom-Left, Bottom-Right
            outIndices[triOutputIdx]     = uint3(vTL, vBL, vBR);
            // Triangle 2: Top-Left, Bottom-Right, Top-Right
            outIndices[triOutputIdx + 1] = uint3(vTL, vBR, vTR);
        }
    }
}

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct PSOutput
{
	float4 albedo : SV_Target0;
	float4 normals : SV_Target1;
	float4 motionVectors : SV_Target2;
};

PSOutput PSMain(VertexOut input)
{
	SamplerState diffuse_sampler = SamplerDescriptorHeap[0];

	float4 albedo = GetTexture(TexIdDiffuse).Sample(diffuse_sampler, input.UV);
	//albedo.rgb *= MaterialColor;

	GridTBN tbn = ReadGridTBN(ResourceDescriptorHeap[TexIdNormalmap], LinearSampler, input.UV);

	PSOutput output;
	output.albedo = float4(albedo.rgb, 0);
	output.normals = float4(tbn.N, 1);
	output.motionVectors = 0;

	return output;
}
