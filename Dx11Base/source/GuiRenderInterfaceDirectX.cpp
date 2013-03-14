#include "GuiRenderInterfaceDirectX.h"
#include <Rocket/Core.h>
#include "GlobalDefinitions.h"

struct VertexShaderConstants
{
	XMMATRIX world_transform;
	XMMATRIX rot_transform;
	XMFLOAT4 scissors;
	FLOAT alpha;
};

// This structure is created for each set of geometry that Rocket compiles. It stores the vertex and index buffers and
// the texture associated with the geometry, if one was specified.
struct RocketD3D11CompiledGeometry
{
	ID3D11Buffer* vertices;
	DWORD num_vertices;

	ID3D11Buffer* indices;
	DWORD num_primitives;

	ID3D11ShaderResourceView* texture;
};

// The internal format of the vertex we use for rendering Rocket geometry. We could optimise space by having a second
// untextured vertex for use when rendering coloured borders and backgrounds.
struct RocketD3D11Vertex
{
	FLOAT x, y, z;
	FLOAT r,g,b,a;
	FLOAT u, v;
};

RenderInterfaceDirectX::RenderInterfaceDirectX(ID3D11Device* d3dDevice,ID3D11DeviceContext* d3dContext, int width, int heigth)
{
	d3dDevice_ = d3dDevice;
	d3dContext_ = d3dContext;

	this->width = width;
	this->heigth = heigth;

	LoadGuiShaders();

}

RenderInterfaceDirectX::~RenderInterfaceDirectX()
{
}

void RenderInterfaceDirectX::LoadGuiShaders()
{
	//vertex shader
	DWORD shaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
	ID3DBlob* errorBuffer = 0;
	HRESULT result;
	std::string guiShaderPath = std::string(std::string(GUI_DIRECTORY) + std::string("guiShader.fx"));

	result = D3DX11CompileFromFile( guiShaderPath.c_str(), 0, 0, "VS_Main", "vs_4_0", shaderFlags, 0, 0, &vsBuffer, &errorBuffer, 0 );

	if( FAILED( result ) )
	{
		if( errorBuffer != 0 )
		{
		OutputDebugStringA(( char* )errorBuffer->GetBufferPointer( ));
		std::string errorMessage=( char* )errorBuffer->GetBufferPointer( );
		errorBuffer->Release();
		}
	}

	result = d3dDevice_->CreateVertexShader( vsBuffer->GetBufferPointer( ),	vsBuffer->GetBufferSize( ), 0, &vs_program );



	//vertex shader input layout
	D3D11_INPUT_ELEMENT_DESC solidColorLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	result = d3dDevice_->CreateInputLayout( solidColorLayout,3, vsBuffer->GetBufferPointer(), vsBuffer->GetBufferSize( ), &input_layout );

	//pixel shader
	ID3DBlob* psBuffer = 0;
	errorBuffer = 0;
	result = D3DX11CompileFromFile( guiShaderPath.c_str(), 0, 0, "PS_Main", "ps_4_0", shaderFlags, 0, 0, &psBuffer, &errorBuffer, 0 );
	
	if( errorBuffer != 0 )
		errorBuffer->Release( );
	
	result = d3dDevice_->CreatePixelShader( psBuffer->GetBufferPointer( ), psBuffer->GetBufferSize( ), 0, &ps_program );

	psBuffer->Release( );
	
	//pixel shader
	result = D3DX11CompileFromFile( guiShaderPath.c_str(), 0, 0, "PS_Main2", "ps_4_0", shaderFlags, 0, 0, &psBuffer, &errorBuffer, 0 );
	
	if( errorBuffer != 0 )
		errorBuffer->Release( );
	
	result = d3dDevice_->CreatePixelShader( psBuffer->GetBufferPointer( ), psBuffer->GetBufferSize( ), 0, &ps_program_notexture );

	psBuffer->Release( );

	//shader constant
	D3D11_BUFFER_DESC constDesc;
	ZeroMemory( &constDesc, sizeof( constDesc ) );
	constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constDesc.ByteWidth = sizeof( VertexShaderConstants );
	constDesc.Usage = D3D11_USAGE_DEFAULT;
	d3dDevice_->CreateBuffer( &constDesc, 0, &wtransf_ );

	//sampler
	D3D11_SAMPLER_DESC textureMapDesc;
	ZeroMemory( &textureMapDesc, sizeof( textureMapDesc ) );
	textureMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	textureMapDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	textureMapDesc.MaxAnisotropy = 8;
	textureMapDesc.MaxLOD = D3D11_FLOAT32_MAX;
	d3dDevice_->CreateSamplerState( &textureMapDesc, &sampler );
}

