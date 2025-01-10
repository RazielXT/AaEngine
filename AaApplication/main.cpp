#include "InputHandler.h"
#include "TargetWindow.h"
#include "FreeCamera.h"
#include "ApplicationCore.h"
#include "DebugWindow.h"
#include <shellscalingapi.h>

class ApplicationObject : public InputListener
{
public:

	ApplicationObject(TargetWindow& window) : app(window), debugWindow(app.renderSystem)
	{
		app.initialize(window);

		freeCamera.bind(window);
		freeCamera.camera.setPosition(XMFLOAT3(0, 0, -14.f));

		debugWindow.state.DlssMode = (int)app.renderSystem.upscale.dlss.selectedMode();
		debugWindow.state.FsrMode = (int)app.renderSystem.upscale.fsr.selectedMode();

		app.loadScene("test");
	}

	~ApplicationObject()
	{
	}

	void start()
	{
		app.beginRendering([this](float timeSinceLastFrame)
			{
				app.physicsMgr.update(timeSinceLastFrame);

				InputHandler::consumeInput(*this);

				freeCamera.update(timeSinceLastFrame);

				if (debugWindow.state.changeScene)
				{
					app.loadScene(debugWindow.state.changeScene);
					debugWindow.state.changeScene = {};
				}
				if (debugWindow.state.reloadShaders)
				{
					app.resources.materials.ReloadShaders();
					debugWindow.state.reloadShaders = false;
				}
				if (debugWindow.state.DlssMode != (int)app.renderSystem.upscale.dlss.selectedMode())
				{
					if (!app.renderSystem.upscale.dlss.selectMode((UpscaleMode)debugWindow.state.DlssMode))
						debugWindow.state.DlssMode = (int)UpscaleMode::Off;
					else
						app.compositor->reloadPasses();
				}
				if (debugWindow.state.FsrMode != (int)app.renderSystem.upscale.fsr.selectedMode())
				{
					app.renderSystem.upscale.fsr.selectMode((UpscaleMode)debugWindow.state.FsrMode);
					app.compositor->reloadPasses();
				}
				if (auto ent = app.sceneMgr.getEntity("Torus001"))
				{
					ent->roll(timeSinceLastFrame);
					ent->yaw(timeSinceLastFrame / 2.f);
					ent->pitch(timeSinceLastFrame / 3.f);
				}
				if (auto ent = app.sceneMgr.getEntity("Suzanne"))
				{
					ent->yaw(timeSinceLastFrame);
					ent->setPosition({ cos(app.params.time / 2.f) * 5, ent->getPosition().y, ent->getPosition().z });
				}
				if (auto ent = app.sceneMgr.getEntity("MovingCube"))
				{
					ent->setPosition({ ent->getPosition().x, ent->getPosition().y, -130.f - cos(app.params.time * 3) * 20 });
				}

				app.renderFrame(freeCamera.camera);
				app.present();

				Sleep(10);

				return continueRendering;
			});
	}

	bool keyPressed(int key) override
	{
		if (key == VK_ESCAPE)
			continueRendering = false;

		return freeCamera.keyPressed(key);
	}

	bool keyReleased(int key) override
	{
		return freeCamera.keyReleased(key);
	}

	bool mousePressed(MouseButton button) override
	{
		return freeCamera.mousePressed(button);
	}

	bool mouseReleased(MouseButton button) override
	{
		return freeCamera.mouseReleased(button);
	}

	bool mouseMoved(int x, int y) override
	{
		return freeCamera.mouseMoved(x, y);
	}

	bool mouseWheel(float change) override
	{
		return freeCamera.mouseWheel(change);
	}

	FreeCamera freeCamera;
	ApplicationCore app;
	imgui::DebugWindow debugWindow;

	bool continueRendering = true;
};

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow)
{
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	TargetWindow window("AaEngine", hInstance, 1280, 800, false);

	window.eventHandler = [](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) -> bool
		{
			ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam);

			return InputHandler::handleMessage(message, wParam, lParam);
		};

	InputHandler::registerWindow(window.getHwnd());

	ApplicationObject app(window);
	app.start();
	
	return 0;
}
