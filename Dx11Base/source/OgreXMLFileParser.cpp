#include "OgreXMLFileParser.h"
#include <boost/lexical_cast.hpp>
#include "AaModelSerialized.h"
#include <iostream>



AaModelInfo* OgreXMLFileParser::loadOgreXML(std::string filename, bool optimize, bool saveBinary )
{	
	if(!parseOgreXMLFile(filename,optimize))
	{
		AaLogger::getLogger()->writeMessage("ERROR failed to load model file "+filename);
		clearBuffers();
		return NULL;
	}

	AaModelInfo* model=createBuffers(optimize);
	model->rawInfo = new RawModelInfo();

	if(saveBinary)
		saveBinaryFile(filename,model->rawInfo);

	clearBuffers();

	return model;
}

bool OgreXMLFileParser::parseOgreXMLFile(std::string filename, bool optimize)
{
	TiXmlDocument document;

	if(!document.LoadFile(filename.c_str()))
		return false;
	
	const TiXmlElement* mainElement = document.RootElement();
	const TiXmlElement* submeshElement = mainElement->FirstChildElement("submeshes")->FirstChildElement("submesh");

	parseFaces(submeshElement->FirstChildElement("faces"));

	parseGeometry(submeshElement->FirstChildElement("geometry"), optimize);

	return true;
}

void OgreXMLFileParser::parseFaces(const TiXmlElement* facesElement)
{
	int face_count=boost::lexical_cast<int>(facesElement->Attribute("count"));
	index_count = face_count * 3;
	indices_ = new WORD[index_count];

	const TiXmlElement* faceElement = facesElement->FirstChildElement("face");

	for(int i=0;i<face_count;i++)
	{
		indices_[i*3] = boost::lexical_cast<WORD>(faceElement->Attribute("v1"));
		indices_[i*3+1] = boost::lexical_cast<WORD>(faceElement->Attribute("v2"));
		indices_[i*3+2] = boost::lexical_cast<WORD>(faceElement->Attribute("v3"));
			
		//char pp[50];
		//sprintf(pp,"Parsed face %d %d %d",indices_[i*3],indices_[i*3+1],indices_[i*3+2]);
		//OutputDebugString(pp);
		//OutputDebugString("\n");


		faceElement = faceElement->NextSiblingElement();
	}

	

}


void OgreXMLFileParser::saveBinaryFile(std::string filename, RawModelInfo* saveInfo)
{
	AaModelSerialized* model= new AaModelSerialized();
	model->useindices=true;
	model->vertex_count=vertex_count;
	model->index_count=index_count;
	model->indices_=indices_;

	//model->tangents_=tangents_;
	model->doCleanup=false;
	//remove ".xml"
	filename.resize(filename.length()-4);

	model->texCoords_= new float[vertex_count*2];
	model->positions_= new float[vertex_count*3];
	model->normals_= new float[vertex_count*3];
	model->tangents_= new float[vertex_count*3];

	for( int i = 0; i < vertex_count; i++ )
	{
		int offset=vertexVector.at(i);

		model->positions_[i*3] = vertices_[offset*3]; model->positions_[i*3+1] = vertices_[offset*3+1]; model->positions_[i*3+2] = vertices_[offset*3+2];
		model->normals_[i*3] = normals_[offset*3]; model->normals_[i*3+1] = normals_[offset*3+1]; model->normals_[i*3+2] = normals_[offset*3+2];
		model->tangents_[i*3] = tangents_[offset*3]; model->tangents_[i*3+1] = tangents_[offset*3+1]; model->tangents_[i*3+2] = tangents_[offset*3+2];
		model->texCoords_[i*2] = texCoords_[offset*2]; model->texCoords_[i*2+1] = texCoords_[offset*2+1];
	}

	{
		std::ofstream ofs(filename+".model",std::ios::binary);
		boost::archive::binary_oarchive oa(ofs);
		oa << model;
	}

	if(saveInfo!=NULL)
	{
		saveInfo->index_count = index_count;
		saveInfo->vertices = model->positions_;
		saveInfo->vertex_count = vertex_count;
		saveInfo->indices = indices_;
		indices_ = NULL;
	}
	else
	{
		delete [] model->positions_;
	}

	delete [] model->texCoords_;	
	delete [] model->normals_;
	delete [] model->tangents_;
	delete model;
}