// Called by Rocket when it wants to render geometry that it does not wish to optimise.
void RenderInterfaceDirectX::RenderGeometry(Rocket::Core::Vertex* ROCKET_UNUSED(vertices), int ROCKET_UNUSED(num_vertices), int* ROCKET_UNUSED(indices), int ROCKET_UNUSED(num_indices), const Rocket::Core::TextureHandle ROCKET_UNUSED(texture), const Rocket::Core::Vector2f& ROCKET_UNUSED(translation))
{
	// We've chosen to not support non-compiled geometry in the DirectX renderer. If you wanted to render non-compiled
	// geometry, for example for very small sections of geometry, you could use DrawIndexedPrimitiveUP or write to a
	// dynamic vertex buffer which is flushed when either the texture changes or compiled geometry is drawn.
}

// Called by Rocket when it wants to compile geometry it believes will be static for the forseeable future.
Rocket::Core::CompiledGeometryHandle RenderInterfaceDirectX::CompileGeometry(Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rocket::Core::TextureHandle texture)
{
	// Construct a new RocketD3D9CompiledGeometry structure, which will be returned as the handle, and the buffers to
	// store the geometry.
	RocketD3D11CompiledGeometry* geometry = new RocketD3D11CompiledGeometry();


	// Fill the vertex buffer.
	RocketD3D11Vertex* d3d11_vertices = new RocketD3D11Vertex[num_vertices];
	for (int i = 0; i < num_vertices; ++i)
	{
		d3d11_vertices[i].x = vertices[i].position.x;
		d3d11_vertices[i].y = vertices[i].position.y;
		d3d11_vertices[i].z = 0;
		d3d11_vertices[i].r = vertices[i].colour.red/255.0f;
		d3d11_vertices[i].g = vertices[i].colour.green/255.0f;
		d3d11_vertices[i].b = vertices[i].colour.blue/255.0f;
		d3d11_vertices[i].a = vertices[i].colour.alpha/255.0f;
		d3d11_vertices[i].u = vertices[i].tex_coord[0];
		d3d11_vertices[i].v = vertices[i].tex_coord[1];
	}

	D3D11_BUFFER_DESC vertexDesc;
	ZeroMemory( &vertexDesc, sizeof( vertexDesc ) );
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexDesc.ByteWidth = sizeof( RocketD3D11Vertex ) * num_vertices;
	D3D11_SUBRESOURCE_DATA resourceData;
	ZeroMemory( &resourceData, sizeof( resourceData ) );
	resourceData.pSysMem = d3d11_vertices;

	d3dDevice_->CreateBuffer( &vertexDesc, &resourceData, &geometry->vertices);


	// Fill the index buffer.
	D3D11_BUFFER_DESC indexDesc;
	ZeroMemory( &indexDesc, sizeof( indexDesc ) );
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexDesc.ByteWidth = sizeof( unsigned int ) * num_indices;
	indexDesc.CPUAccessFlags = 0;
	ZeroMemory( &resourceData, sizeof( resourceData ) );
	resourceData.pSysMem = indices;

	d3dDevice_->CreateBuffer( &indexDesc, &resourceData, &geometry->indices );


	geometry->num_vertices = (DWORD) num_vertices;
	geometry->num_primitives = (DWORD) num_indices / 3;

	geometry->texture = texture == NULL ? NULL : (ID3D11ShaderResourceView*) texture;

	return reinterpret_cast<Rocket::Core::CompiledGeometryHandle> (geometry);
}

