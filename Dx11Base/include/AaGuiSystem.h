#ifndef _AA_GUI_MANAGER_
#define _AA_GUI_MANAGER_


#include <Rocket/Core.h>
#include <Rocket/Controls.h>
#include <Rocket/Debugger.h>
#include <Rocket/Core/EventListener.h>
#include "GuiRenderInterfaceDirectX.h"
#include "GuiSystemInterface.h"
#include "OIS.h"

class AaGuiSystem
{
public:

	AaGuiSystem();
	~AaGuiSystem();
	void init(ID3D11Device* d3dDevice,ID3D11DeviceContext* d3dContext, int width, int heigth);
	void loadGui(Rocket::Core::EventListener* gui);
	void update(float deltaTime);
	void render();

	void keyPressed( const OIS::KeyEvent &arg );
	void keyReleased( const OIS::KeyEvent &arg );
	void mouseMoved( const OIS::MouseEvent &arg );
	void mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id );
	void mouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id );

	Rocket::Core::Context* getContext() { return context; }

private:

	void AaGuiSystem::prepareGuiDrawingStates();

	float blendFactor[4];
	ID3D11Device* d3dDevice_;
	ID3D11DeviceContext* d3dContext_;
	Rocket::Core::Context* context;
	GuiSystemInterface* system_interface;
	RenderInterfaceDirectX* directx_renderer;
	ID3D11RasterizerState* guiRenderState;
	ID3D11BlendState* alphaBlendState_;
	ID3D11DepthStencilState* dsState;
	float elapsedTime;
};

#endif