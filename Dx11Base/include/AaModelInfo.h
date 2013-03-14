#ifndef _AA_MODEL_INFO_
#define _AA_MODEL_INFO_

struct VertexPos
{
	XMFLOAT3 pos;
	XMFLOAT2 tex0;
	XMFLOAT3 norm;
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
	ID3D11Buffer* vertexBuffer_;
	ID3D11Buffer* indexBuffer_;
	bool usesIndexBuffer;
	ID3D11InputLayout* inputLayout; 
	UINT vertexCount;
	RawModelInfo* rawInfo;
};

#endif