void OgreXMLFileParser::parseGeometry(const TiXmlElement* geometryElement, bool optimize)
{
	optimize = false;

	vertex_count=boost::lexical_cast<int>(geometryElement->Attribute("vertexcount"));
	vertices_ = new float[vertex_count * 3];

	const TiXmlElement* vertexElement = geometryElement->FirstChildElement("vertexbuffer");
	
	bool use_normals;
	if(vertexElement->Attribute("normals")) 
	{
		use_normals=true;
		normals_= new float[vertex_count * 3];
	}
	else
	{
		use_normals=false;
		normals_=NULL;
	}

	bool use_tangents;
	if(vertexElement->Attribute("tangents")) 
	{
		use_tangents=true;
		tangents_= new float[vertex_count * 3];
	}
	else
	{
		use_tangents=false;
		tangents_=NULL;
	}

	int tc_count=boost::lexical_cast<int>(vertexElement->Attribute("texture_coords"));

	if(tc_count>0)
		texCoords_ = new float[vertex_count * 2];


	vertexElement = vertexElement->FirstChildElement("vertex");
	const TiXmlElement* vertexInfoElement;

	int validVertices=0;

	for(int i=0;i<vertex_count;i++)
	{

		vertexInfoElement=vertexElement->FirstChildElement("position");
		vertices_[i*3]=boost::lexical_cast<float>(vertexInfoElement->Attribute("x"));
		vertices_[i*3+1]=boost::lexical_cast<float>(vertexInfoElement->Attribute("y"));
		vertices_[i*3+2]=boost::lexical_cast<float>(vertexInfoElement->Attribute("z"));

		//char pp[50];
		//sprintf(pp,"Parsed vertex %f %f %f",vertices_[i*3],vertices_[i*3+1],vertices_[i*3+2]);
		//OutputDebugString(pp);
		//OutputDebugString("\n");

		if(use_normals)
		{
			vertexInfoElement=vertexElement->FirstChildElement("normal");
			normals_[i*3]=boost::lexical_cast<float>(vertexInfoElement->Attribute("x"));
			normals_[i*3+1]=boost::lexical_cast<float>(vertexInfoElement->Attribute("y"));
			normals_[i*3+2]=boost::lexical_cast<float>(vertexInfoElement->Attribute("z"));
		}

		if(use_tangents)
		{
			vertexInfoElement=vertexElement->FirstChildElement("tangent");
			tangents_[i*3]=boost::lexical_cast<float>(vertexInfoElement->Attribute("x"));
			tangents_[i*3+1]=boost::lexical_cast<float>(vertexInfoElement->Attribute("y"));
			tangents_[i*3+2]=boost::lexical_cast<float>(vertexInfoElement->Attribute("z"));
		}

		if(tc_count>0)
		{
			vertexInfoElement=vertexElement->FirstChildElement("texcoord");
			texCoords_[i*2]=boost::lexical_cast<float>(vertexInfoElement->Attribute("u"));
			texCoords_[i*2+1]=boost::lexical_cast<float>(vertexInfoElement->Attribute("v"));
		}	

		if(optimize)
		{
			//vertices differ by position+normal+texcoord
			std::string loadedVertexInfo;
			loadedVertexInfo.append(boost::lexical_cast<std::string>(vertices_[i*3]));
			loadedVertexInfo.append(boost::lexical_cast<std::string>(vertices_[i*3+1]));
			loadedVertexInfo.append(boost::lexical_cast<std::string>(vertices_[i*3+2]));
			loadedVertexInfo.append(boost::lexical_cast<std::string>(normals_[i*3]));
			loadedVertexInfo.append(boost::lexical_cast<std::string>(normals_[i*3+1]));
			loadedVertexInfo.append(boost::lexical_cast<std::string>(normals_[i*3+2]));
			loadedVertexInfo.append(boost::lexical_cast<std::string>(texCoords_[i*2]));
			loadedVertexInfo.append(boost::lexical_cast<std::string>(texCoords_[i*2+1]));
			//LoadedVertex lv={XMFLOAT3(vertices_[i*3],vertices_[i*3+1],vertices_[i*3+2]),XMFLOAT3(normals_[i*3],normals_[i*3+1],normals_[i*3+2]),XMFLOAT2(texCoords_[i*2],texCoords_[i*2+1])};
			
			std::map<std::string,int>::iterator it=vertexMap.find(loadedVertexInfo);

			if(it==vertexMap.end())
			{
				//index of useful vertices for later use
				vertexVector.push_back(i);
				//for unique checking
				vertexMap[loadedVertexInfo]=validVertices;
				//for changing of index buffer
				optimizedindices[i]=validVertices;

				validVertices++;
			}
			else
			{
				//for changing of index buffer
				optimizedindices[i]=it->second;
			}
		}
		else
		{
			vertexVector.push_back(i);
		}

		vertexElement = vertexElement->NextSiblingElement();

	}

	if(optimize)
	{
		for(int i=0;i<index_count;i++)
			indices_[i]=optimizedindices[i];

		vertex_count=vertexVector.size();
	}

	vertices = new VertexPos[vertex_count];
	verticesNT = new VertexPos2[vertex_count];

	for( int i = 0; i < vertex_count; i++ )
	{
		int offset=vertexVector.at(i);

		vertices[i].pos = XMFLOAT3( vertices_[offset*3], vertices_[offset*3 + 1], vertices_[offset*3 + 2] );
		vertices[i].tex0 = XMFLOAT2( texCoords_[offset*2] ,  texCoords_[offset*2 + 1] );

		verticesNT[i].norm = XMFLOAT3( normals_[offset*3], normals_[offset*3 + 1], normals_[offset*3 + 2] );
		verticesNT[i].tang = XMFLOAT3( tangents_[offset*3], tangents_[offset*3 + 1], tangents_[offset*3 + 2] );
	}

}

