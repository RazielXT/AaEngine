#include "AaBinaryModelLoader.h"
#include "AaModelSerialized.h"


AaModelInfo* AaBinaryModelLoader::load(std::string filename)
{

	AaModelSerialized* m;

	std::ifstream ifs(filename, std::ios::binary);
	
	if(ifs.fail())
	{
		AaLogger::getLogger()->writeMessage("ERROR Failed to open file "+filename);
		return NULL;
	}

	{
		boost::archive::binary_iarchive ia(ifs);
		ia >> m;
	}

	RawModelInfo* rawModel = new RawModelInfo;
	rawModel->vertices = new float[m->vertex_count*3];
	rawModel->vertex_count = m->vertex_count;

	VertexPos* vertices = new VertexPos[m->vertex_count];
	VertexPos2* vertices2 = new VertexPos2[m->vertex_count];

	for( int i = 0; i < m->vertex_count; i++ )
	{
		vertices[i].pos = XMFLOAT3( m->positions_[i*3], m->positions_[i*3 + 1], m->positions_[i*3 + 2] );
		vertices[i].tex0 = XMFLOAT2( m->texCoords_[i*2] ,  m->texCoords_[i*2 + 1] );

		vertices2[i].norm = XMFLOAT3( m->normals_[i*3], m->normals_[i*3 + 1], m->normals_[i*3 + 2] );
		vertices2[i].tang = XMFLOAT3( m->tangents_[i*3], m->tangents_[i*3 + 1], m->tangents_[i*3 + 2] );

		rawModel->vertices[i*3] = m->positions_[i*3];
		rawModel->vertices[i*3+1] = m->positions_[i*3+1];
		rawModel->vertices[i*3+2] = m->positions_[i*3+2];
	}

	if(m->useindices)
	{
		rawModel->indices = new WORD[m->index_count];

		for( int i = 0; i < m->index_count; i++ )
		{
			rawModel->indices[i] = m->indices_[i];
		}

		rawModel->index_count = m->index_count;
	}
	else
	{
		rawModel->index_count = 0;
	}

	AaModelInfo* model= new AaModelInfo();
	model->rawInfo = rawModel;

	D3D11_BUFFER_DESC indexDesc;
	ZeroMemory( &indexDesc, sizeof( indexDesc ) );
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexDesc.ByteWidth = sizeof( WORD ) * m->index_count;
	indexDesc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA resourceData;
	ZeroMemory( &resourceData, sizeof( resourceData ) );
	resourceData.pSysMem = m->indices_;
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
	vertexDesc.ByteWidth = sizeof( VertexPos ) * m->vertex_count;
	ZeroMemory( &resourceData, sizeof( resourceData ) );
	resourceData.pSysMem = vertices;

	d3dResult = d3dDevice->CreateBuffer( &vertexDesc, &resourceData,	&model->vertexBuffers_[0] );

	if( FAILED( d3dResult ) )
	{
		DXTRACE_MSG( "Failed to create vertex buffer!" );
		return false;
	}

	ZeroMemory( &vertexDesc, sizeof( vertexDesc ) );
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexDesc.ByteWidth = sizeof( VertexPos2 ) * m->vertex_count;
	ZeroMemory( &resourceData, sizeof( resourceData ) );
	resourceData.pSysMem = vertices2;

	d3dResult = d3dDevice->CreateBuffer( &vertexDesc, &resourceData,	&model->vertexBuffers_[1] );

	if( FAILED( d3dResult ) )
	{
		DXTRACE_MSG( "Failed to create vertex buffer!" );
		return false;
	}


	//input do vertex shadera
	D3D11_INPUT_ELEMENT_DESC vLp= { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
	D3D11_INPUT_ELEMENT_DESC vLtc = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 };

	D3D11_INPUT_ELEMENT_DESC vLn = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
	D3D11_INPUT_ELEMENT_DESC vLt = { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 };

	model->vertexLayout = new D3D11_INPUT_ELEMENT_DESC[4];
	model->vertexLayout[0] = vLp;
	model->vertexLayout[1] = vLtc;
	model->vertexLayout[2] = vLn;
	model->vertexLayout[3] = vLt;

	model->totalLayoutElements = 4;
	model->vBuffersCount = 2;
	model->usesIndexBuffer=true;
	model->vertexCount=m->index_count;

	delete m;
	delete [] vertices;
	delete [] vertices2;

	return model;
}