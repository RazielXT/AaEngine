#include "OgreXMLFileParser.h"
#include "AaModelSerialization.h"
#include "Xml.h"
#include "AaLogger.h"
#include <map>
#include "AaBinaryModelLoader.h"

void parseFaces(const XmlParser::Element* facesElement, ModelInfo& info)
{
	int face_count = facesElement->attributeAs<int>("count");
	info.indexCount = face_count * 3;
	info.indices.resize(info.indexCount);

	auto faceElement = facesElement->firstNode("face");

	for (int i = 0; i < face_count; i++)
	{
		info.indices[i * 3] = faceElement->attributeAs<uint16_t>("v1");
		info.indices[i * 3 + 1] = faceElement->attributeAs<uint16_t>("v2");
		info.indices[i * 3 + 2] = faceElement->attributeAs<uint16_t>("v3");

		faceElement = faceElement->nextSibling();
	}
}

void parseGeometry(const XmlParser::Element* geometryElement, ModelInfo& info, bool optimize)
{
	optimize = false;

	info.vertexCount = geometryElement->attributeAs<uint32_t>("vertexcount");
	info.positions.resize(info.vertexCount * 3);

	auto vertexElement = geometryElement->firstNode("vertexbuffer");

	bool use_normals;
	if (vertexElement->attribute("normals") == std::string("true"))
	{
		use_normals = true;
		info.normals.resize(info.vertexCount * 3);
	}
	else
	{
		use_normals = false;
	}

	bool use_tangents;
	if (vertexElement->attribute("tangents") == std::string("true") || vertexElement->attribute("tangents") == std::string("True"))
	{
		use_tangents = true;
		info.tangents.resize(info.vertexCount * 3);
	}
	else
	{
		use_tangents = false;
	}

	int tc_count = vertexElement->attributeAs<int>("texture_coords");

	if (tc_count > 0)
		info.texCoords.resize(info.vertexCount * 2);

	vertexElement = vertexElement->firstNode("vertex");

	int validVertices = 0;
	std::map<std::string, int> vertexMap;
	std::vector<int> vertexVector;
	std::map<int, int> optimizedindices;

	for (uint32_t i = 0; i < info.vertexCount; i++)
	{
		auto vertexInfoElement = vertexElement->firstNode("position");
		info.positions[i * 3] = vertexInfoElement->attributeAs<float>("x");
		info.positions[i * 3 + 1] = vertexInfoElement->attributeAs<float>("y");
		info.positions[i * 3 + 2] = vertexInfoElement->attributeAs<float>("z");

		if (use_normals)
		{
			vertexInfoElement = vertexElement->firstNode("normal");
			info.normals[i * 3] = vertexInfoElement->attributeAs<float>("x");
			info.normals[i * 3 + 1] = vertexInfoElement->attributeAs<float>("y");
			info.normals[i * 3 + 2] = vertexInfoElement->attributeAs<float>("z");
		}

		if (use_tangents)
		{
			vertexInfoElement = vertexElement->firstNode("tangent");
			info.tangents[i * 3] = vertexInfoElement->attributeAs<float>("x");
			info.tangents[i * 3 + 1] = vertexInfoElement->attributeAs<float>("y");
			info.tangents[i * 3 + 2] = vertexInfoElement->attributeAs<float>("z");
		}

		if (tc_count > 0)
		{
			vertexInfoElement = vertexElement->firstNode("texcoord");
			info.texCoords[i * 2] = vertexInfoElement->attributeAs<float>("u");
			info.texCoords[i * 2 + 1] = vertexInfoElement->attributeAs<float>("v");
		}

		if (optimize)
		{
			//vertices differ by position+normal+texcoord
			std::string loadedVertexInfo;
			loadedVertexInfo.append(std::to_string(info.positions[i * 3]));
			loadedVertexInfo.append(std::to_string(info.positions[i * 3 + 1]));
			loadedVertexInfo.append(std::to_string(info.positions[i * 3 + 2]));
			loadedVertexInfo.append(std::to_string(info.normals[i * 3]));
			loadedVertexInfo.append(std::to_string(info.normals[i * 3 + 1]));
			loadedVertexInfo.append(std::to_string(info.normals[i * 3 + 2]));
			loadedVertexInfo.append(std::to_string(info.texCoords[i * 2]));
			loadedVertexInfo.append(std::to_string(info.texCoords[i * 2 + 1]));
			//LoadedVertex lv={XMFLOAT3(vertices_[i*3],vertices_[i*3+1],vertices_[i*3+2]),XMFLOAT3(normals_[i*3],normals_[i*3+1],normals_[i*3+2]),XMFLOAT2(texCoords_[i*2],texCoords_[i*2+1])};

			auto it = vertexMap.find(loadedVertexInfo);

			if (it == vertexMap.end())
			{
				//index of useful vertices for later use
				vertexVector.push_back(i);
				//for unique checking
				vertexMap[loadedVertexInfo] = validVertices;
				//for changing of index buffer
				optimizedindices[i] = validVertices;

				validVertices++;
			}
			else
			{
				//for changing of index buffer
				optimizedindices[i] = it->second;
			}
		}
		else
		{
			vertexVector.push_back(i);
		}

		vertexElement = vertexElement->nextSibling();
	}

	if (optimize)
	{
		for (int i = 0; i < info.indexCount; i++)
			info.indices[i] = optimizedindices[i];

		info.vertexCount = vertexVector.size();

// 		for (int i = 0; i < info.vertexCount; i++)
// 		{
// 			int offset = vertexVector[i];
// 
// 			m.positions[i * 3] = m.positions[offset * 3]; ... , m.positions[offset * 3 + 1], m.positions[offset * 3 + 2]);
// 			vertices[i].tex0 = XMFLOAT2(m.texCoords[offset * 2], m.texCoords[offset * 2 + 1]);
// 			vertices2[i].norm = XMFLOAT3(m.normals[offset * 3], m.normals[offset * 3 + 1], m.normals[offset * 3 + 2]);
// 
// 			if (!m.tangents.empty())
// 				vertices2[i].tang = XMFLOAT3(m.tangents[offset * 3]
// 		}
	}
}

bool parseOgreXMLFile(std::string filename, ModelInfo& info, bool optimize)
{
	XmlParser::Document document;

	if (!document.load(filename.c_str()))
		return false;

	auto mainElement = document.getRoot();
	auto submeshElement = mainElement->firstNode("submeshes")->firstNode("submesh");

	parseFaces(submeshElement->firstNode("faces"), info);

	auto geometryNode = submeshElement->firstNode("geometry");
	if (!geometryNode)
		geometryNode = mainElement->firstNode("sharedgeometry");

	parseGeometry(geometryNode, info, optimize);

	return true;
}

AaModel* OgreXMLFileParser::load(std::string filename, AaRenderSystem* rs, bool optimize /*= true*/, bool saveBinary /*= false*/)
{
	ModelInfo info{};

	if (!parseOgreXMLFile(filename, info, optimize))
	{
		AaLogger::logError("failed to load model file " + filename);
		return nullptr;
	}

	if (saveBinary)
	{
		//replace .xml with .model
		filename.resize(filename.length() - 3);
		filename += "model";
		AaModelSerialization::SaveModel(filename, info);
	}

	return AaBinaryModelLoader::load(info, rs);
}