AaModelInfo* OgreXMLFileParser::createBuffers(bool optimize)
{
	AaModelInfo* model= new AaModelInfo;

	D3D11_BUFFER_DESC indexDesc;
	ZeroMemory( &indexDesc, sizeof( indexDesc ) );
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexDesc.ByteWidth = sizeof( WORD ) * index_count;
	indexDesc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA resourceData;
	ZeroMemory( &resourceData, sizeof( resourceData ) );
	resourceData.pSysMem = indices_;
	HRESULT d3dResult = d3dDevice->CreateBuffer( &indexDesc, &resourceData, &model->indexBuffer_ );

	if( FAILED( d3dResult ) )
	{
		DXTRACE_MSG( "Failed to create index buffer!" );
		return NULL;
	}


	D3D11_BUFFER_DESC vertexDesc;
	ZeroMemory( &vertexDesc, sizeof( vertexDesc ) );
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexDesc.ByteWidth = sizeof( VertexPos ) * vertex_count;
	ZeroMemory( &resourceData, sizeof( resourceData ) );
	resourceData.pSysMem = vertices;

	d3dResult = d3dDevice->CreateBuffer( &vertexDesc, &resourceData,	&model->vertexBuffers_[0]);

	if( FAILED( d3dResult ) )
	{
		DXTRACE_MSG( "Failed to create vertex buffer!" );
		return false;
	}


	ZeroMemory( &vertexDesc, sizeof( vertexDesc ) );
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexDesc.ByteWidth = sizeof( VertexPos2 ) * vertex_count;
	ZeroMemory( &resourceData, sizeof( resourceData ) );
	resourceData.pSysMem = verticesNT;

	d3dResult = d3dDevice->CreateBuffer( &vertexDesc, &resourceData,	&model->vertexBuffers_[1] );

	if( FAILED( d3dResult ) )
	{
		DXTRACE_MSG( "Failed to create vertex buffer!" );
		return false;
	}

	delete[] vertices;
	delete[] verticesNT;

	//input do vertex shadera
	D3D11_INPUT_ELEMENT_DESC vLp= { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
	D3D11_INPUT_ELEMENT_DESC vLtc = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 };

	D3D11_INPUT_ELEMENT_DESC vLn = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 };
	D3D11_INPUT_ELEMENT_DESC vLt = { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 };

	model->vertexLayout = new D3D11_INPUT_ELEMENT_DESC[4];
	model->vertexLayout[0] = vLp;
	model->vertexLayout[1] = vLtc;
	model->vertexLayout[2] = vLn;
	model->vertexLayout[3] = vLt;

	model->vBuffersCount = 2;
	model->totalLayoutElements = 4;
	model->usesIndexBuffer=true;
	model->vertexCount=index_count;

	return model;
}

void OgreXMLFileParser::clearBuffers()
{
	vertexMap.clear();
	vertexVector.clear();
	optimizedindices.clear();

	if(vertices_) delete vertices_;
	if(normals_)  delete normals_;
	if(tangents_)  delete tangents_;
	if(texCoords_)   delete texCoords_;
	if(indices_)   delete indices_;

	vertices_=0;
	normals_=0;
	tangents_=0;
	texCoords_=0;
	indices_=0;
	index_count=0;
	vertex_count=0;
}
