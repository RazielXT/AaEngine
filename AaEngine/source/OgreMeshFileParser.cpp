#include "OgreMeshFileParser.h"
#include "FileLogger.h"
#include <map>

using namespace OgreMeshFileParser;

template<typename T>
void readData(std::ifstream& stream, T* data, size_t count)
{
	stream.read((char*)data, sizeof(T) * count);
}

std::string readString(std::ifstream& stream)
{
	std::string out;
	char c = 0;
	// Keep looping while not hitting delimiter
	while (stream.read(&c, 1) && c != 0)
	{
		if (c == '\n')
			break;

		if (c != '\r')
			out += c;
	}

	return out;
}

enum MeshChunkID
{
	M_HEADER = 0x1000,
	// char*          version           : Version number check
	M_MESH = 0x3000,
	// bool skeletallyAnimated   // important flag which affects h/w buffer policies
	// Optional M_GEOMETRY chunk
	M_SUBMESH = 0x4000,
	// char* materialName
	// bool useSharedVertices
	// unsigned int indexCount
	// bool indexes32Bit
	// unsigned int* faceVertexIndices (indexCount)
	// OR
	// unsigned short* faceVertexIndices (indexCount)
	// M_GEOMETRY chunk (Optional: present only if useSharedVertices = false)
	M_SUBMESH_OPERATION = 0x4010, // optional, trilist assumed if missing
	// unsigned short operationType
	M_SUBMESH_BONE_ASSIGNMENT = 0x4100,
	// Optional bone weights (repeating section)
	// unsigned int vertexIndex;
	// unsigned short boneIndex;
	// float weight;
	// Optional chunk that matches a texture name to an alias
	// a texture alias is sent to the submesh material to use this texture name
	// instead of the one in the texture unit with a matching alias name
	M_SUBMESH_TEXTURE_ALIAS = 0x4200, // Repeating section
	// char* aliasName;
	// char* textureName;

	M_GEOMETRY = 0x5000, // NB this chunk is embedded within M_MESH and M_SUBMESH
	// unsigned int vertexCount
	M_GEOMETRY_VERTEX_DECLARATION = 0x5100,
	M_GEOMETRY_VERTEX_ELEMENT = 0x5110, // Repeating section
	// unsigned short source;   // buffer bind source
	// unsigned short type;     // VertexElementType
	// unsigned short semantic; // VertexElementSemantic
	// unsigned short offset;   // start offset in buffer in bytes
	// unsigned short index;    // index of the semantic (for colours and texture coords)
	M_GEOMETRY_VERTEX_BUFFER = 0x5200, // Repeating section
	// unsigned short bindIndex;    // Index to bind this buffer to
	// unsigned short vertexSize;   // Per-vertex size, must agree with declaration at this index
	M_GEOMETRY_VERTEX_BUFFER_DATA = 0x5210,
	// raw buffer data
	M_MESH_SKELETON_LINK = 0x6000,
	// Optional link to skeleton
	// char* skeletonName           : name of .skeleton to use
	M_MESH_BONE_ASSIGNMENT = 0x7000,
	// Optional bone weights (repeating section)
	// unsigned int vertexIndex;
	// unsigned short boneIndex;
	// float weight;
	M_MESH_LOD_LEVEL = 0x8000,
	// Optional LOD information
	// string strategyName;
	// unsigned short numLevels;
	// bool manual;  (true for manual alternate meshes, false for generated)
	M_MESH_LOD_USAGE = 0x8100,
	// Repeating section, ordered in increasing depth
	// NB LOD 0 (full detail from 0 depth) is omitted
	// LOD value - this is a distance, a pixel count etc, based on strategy
	// float lodValue;
	M_MESH_LOD_MANUAL = 0x8110,
	// Required if M_MESH_LOD section manual = true
	// String manualMeshName;
	M_MESH_LOD_GENERATED = 0x8120,
	// Required if M_MESH_LOD section manual = false
	// Repeating section (1 per submesh)
	// unsigned int indexCount;
	// bool indexes32Bit
	// unsigned short* faceIndexes;  (indexCount)
	// OR
	// unsigned int* faceIndexes;  (indexCount)
	M_MESH_BOUNDS = 0x9000,
	// float minx, miny, minz
	// float maxx, maxy, maxz
	// float radius

