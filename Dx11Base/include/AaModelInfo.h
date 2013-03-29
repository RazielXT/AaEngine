#ifndef _AA_MODEL_INFO_
#define _AA_MODEL_INFO_

struct VertexPos
{
	XMFLOAT3 pos;
	XMFLOAT2 tex0;
};

struct VertexPos2
{
	XMFLOAT3 norm;
	XMFLOAT3 tang;
};

struct RawModelInfo
{
	float* vertices;
	WORD* indices;
	int vertex_count;
	int index_count;
};

struct AaModelInfo
{
	D3D11_INPUT_ELEMENT_DESC* vertexLayout;
	unsigned int totalLayoutElements;
	ID3D11Buffer* vertexBuffers_[3];
	UCHAR vBuffersCount;
	ID3D11Buffer* indexBuffer_;
	bool usesIndexBuffer;
	
	UINT vertexCount;
	RawModelInfo* rawInfo;

	AaModelInfo()
	{
		vBuffersCount = 0;
		usesIndexBuffer = false;
	}

	~AaModelInfo()
	{
		if(usesIndexBuffer)
			indexBuffer_->Release();

		for (int i = 0;i<vBuffersCount;i++)
			vertexBuffers_[i]->Release();

		delete rawInfo;
		delete [] vertexLayout;
	}
};

#endif