// Called by Rocket when it wants to render application-compiled geometry.
void RenderInterfaceDirectX::RenderCompiledGeometry(Rocket::Core::CompiledGeometryHandle geometry, const Rocket::Core::Vector2f& translation)
{

	RocketD3D11CompiledGeometry* d3d11_geometry = (RocketD3D11CompiledGeometry*) geometry;

	// Build and set the transform matrix.
	VertexShaderConstants con;
	con.world_transform = XMMatrixTranslation(translation.x, translation.y, 0);
	con.world_transform = XMMatrixMultiply(con.world_transform,XMMatrixRotationRollPitchYaw(0,0,0.1));
	con.world_transform = XMMatrixMultiply(con.world_transform,XMMatrixOrthographicOffCenterLH(0, width, 0, heigth, -1, 1));	
		
	con.world_transform = XMMatrixTranspose( con.world_transform );

	con.rot_transform = XMMatrixRotationRollPitchYaw(0,0,0.1);

	if(sc)
	con.scissors = scissors;
	else
	con.scissors = XMFLOAT4(0,width,0,heigth);
	con.alpha = 1;

	unsigned int stride = sizeof( RocketD3D11Vertex );
	unsigned int offset = 0;

	d3dContext_->IASetInputLayout(input_layout );
	d3dContext_->IASetVertexBuffers( 0, 1, &d3d11_geometry->vertices, &stride, &offset );
	d3dContext_->IASetIndexBuffer( d3d11_geometry->indices , DXGI_FORMAT_R32_UINT, 0 );

	d3dContext_->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	d3dContext_->VSSetShader( vs_program, 0, 0 );
	

	d3dContext_->UpdateSubresource( wtransf_, 0, 0, &con, 0, 0 );
	d3dContext_->VSSetConstantBuffers( 0, 1, &wtransf_ );
	d3dContext_->PSSetConstantBuffers( 0, 1, &wtransf_ );

	if (d3d11_geometry->texture != NULL)
	{
		d3dContext_->PSSetShaderResources( 0, 1, &d3d11_geometry->texture );
		d3dContext_->PSSetSamplers( 0, 1, &sampler );
		d3dContext_->PSSetShader( ps_program, 0, 0 );
	}
	else
	{
		d3dContext_->PSSetShaderResources( 0, 0, 0 );
		d3dContext_->PSSetSamplers( 0, 0, 0 );
		d3dContext_->PSSetShader( ps_program_notexture, 0, 0 );
	}

	d3dContext_->DrawIndexed( d3d11_geometry->num_primitives*3, 0, 0 );
}

// Called by Rocket when it wants to release application-compiled geometry.
void RenderInterfaceDirectX::ReleaseCompiledGeometry(Rocket::Core::CompiledGeometryHandle geometry)
{
	RocketD3D11CompiledGeometry* d3d11_geometry = (RocketD3D11CompiledGeometry*) geometry;

	d3d11_geometry->vertices->Release();
	d3d11_geometry->indices->Release();

	delete d3d11_geometry;
}

// Called by Rocket when it wants to enable or disable scissoring to clip content.
void RenderInterfaceDirectX::EnableScissorRegion(bool enable)
{
	if(enable)
		sc=true;
	else
	{
		sc=false;
		scissors = XMFLOAT4(0,width,0,heigth);
	}
}

// Called by Rocket when it wants to change the scissor region.
void RenderInterfaceDirectX::SetScissorRegion(int x, int y, int width, int height)
{
	scissors = XMFLOAT4((FLOAT)x,(FLOAT)x+width,(FLOAT)y,(FLOAT)y+height);
}

// Set to byte packing, or the compiler will expand our struct, which means it won't read correctly from file
#pragma pack(1) 
struct TGAHeader 
{
	char  idLength;
	char  colourMapType;
	char  dataType;
	short int colourMapOrigin;
	short int colourMapLength;
	char  colourMapDepth;
	short int xOrigin;
	short int yOrigin;
	short int width;
	short int height;
	char  bitsPerPixel;
	char  imageDescriptor;
};
// Restore packing
#pragma pack()