	// Added By DrEvil
	// optional chunk that contains a table of submesh indexes and the names of
	// the sub-meshes.
	M_SUBMESH_NAME_TABLE = 0xA000,
	// Subchunks of the name table. Each chunk contains an index & string
	M_SUBMESH_NAME_TABLE_ELEMENT = 0xA100,
	// short index
	// char* name

	// Optional chunk which stores precomputed edge data                     
	M_EDGE_LISTS = 0xB000,
	// Each LOD has a separate edge list
	M_EDGE_LIST_LOD = 0xB100,
	// unsigned short lodIndex
	// bool isManual            // If manual, no edge data here, loaded from manual mesh
		// bool isClosed
		// unsigned long numTriangles
		// unsigned long numEdgeGroups
		// Triangle* triangleList
			// unsigned long indexSet
			// unsigned long vertexSet
			// unsigned long vertIndex[3]
			// unsigned long sharedVertIndex[3] 
			// float normal[4] 

	M_EDGE_GROUP = 0xB110,
	// unsigned long vertexSet
	// unsigned long triStart
	// unsigned long triCount
	// unsigned long numEdges
	// Edge* edgeList
		// unsigned long  triIndex[2]
		// unsigned long  vertIndex[2]
		// unsigned long  sharedVertIndex[2]
		// bool degenerate

// Optional poses section, referred to by pose keyframes
M_POSES = 0xC000,
M_POSE = 0xC100,
// char* name (may be blank)
// unsigned short target    // 0 for shared geometry, 
							// 1+ for submesh index + 1
// bool includesNormals [1.8+]
M_POSE_VERTEX = 0xC111,
// unsigned long vertexIndex
// float xoffset, yoffset, zoffset
// float xnormal, ynormal, znormal (optional, 1.8+)
// Optional vertex animation chunk
M_ANIMATIONS = 0xD000,
M_ANIMATION = 0xD100,
// char* name
// float length
M_ANIMATION_BASEINFO = 0xD105,
// [Optional] base keyframe information (pose animation only)
// char* baseAnimationName (blank for self)
// float baseKeyFrameTime

M_ANIMATION_TRACK = 0xD110,
// unsigned short type          // 1 == morph, 2 == pose
// unsigned short target        // 0 for shared geometry, 
								// 1+ for submesh index + 1
	M_ANIMATION_MORPH_KEYFRAME = 0xD111,
	// float time
	// bool includesNormals [1.8+]
	// float x,y,z          // repeat by number of vertices in original geometry
	M_ANIMATION_POSE_KEYFRAME = 0xD112,
	// float time
	M_ANIMATION_POSE_REF = 0xD113, // repeat for number of referenced poses
	// unsigned short poseIndex 
	// float influence

// Optional submesh extreme vertex list chink
M_TABLE_EXTREMES = 0xE000,
// unsigned short submesh_index;
// float extremes [n_extremes][3];
};

uint16_t readChunk(std::ifstream& stream)
{
	uint16_t id;
	readData(stream, &id, 1);

	uint32_t mCurrentstreamLen;
	readData(stream, &mCurrentstreamLen, 1);

	return id;
}

void backpedalChunkHeader(std::ifstream& stream)
{
	stream.seekg(-int(sizeof(uint16_t) + sizeof(uint32_t)), std::ios::cur);
}

enum OperationType
{
	/// A list of points, 1 vertex per point
	OT_POINT_LIST = 1,
	/// A list of lines, 2 vertices per line
	OT_LINE_LIST = 2,
	/// A strip of connected lines, 1 vertex per line plus 1 start vertex
	OT_LINE_STRIP = 3,
	/// A list of triangles, 3 vertices per triangle
	OT_TRIANGLE_LIST = 4,
	/// A strip of triangles, 3 vertices for the first triangle, and 1 per triangle after that
	OT_TRIANGLE_STRIP = 5,
	/// A fan of triangles, 3 vertices for the first triangle, and 1 per triangle after that
	OT_TRIANGLE_FAN = 6,
};

