#include "AaGuiSystem.h"
#include "GlobalDefinitions.h"

AaGuiSystem::AaGuiSystem()
{
}

AaGuiSystem::~AaGuiSystem()
{
	context->RemoveReference();

	system_interface->Release();

	directx_renderer->Release();

	Rocket::Core::Shutdown();

	delete directx_renderer;

	guiRenderState->Release();
	alphaBlendState_->Release();
	dsState->Release();
}

void AaGuiSystem::init(ID3D11Device* d3dDevice,ID3D11DeviceContext* d3dContext, int width, int heigth)
{
	d3dDevice_ = d3dDevice;
	d3dContext_ = d3dContext;

	directx_renderer = new RenderInterfaceDirectX(d3dDevice_, d3dContext_, width, heigth);
	Rocket::Core::SetRenderInterface(directx_renderer);

	prepareGuiDrawingStates();

	system_interface = new GuiSystemInterface(&elapsedTime);
	Rocket::Core::SetSystemInterface(system_interface);

	Rocket::Core::Initialise();
	Rocket::Controls::Initialise();

	// Create the main Rocket context and set it on the shell's input layer.
	context = Rocket::Core::CreateContext("main", Rocket::Core::Vector2i(width, heigth));
	if (context == NULL)
	{
		Rocket::Core::Shutdown();
		return;
	}

	//Rocket::Debugger::Initialise(context);
	
	//Input::SetContext(context);
	//context->UnloadAllDocuments();

	Rocket::Core::String font_names[4];
	font_names[0] = Rocket::Core::String(GUI_DIRECTORY) + "Delicious-Roman.otf";
	font_names[1] = Rocket::Core::String(GUI_DIRECTORY)+ "Delicious-Italic.otf";
	font_names[2] = Rocket::Core::String(GUI_DIRECTORY) + "Delicious-Bold.otf";
	font_names[3] = Rocket::Core::String(GUI_DIRECTORY) + "Delicious-BoldItalic.otf";

	for (int i = 0; i < sizeof(font_names) / sizeof(Rocket::Core::String); i++)
	{
		Rocket::Core::FontDatabase::LoadFontFace(font_names[i]);
	}

	Rocket::Core::ElementDocument* cursor = context->LoadMouseCursor( Rocket::Core::String(GUI_DIRECTORY) + "cursor.rml");
	if (cursor)
		cursor->RemoveReference();

}

void AaGuiSystem::prepareGuiDrawingStates()
{
	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_FRONT;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;
	rasterDesc.ScissorEnable=false;
	d3dDevice_->CreateRasterizerState(&rasterDesc,&guiRenderState);

	D3D11_BLEND_DESC blendStateDescription;
	ZeroMemory( &blendStateDescription, sizeof( blendStateDescription ) );
	// Create an alpha enabled blend state description.
	blendStateDescription.RenderTarget[0].BlendEnable = TRUE;
	blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendStateDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].RenderTargetWriteMask = 0x0f;

	blendFactor[0] = blendFactor[1] = blendFactor[2] = blendFactor[3] = 0;
	d3dDevice_->CreateBlendState( &blendStateDescription, &alphaBlendState_ );

	D3D11_DEPTH_STENCIL_DESC depthDisabledStencilDesc;
	depthDisabledStencilDesc.DepthEnable = false;
	depthDisabledStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthDisabledStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthDisabledStencilDesc.StencilEnable = false;

	d3dDevice_->CreateDepthStencilState(&depthDisabledStencilDesc, &dsState );

}

void AaGuiSystem::update(float deltaTime)
{
	elapsedTime += deltaTime;
	context->Update();
}

void AaGuiSystem::render()
{
	d3dContext_->RSSetState(guiRenderState);
	d3dContext_->OMSetBlendState( alphaBlendState_, blendFactor, 0xFFFFFFFF );
	d3dContext_->OMSetDepthStencilState(dsState,1);

	context->Render();
}

void AaGuiSystem::keyPressed( const OIS::KeyEvent &arg )
{
	if(arg.key==OIS::KC_BACK)
	context->ProcessKeyDown(Rocket::Core::Input::KI_BACK, 0);
	else
	if(arg.key<83 && arg.key>70)
	{
		if(arg.key==OIS::KC_NUMPAD0)
		context->ProcessTextInput((Rocket::Core::word) '0');
		else
		if(arg.key==OIS::KC_NUMPAD1)
		context->ProcessTextInput((Rocket::Core::word) '1');
		else
		if(arg.key==OIS::KC_NUMPAD2)
		context->ProcessTextInput((Rocket::Core::word) '2');
		else
		if(arg.key==OIS::KC_NUMPAD3)
		context->ProcessTextInput((Rocket::Core::word) '3');
		else
		if(arg.key==OIS::KC_NUMPAD4)
		context->ProcessTextInput((Rocket::Core::word) '4');
		else
		if(arg.key==OIS::KC_NUMPAD5)
		context->ProcessTextInput((Rocket::Core::word) '5');
		else
		if(arg.key==OIS::KC_NUMPAD6)
		context->ProcessTextInput((Rocket::Core::word) '6');
		else
		if(arg.key==OIS::KC_NUMPAD7)
		context->ProcessTextInput((Rocket::Core::word) '7');
		else
		if(arg.key==OIS::KC_NUMPAD8)
		context->ProcessTextInput((Rocket::Core::word) '8');
		else
		if(arg.key==OIS::KC_NUMPAD9)
		context->ProcessTextInput((Rocket::Core::word) '9');
	}
	else
	context->ProcessTextInput((Rocket::Core::word) arg.text);
}

void AaGuiSystem::keyReleased( const OIS::KeyEvent &arg )
{
	if(arg.key==OIS::KC_BACK)
	context->ProcessKeyUp(Rocket::Core::Input::KI_BACK, 0);
}

void AaGuiSystem::mouseMoved( const OIS::MouseEvent &arg )
{
	context->ProcessMouseMove(arg.state.X.abs,arg.state.Y.abs,0);
}

void AaGuiSystem::mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id )
{
	switch (id)
	{
		case OIS::MB_Left:
			context->ProcessMouseButtonDown(0, 0);
			break;
		case OIS::MB_Right:
			context->ProcessMouseButtonDown(1, 0);
			break;
		case OIS::MB_Middle:
			context->ProcessMouseButtonDown(2, 0);
			break;
	}
}
void AaGuiSystem::mouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id )
{
	switch (id)
	{
		case OIS::MB_Left:
			context->ProcessMouseButtonUp(0, 0);
			break;
		case OIS::MB_Right:
			context->ProcessMouseButtonUp(1, 0);
			break;
		case OIS::MB_Middle:
			context->ProcessMouseButtonUp(2, 0);
			break;
	}
}

void AaGuiSystem::loadGui(Rocket::Core::EventListener* gui)
{
	//context->AddEventListener("iclicked",gui);
}

