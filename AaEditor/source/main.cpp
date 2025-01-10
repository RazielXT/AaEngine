#include "InputHandler.h"
#include "TargetWindow.h"
#include "FreeCamera.h"
#include "ApplicationCore.h"
#include "DebugWindow.h"
#include <shellscalingapi.h>
#include "Editor.h"
#include "imgui.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class ApplicationObject : public InputListener, public ViewportListener
{
public:

	ApplicationObject(HINSTANCE hInstance) : app(renderPanelViewport), editorUi(app, renderPanelViewport)
	{
		window = std::make_unique<TargetWindow>("AaEngine", hInstance, 1800, 1080, false);

		InputHandler::registerWindow(window->getHwnd());

		editorUi.initialize(*window);

		window->eventHandler = [](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) -> bool
			{
				ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam);

				return InputHandler::handleMessage(message, wParam, lParam);
			};

		window->listeners.push_back(this);

		ApplicationCore::InitParams params;
		params.defaultCompositorParams.renderToBackbuffer = false;
		app.initialize(*window, params);

		freeCamera.bind(renderPanelViewport);
		freeCamera.camera.setPosition(XMFLOAT3(0, 0, -14.f));

		app.loadScene("test");
	}

	~ApplicationObject()
	{
		editorUi.deinit();
	}

	void start()
	{
		app.beginRendering([this](float timeSinceLastFrame)
			{
				app.physicsMgr.update(timeSinceLastFrame);

				editorUi.rendererActive ? InputHandler::consumeInput(*this) : InputHandler::clearInput();
				editorUi.updateViewportSize();

				updateDebugState();

				if (!editorUi.rendererHovered)
					freeCamera.stop();
				else
					freeCamera.update(timeSinceLastFrame);

				app.renderFrame(freeCamera.camera);

				editorUi.render(freeCamera.camera);

				auto presentResult = app.present();

				if (editorUi.state.limitFrameRate)
				{
					UINT sleepTime = 10;

					//locked
					if (presentResult == DXGI_STATUS_OCCLUDED)
						sleepTime += 1000;
					//minimized
					if (IsIconic(window->getHwnd()))
						sleepTime += 100;

					Sleep(sleepTime);
				}

				return continueRendering;
			});
	}

	void updateDebugState()
	{
		if (editorUi.state.changeScene)
		{
			app.loadScene(editorUi.state.changeScene);
			editorUi.state.changeScene = {};
			editorUi.reset();
		}
		if (editorUi.state.reloadShaders)
		{
			app.resources.materials.ReloadShaders();
			editorUi.state.reloadShaders = false;
		}
		if (editorUi.state.DlssMode != (int)app.renderSystem.upscale.dlss.selectedMode())
		{
			if (!app.renderSystem.upscale.dlss.selectMode((UpscaleMode)editorUi.state.DlssMode))
				editorUi.state.DlssMode = (int)UpscaleMode::Off;
			else
				app.compositor->reloadPasses();

			editorUi.resetViewportOutput();
		}
		if (editorUi.state.FsrMode != (int)app.renderSystem.upscale.fsr.selectedMode())
		{
			app.renderSystem.upscale.fsr.selectMode((UpscaleMode)editorUi.state.FsrMode);
			app.compositor->reloadPasses();

			editorUi.resetViewportOutput();
		}
	}

	bool keyPressed(int key) override
	{
		if (key == VK_ESCAPE)
			continueRendering = false;

		editorUi.keyPressed(key);

		if (key == 'F')
		{
			auto model = app.resources.models.getLoadedModel("sphere.mesh", ResourceGroup::Core);
			auto material = app.resources.materials.getMaterial("WhiteVCT");

			auto pos = freeCamera.camera.getPosition();

			static int i = 0;
			auto ent = app.sceneMgr.createEntity("shoot" + std::to_string(i++));
			ent->setPosition(pos);
			ent->geometry.fromModel(*model);
			ent->material = material;

			auto id = app.physicsMgr.createDynamicSphere(model->bbox.Extents.x, pos, freeCamera.camera.getCameraDirection() * 30);
			app.physicsMgr.dynamicBodies.push_back({ ent, id });
		}
		return freeCamera.keyPressed(key);
	}

	bool keyReleased(int key) override
	{
		editorUi.keyReleased(key);

		return freeCamera.keyReleased(key);
	}

	bool mousePressed(MouseButton button) override
	{
		editorUi.onClick(button);

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

	std::unique_ptr<TargetWindow> window;

	ImguiPanelViewport renderPanelViewport;

	ApplicationCore app;
	FreeCamera freeCamera;
	Editor editorUi;

	bool continueRendering = true;

	void onViewportResize(UINT, UINT) override
	{
	}

	void onScreenResize(UINT w, UINT h) override
	{
		for (auto l : renderPanelViewport.listeners)
		{
			l->onScreenResize(w, h);
		}
	}
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow)
{
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	ApplicationObject app(hInstance);
	app.start();

	return 0;
}