void readSubMeshOperation(std::ifstream& stream, VertexBufferModel& info)
{
	// unsigned short operationType
	unsigned short opType;
	readData(stream, &opType, 1);
}

void readBoundsInfo(std::ifstream& stream, MeshInfo& info)
{
	Vector3 min;
	readData(stream, &min.x, 3);
	Vector3 max;
	readData(stream, &max.x, 3);

	info.boundingBox.Center = (max + min) / 2;
	info.boundingBox.Extents = (max - min) / 2;

	readData(stream, &info.boundingSphere.Radius, 1);
}

void readSubMeshNameTable(std::ifstream& stream, MeshInfo& info)
{
	// The map for
	std::map<unsigned short, std::string> subMeshNames;
	unsigned short streamID, subMeshIndex;

	// Need something to store the index, and the objects name
	// This table is a method that imported meshes can retain their naming
	// so that the names established in the modelling software can be used
	// to get the sub-meshes by name. The exporter must support exporting
	// the optional stream M_SUBMESH_NAME_TABLE.

	// Read in all the sub-streams. Each sub-stream should contain an index and Ogre::String for the name.
	if (stream)
	{
		streamID = readChunk(stream);
		while (stream && (streamID == M_SUBMESH_NAME_TABLE_ELEMENT))
		{
			// Read in the index of the submesh.
			readData(stream, &subMeshIndex, 1);
			// Read in the String and map it to its index.
			subMeshNames[subMeshIndex] = readString(stream);

			// If we're not end of file get the next stream ID
			if (stream)
				streamID = readChunk(stream);
		}
		if (stream)
		{
			// Backpedal back to start of stream
			backpedalChunkHeader(stream);
		}
	}

	// Loop through and save out the index and names.
	for (auto& sn : subMeshNames)
	{
		// Name this submesh to the stored name.
		info.submeshes[sn.first].name = sn.second;
	}
}

enum OgreVertexElementType
{
	VET_FLOAT1 = 0,
	VET_FLOAT2 = 1,
	VET_FLOAT3 = 2,
	VET_FLOAT4 = 3,

	VET_SHORT1 = 5,  ///< not supported on D3D9
	VET_SHORT2 = 6,
	VET_SHORT3 = 7,  ///< not supported on D3D9 and D3D11
	VET_SHORT4 = 8,
	VET_UBYTE4 = 9,
	_DETAIL_SWAP_RB = 10,

	// the following are not universally supported on all hardware:
	VET_DOUBLE1 = 12,
	VET_DOUBLE2 = 13,
	VET_DOUBLE3 = 14,
	VET_DOUBLE4 = 15,
	VET_USHORT1 = 16,  ///< not supported on D3D9
	VET_USHORT2 = 17,
	VET_USHORT3 = 18,  ///< not supported on D3D9 and D3D11
	VET_USHORT4 = 19,
	VET_INT1 = 20,
	VET_INT2 = 21,
	VET_INT3 = 22,
	VET_INT4 = 23,
	VET_UINT1 = 24,
	VET_UINT2 = 25,
	VET_UINT3 = 26,
	VET_UINT4 = 27,
	VET_BYTE4 = 28,  ///< signed bytes
	VET_BYTE4_NORM = 29,   ///< signed bytes (normalized to -1..1)
	VET_UBYTE4_NORM = 30,  ///< unsigned bytes (normalized to 0..1)
	VET_SHORT2_NORM = 31,  ///< signed shorts (normalized to -1..1)
	VET_SHORT4_NORM = 32,
	VET_USHORT2_NORM = 33, ///< unsigned shorts (normalized to 0..1)
	VET_USHORT4_NORM = 34,
	VET_INT_10_10_10_2_NORM = 35, ///< signed int (normalized to 0..1)
	VET_HALF1 = 36, ///< not supported on D3D9
	VET_HALF2 = 37,
	VET_HALF3 = 38, ///< not supported on D3D9 and D3D11
	VET_HALF4 = 39,
	VET_COLOUR = VET_UBYTE4_NORM,  ///< @deprecated use VET_UBYTE4_NORM
	VET_COLOUR_ARGB = VET_UBYTE4_NORM,  ///< @deprecated use VET_UBYTE4_NORM
	VET_COLOUR_ABGR = VET_UBYTE4_NORM,  ///< @deprecated use VET_UBYTE4_NORM
};