// Called by Rocket when a texture is required by the library.
bool RenderInterfaceDirectX::LoadTexture(Rocket::Core::TextureHandle& texture_handle, Rocket::Core::Vector2i& texture_dimensions, const Rocket::Core::String& source)
{
	Rocket::Core::FileInterface* file_interface = Rocket::Core::GetFileInterface();
	Rocket::Core::FileHandle file_handle = file_interface->Open(source);
	if (file_handle == NULL)
		return false;

	file_interface->Seek(file_handle, 0, SEEK_END);
	size_t buffer_size = file_interface->Tell(file_handle);
	file_interface->Seek(file_handle, 0, SEEK_SET);
	
	char* buffer = new char[buffer_size];
	file_interface->Read(buffer, buffer_size, file_handle);
	file_interface->Close(file_handle);

	TGAHeader header;
	memcpy(&header, buffer, sizeof(TGAHeader));
	
	int color_mode = header.bitsPerPixel / 8;
	int image_size = header.width * header.height * 4; // We always make 32bit textures 
	
	if (header.dataType != 2)
	{
		Rocket::Core::Log::Message(Rocket::Core::Log::LT_ERROR, "Only 24/32bit uncompressed TGAs are supported.");
		return false;
	}
	
	// Ensure we have at least 3 colors
	if (color_mode < 3)
	{
		Rocket::Core::Log::Message(Rocket::Core::Log::LT_ERROR, "Only 24 and 32bit textures are supported");
		return false;
	}
	
	const char* image_src = buffer + sizeof(TGAHeader);
	unsigned char* image_dest = new unsigned char[image_size];
	
	// Targa is BGR, swap to RGB and flip Y axis
	for (long y = 0; y < header.height; y++)
	{
		long read_index = y * header.width * color_mode;
		long write_index = ((header.imageDescriptor & 32) != 0) ? read_index : (header.height - y - 1) * header.width * color_mode;
		for (long x = 0; x < header.width; x++)
		{
			image_dest[write_index] = image_src[read_index+2];
			image_dest[write_index+1] = image_src[read_index+1];
			image_dest[write_index+2] = image_src[read_index];
			if (color_mode == 4)
				image_dest[write_index+3] = image_src[read_index+3];
			else
				image_dest[write_index+3] = 255;
			
			write_index += 4;
			read_index += color_mode;
		}
	}

	texture_dimensions.x = header.width;
	texture_dimensions.y = header.height;
	
	bool success = GenerateTexture(texture_handle, image_dest, texture_dimensions);
	
	delete [] image_dest;
	delete [] buffer;
	
	return success;
}

// Called by Rocket when a texture is required to be built from an internally-generated sequence of pixels.
bool RenderInterfaceDirectX::GenerateTexture(Rocket::Core::TextureHandle& texture_handle, const byte* source, const Rocket::Core::Vector2i& source_dimensions)
{
	// Create a Direct3DTexture9, which will be set as the texture handle. Note that we only create one surface for
	// this texture; because we're rendering in a 2D context, mip-maps are not required.
	D3D11_TEXTURE2D_DESC tex2Ddesc = {
			source_dimensions.x,//UINT Width;
			source_dimensions.y,//UINT Height;
			1,//UINT MipLevels;
			1,//UINT ArraySize;
			DXGI_FORMAT_R8G8B8A8_UNORM,//DXGI_FORMAT Format;
			1, 0,//DXGI_SAMPLE_DESC SampleDesc;
			D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,//UINT BindFlags;
			0,//UINT CPUAccessFlags; "0" or "D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE"
			0//UINT MiscFlags;
		};


	ID3D11Texture2D* texture2Dp;

	D3D11_SUBRESOURCE_DATA resourceData;
	ZeroMemory( &resourceData, sizeof( resourceData ) );
	resourceData.pSysMem = source;
	resourceData.SysMemPitch = 4*source_dimensions.x;

	d3dDevice_->CreateTexture2D(&tex2Ddesc,&resourceData,&texture2Dp);

	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = tex2Ddesc.Format;
	desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipLevels = 1;
    desc.Texture2D.MostDetailedMip = 0;

	ID3D11ShaderResourceView *tex2Dview;
	d3dDevice_->CreateShaderResourceView(texture2Dp, &desc, &tex2Dview);

	// Set the handle on the Rocket texture structure.
	texture_handle = (Rocket::Core::TextureHandle)tex2Dview;
	return true;
}

// Called by Rocket when a loaded texture is no longer required.
void RenderInterfaceDirectX::ReleaseTexture(Rocket::Core::TextureHandle texture_handle)
{
	((ID3D11ShaderResourceView*) texture_handle)->Release();
}

// Returns the native horizontal texel offset for the renderer.
float RenderInterfaceDirectX::GetHorizontalTexelOffset()
{
	return -0.5f;
}

// Returns the native vertical texel offset for the renderer.
float RenderInterfaceDirectX::GetVerticalTexelOffset()
{
	return -0.5f;
}

