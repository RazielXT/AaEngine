#include "ObjFileParser.h"
#include "StringUtils.h"

AaModelInfo* ObjFileParser::loadOBJ(std::string filename)
{
	auto model = new AaModelInfo;

	if (!loadFile(filename, model))
	{
		//DXTRACE_MSG( "Error loading obj file!" );
		delete model;
		return nullptr;
	}

	return model;
}

bool ObjFileParser::loadFile(std::string filename, AaModelInfo* model)
{
	std::ifstream fileStream(filename);

	if (!fileStream.is_open())
		return false;

	std::string line;
	bool error = false;

	std::vector<float> verts, norms, texC;
	std::vector<int> faces;

	while (getline(fileStream, line) && !error)
	{
		line = StringUtils::toNextWord(line);

		try
		{
			if (StringUtils::getToken(line, "vn"))
			{
				norms.push_back((float)atof(StringUtils::onlyNextWordAndContinue(line).c_str()));
				norms.push_back((float)atof(StringUtils::onlyNextWordAndContinue(line).c_str()));
				norms.push_back((float)atof(StringUtils::onlyNextWordAndContinue(line).c_str()));
			}
			else if (StringUtils::getToken(line, "vt"))
			{
				texC.push_back((float)atof(StringUtils::onlyNextWordAndContinue(line).c_str()));
				texC.push_back((float)atof(StringUtils::onlyNextWordAndContinue(line).c_str()));
			}
			else if (StringUtils::getToken(line, "v"))
			{
				verts.push_back((float)atof(StringUtils::onlyNextWordAndContinue(line).c_str()));
				verts.push_back((float)atof(StringUtils::onlyNextWordAndContinue(line).c_str()));
				verts.push_back((float)atof(StringUtils::onlyNextWordAndContinue(line).c_str()));
			}
			else if (StringUtils::getToken(line, "f"))
			{
				for (int i = 0; i < 3; i++)
				{
					faces.push_back((int)atoi(StringUtils::onlyNextWordToSeparatorAndContinue(line, '/').c_str()));
					faces.push_back((int)atoi(StringUtils::onlyNextWordToSeparatorAndContinue(line, '/').c_str()));
					faces.push_back((int)atoi(StringUtils::onlyNextWordToSeparatorAndContinue(line, '/').c_str()));
				}
			}
		}
		catch (std::exception ex)
		{
			return false;
		}
	}

	// "Unroll" the loaded obj information into a list of triangles.
	int vIndex = 0, nIndex = 0, tIndex = 0;
	int numFaces = (int)faces.size() / 9;

	auto& info = model->rawInfo;
	info.vertexCount = numFaces * 3;
	info.positions.resize(info.vertexCount * 3);

	if ((int)norms.size() != 0)
	{
		info.normals.resize(info.vertexCount * 3);
	}
	if ((int)texC.size() != 0)
	{
		info.texCoords.resize(info.vertexCount * 2);
	}

	for (int f = 0; f < (int)faces.size(); f += 3)
	{
		info.positions[vIndex + 0] = verts[(faces[f + 0] - 1) * 3 + 0];
		info.positions[vIndex + 1] = verts[(faces[f + 0] - 1) * 3 + 1];
		info.positions[vIndex + 2] = verts[(faces[f + 0] - 1) * 3 + 2];
		vIndex += 3;

		if (!info.texCoords.empty())
		{
			info.texCoords[tIndex + 0] = texC[(faces[f + 1] - 1) * 2 + 0];
			info.texCoords[tIndex + 1] = texC[(faces[f + 1] - 1) * 2 + 1];
			tIndex += 2;
		}
		if (!info.normals.empty())
		{
			info.normals[nIndex + 0] = norms[(faces[f + 2] - 1) * 3 + 0];
			info.normals[nIndex + 1] = norms[(faces[f + 2] - 1) * 3 + 1];
			info.normals[nIndex + 2] = norms[(faces[f + 2] - 1) * 3 + 2];
			nIndex += 3;
		}
	}

	return true;
}

// bool ObjFileParser::loadContent(AaModelInfo* model)
// {
// 	VertexPos* vertices = new VertexPos[totalVerts_];
// 	float* vertsPtr = vertices_;
// 	float* texCPtr = texCoords_;
// 	float* normPtr = normals_;
// 
// 	for (int i = 0; i < totalVerts_; i++)
// 	{
// 		vertices[i].pos = XMFLOAT3(*(vertsPtr + 0), *(vertsPtr + 1), *(vertsPtr + 2));
// 		vertsPtr += 3;
// 		vertices[i].tex0 = XMFLOAT2(*(texCPtr + 0), *(texCPtr + 1));
// 		texCPtr += 2;
// 		//vertices[i].norm = XMFLOAT3( *(normPtr + 0), *(normPtr + 1), *(normPtr + 2) );
// 		//normPtr += 3;
// 	}
// 
// 	D3D11_BUFFER_DESC vertexDesc;
// 	ZeroMemory(&vertexDesc, sizeof(vertexDesc));
// 	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
// 	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
// 	vertexDesc.ByteWidth = sizeof(VertexPos) * totalVerts_;
// 	D3D11_SUBRESOURCE_DATA resourceData;
// 	ZeroMemory(&resourceData, sizeof(resourceData));
// 	resourceData.pSysMem = vertices;
// 
// 	// 	HRESULT d3dResult;// = d3dDevice->CreateBuffer( &vertexDesc, &resourceData,	&model->vertexBuffer_ );
// 	// 
// 	// 	if( FAILED( d3dResult ) )
// 	// 	{
// 	// 		//DXTRACE_MSG( "Failed to create vertex buffer!" );
// 	// 		return false;
// 	// 	}
// 
// 	delete[] vertices;
// 
// 	//input do vertex shadera
// 	D3D11_INPUT_ELEMENT_DESC vLp = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
// 	D3D11_INPUT_ELEMENT_DESC vLtc = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 };
// 	D3D11_INPUT_ELEMENT_DESC vLn = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 };
// 
// 	model->vertexLayout = new D3D11_INPUT_ELEMENT_DESC[3];
// 	model->vertexLayout[0] = vLp;
// 	model->vertexLayout[1] = vLtc;
// 	model->vertexLayout[2] = vLn;
// 
// 	model->totalLayoutElements = 3;
// 	model->vertexCount = totalVerts_;
// 	model->usesIndexBuffer = false;
// 
// 	return true;
// }