enum OgreVertexElementSemantic
{
	/// Position, typically VET_FLOAT3
	VES_POSITION = 1,
	/// Blending weights
	VES_BLEND_WEIGHTS = 2,
	/// Blending indices
	VES_BLEND_INDICES = 3,
	/// Normal, typically VET_FLOAT3
	VES_NORMAL = 4,
	/// Colour, typically VET_UBYTE4
	VES_COLOUR = 5,
	/// Secondary colour. Generally free for custom data. Means specular with OpenGL FFP.
	VES_COLOUR2 = 6,
	/// Texture coordinates, typically VET_FLOAT2
	VES_TEXTURE_COORDINATES = 7,
	/// Binormal (Y axis if normal is Z)
	VES_BINORMAL = 8,
	/// Tangent (X axis if normal is Z)
	VES_TANGENT = 9,
	/// The  number of VertexElementSemantic elements (note - the first value VES_POSITION is 1) 
	VES_COUNT = 9,
	/// @deprecated use VES_COLOUR
	VES_DIFFUSE = VES_COLOUR,
	/// @deprecated use VES_COLOUR2
	VES_SPECULAR = VES_COLOUR2
};

void readGeometryVertexElement(std::ifstream& stream, VertexBufferModel& info)
{
	unsigned short source, offset, index, tmp;
	OgreVertexElementType vType;
	OgreVertexElementSemantic vSemantic;
	// unsigned short source;   // buffer bind source
	readData(stream, &source, 1);
	// unsigned short type;     // VertexElementType
	readData(stream, &tmp, 1);
	if (tmp == 4 || tmp == 11)
		vType = VET_UBYTE4_NORM;
	else
		vType = static_cast<OgreVertexElementType>(tmp);
	// unsigned short semantic; // VertexElementSemantic
	readData(stream, &tmp, 1);
	vSemantic = static_cast<OgreVertexElementSemantic>(tmp);
	// unsigned short offset;   // start offset in buffer in bytes
	readData(stream, &offset, 1);
	// unsigned short index;    // index of the semantic
	readData(stream, &index, 1);

	const char* semantic = nullptr;
	switch (vSemantic)
	{
	case VES_POSITION:
		semantic = VertexElementSemantic::POSITION;
		break;
	case VES_BLEND_WEIGHTS:
		semantic = VertexElementSemantic::BLEND_WEIGHTS;
		break;
	case VES_BLEND_INDICES:
		semantic = VertexElementSemantic::BLEND_INDICES;
		break;
	case VES_NORMAL:
		semantic = VertexElementSemantic::NORMAL;
		break;
	case VES_COLOUR:
		semantic = VertexElementSemantic::COLOR;
		break;
	case VES_TEXTURE_COORDINATES:
		semantic = VertexElementSemantic::TEXCOORD;
		break;
	case VES_BINORMAL:
		semantic = VertexElementSemantic::BINORMAL;
		break;
	case VES_TANGENT:
		semantic = VertexElementSemantic::TANGENT;
		break;
	default:
		break;
	}

	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	switch (vType)
	{
	case VET_FLOAT1:
		format = DXGI_FORMAT_R32_FLOAT;
		break;
	case VET_FLOAT2:
		format = DXGI_FORMAT_R32G32_FLOAT;
		break;
	case VET_FLOAT3:
		format = DXGI_FORMAT_R32G32B32_FLOAT;
		break;
	case VET_FLOAT4:
		format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case _DETAIL_SWAP_RB:
		format = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case VET_UBYTE4_NORM:
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	default:
		break;
	}

	if (semantic && format != DXGI_FORMAT_UNKNOWN)
		info.addLayoutElement(source, offset, format, semantic, index);
}

void readGeometryVertexBuffer(std::ifstream& stream, VertexBufferModel& info, ModelParseOptions o)
{
	unsigned short bindIndex, vertexSize;
	// unsigned short bindIndex;    // Index to bind this buffer to
	readData(stream, &bindIndex, 1);
	// unsigned short vertexSize;   // Per-vertex size, must agree with declaration at this index
	readData(stream, &vertexSize, 1);
	{
		// Check for vertex data header
		unsigned short headerID;
		headerID = readChunk(stream);
		if (headerID != M_GEOMETRY_VERTEX_BUFFER_DATA)
		{
			// 			OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, "Can't find vertex buffer data area",
			// 				"MeshSerializerImpl::readGeometryVertexBuffer");
		}
		// Check that vertex size agrees
		if (info.getLayoutVertexSize(bindIndex) != vertexSize)
		{
			// 			OGRE_EXCEPT(Exception::ERR_INTERNAL_ERROR, "Buffer vertex size does not agree with vertex declaration",
			// 				"MeshSerializerImpl::readGeometryVertexBuffer");
		}

		std::vector<char> data;
		data.resize(vertexSize * info.vertexCount);
		readData(stream, data.data(), data.size());

		info.CreateVertexBuffer(o.device, o.batch, data.data(), info.vertexCount, vertexSize);
	}
}

void readGeometryVertexDeclaration(std::ifstream& stream, VertexBufferModel& info)
{
	// Find optional geometry streams
	if (stream)
	{
		unsigned short streamID = readChunk(stream);
		while (stream &&
			(streamID == M_GEOMETRY_VERTEX_ELEMENT))
		{
			switch (streamID)
			{
			case M_GEOMETRY_VERTEX_ELEMENT:
				readGeometryVertexElement(stream, info);
				break;
			}
			// Get next stream
			if (stream)
			{
				streamID = readChunk(stream);
			}
		}
		if (stream)
		{
			// Backpedal back to start of non-submesh stream
			backpedalChunkHeader(stream);
		}
	}
}

void readGeometry(std::ifstream& stream, VertexBufferModel& info, ModelParseOptions o)
{
	readData(stream, &info.vertexCount, 1);

	if (stream)
	{
		unsigned short streamID = readChunk(stream);
		while (stream &&
			(streamID == M_GEOMETRY_VERTEX_DECLARATION ||
				streamID == M_GEOMETRY_VERTEX_BUFFER))
		{
			switch (streamID)
			{
			case M_GEOMETRY_VERTEX_DECLARATION:
				readGeometryVertexDeclaration(stream, info);
				break;
			case M_GEOMETRY_VERTEX_BUFFER:
				readGeometryVertexBuffer(stream, info, o);
				break;
			}
			// Get next stream
			if (stream)
			{
				streamID = readChunk(stream);
			}
		}
		if (stream)
		{
			// Backpedal back to start of non-submesh stream
			backpedalChunkHeader(stream);
		}
	}
}

void readSubMesh(std::ifstream& stream, MeshInfo& info, ModelParseOptions o)
{
	SubmeshInfo submesh{};
	submesh.model = new VertexBufferModel();

	submesh.materialName = readString(stream);

	bool useSharedVertices;
	readData(stream, &useSharedVertices, 1);

	readData(stream, &submesh.model->indexCount, 1);

	bool idx32bit;
	readData(stream, &idx32bit, 1);

	if (submesh.model->indexCount)
	{
		if (idx32bit)
		{
			std::vector<uint32_t> data;
			data.resize(submesh.model->indexCount);
			readData(stream, data.data(), submesh.model->indexCount);
			submesh.model->CreateIndexBuffer(o.device, o.batch, data.data(), submesh.model->indexCount);
		}
		else // 16-bit
		{
			std::vector<uint16_t> data;
			data.resize(submesh.model->indexCount);
			readData(stream, data.data(), submesh.model->indexCount);
			submesh.model->CreateIndexBuffer(o.device, o.batch, data.data(), data.size());
		}
	}

	if (!useSharedVertices)
	{
		auto streamID = readChunk(stream);
		if (streamID != M_GEOMETRY)
		{
			return;
		}
		readGeometry(stream, *submesh.model, o);
	}

	// Find all bone assignments, submesh operation, and texture aliases (if present)
	if (stream)
	{
		auto streamID = readChunk(stream);
		bool seenTexAlias = false;
		while (stream &&
			(streamID == M_SUBMESH_BONE_ASSIGNMENT ||
				streamID == M_SUBMESH_OPERATION ||
				streamID == M_SUBMESH_TEXTURE_ALIAS))
		{
			switch (streamID)
			{
			case M_SUBMESH_OPERATION:
				readSubMeshOperation(stream, *submesh.model);
				break;
				// 			case M_SUBMESH_BONE_ASSIGNMENT:
				// 				readSubMeshBoneAssignment(stream, info);
				// 				break;
			case M_SUBMESH_TEXTURE_ALIAS:
				seenTexAlias = true;
				auto aliasName = readString(stream);
				auto textureName = readString(stream);
				break;
			}

			if (stream)
			{
				streamID = readChunk(stream);
			}
		}

		if (stream)
		{
			// Backpedal back to start of stream
			backpedalChunkHeader(stream);
		}
	}

	info.submeshes.emplace_back(std::move(submesh));
}

void readMesh(std::ifstream& stream, MeshInfo& info, ModelParseOptions o)
{
	bool skeletallyAnimated;
	readData(stream, &skeletallyAnimated, 1);

	if (stream)
	{
		unsigned short streamID = readChunk(stream);
		while (stream &&
			(streamID == M_GEOMETRY ||
				streamID == M_SUBMESH ||
				streamID == M_MESH_SKELETON_LINK ||
				streamID == M_MESH_BONE_ASSIGNMENT ||
				streamID == M_MESH_LOD_LEVEL ||
				streamID == M_MESH_BOUNDS ||
				streamID == M_SUBMESH_NAME_TABLE ||
				streamID == M_EDGE_LISTS ||
				streamID == M_POSES ||
				streamID == M_ANIMATIONS ||
				streamID == M_TABLE_EXTREMES))
		{
			switch (streamID)
			{
			case M_GEOMETRY:
				//readGeometry(stream, info);
				break;
			case M_SUBMESH:
				readSubMesh(stream, info, o);
				break;
				// 			case M_MESH_SKELETON_LINK:
				// 				readSkeletonLink(stream, pMesh, listener);
				// 				break;
				// 			case M_MESH_BONE_ASSIGNMENT:
				// 				readMeshBoneAssignment(stream, pMesh);
				// 				break;
				// 			case M_MESH_LOD_LEVEL:
				// 				readMeshLodLevel(stream, pMesh);
				// 				break;
			case M_MESH_BOUNDS:
				readBoundsInfo(stream, info);
				break;
			case M_SUBMESH_NAME_TABLE:
				readSubMeshNameTable(stream, info);
				break;
				// 			case M_EDGE_LISTS:
				// 				readEdgeList(stream, pMesh);
				// 				break;
				// 			case M_POSES:
				// 				readPoses(stream, pMesh);
				// 				break;
				// 			case M_ANIMATIONS:
				// 				readAnimations(stream, pMesh);
				// 				break;
				// 			case M_TABLE_EXTREMES:
				// 				readExtremes(stream, pMesh);
				// 				break;
			}

			if (stream)
			{
				streamID = readChunk(stream);
			}

		}
		if (stream)
		{
			// Backpedal back to start of stream
			backpedalChunkHeader(stream);
		}
	}
}

bool parseOgreMeshFile(std::string filename, MeshInfo& info, ModelParseOptions o)
{
	std::ifstream stream(filename, std::ios::binary);

	if (!stream)
		return false;

	uint16_t headerID = 0;
	readData(stream, &headerID, 1);

	if (headerID != MeshChunkID::M_HEADER)
		return false;

	auto ver = readString(stream);

	auto streamID = readChunk(stream);

	while (stream)
	{
		switch (streamID)
		{
		case MeshChunkID::M_MESH:
			readMesh(stream, info, o);
			break;
		}

		streamID = readChunk(stream);
	}

	return true;
}

MeshInfo OgreMeshFileParser::load(std::string filename, ModelParseOptions o)
{
	MeshInfo out;

	if (!parseOgreMeshFile(filename, out, o))
	{
		FileLogger::logError("failed to load model file " + filename);
		return {};
	}

	return out;
}
