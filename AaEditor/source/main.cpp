#include "InputHandler.h"
#include "App/TargetWindow.h"
#include "FreeCamera.h"
#include "ApplicationCore.h"
#include <shellscalingapi.h>
#include "Editor/Editor.h"
#include "Utils/ColorUtils.h"
#include "imgui.h"
#include "Physics/Render/PhysicsRenderTask.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class ApplicationObject : public InputListener, public ViewportListener
{
public:

	ApplicationObject(HINSTANCE hInstance) : app(renderPanelViewport), editorUi(app, renderPanelViewport)
	{
		window = std::make_unique<TargetWindow>("AaEngine", hInstance, 1800, 1080, false);

		InputHandler::registerWindow(window->getHwnd());

		editorUi.initializeUi(*window);

		window->eventHandler = [](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) -> bool
			{
				ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam);

				return InputHandler::handleMessage(message, wParam, lParam);
			};

		window->listeners.push_back(this);

		ApplicationCore::InitParams params;
		params.compositor.renderToBackbuffer = false;
		app.initialize(*window, params);

		editorUi.initializeRenderer(app.compositor->getColorSpace().outputFormat);

		freeCamera.bind(renderPanelViewport);
		freeCamera.camera.setPosition(XMFLOAT3(0, 0, 0));

		app.loadScene();
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

				editorUi.isRendererActive() ? InputHandler::consumeInput(*this) : InputHandler::clearInput();
				editorUi.updateViewportSize();

				updateDebugState();

				if (!editorUi.isRendererHovered())
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
		if (editorUi.state.reloadShaders)
		{
			app.resources.materials.reloadChangedShaders();
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
		if (editorUi.state.wireframeChange)
		{
			app.compositor->setDefine("WIREFRAME", editorUi.state.wireframe);
			editorUi.resetViewportOutput();
		}
		if (editorUi.state.wireframePhysicsChange)
		{
			app.compositor->setDefine("RENDER_PHYSICS", true);
			((PhysicsRenderTask*)app.compositor->getTask("RenderPhysics"))->setMode(PhysicsRenderTask::Mode(*editorUi.state.wireframePhysicsChange));
			editorUi.resetViewportOutput();

			editorUi.state.wireframePhysicsChange.reset();;
		}
	}

	bool keyPressed(int key) override
	{
		if (key == VK_ESCAPE)
			continueRendering = false;

		editorUi.keyPressed(key);

		if (key == 'F')
		{
			auto pos = freeCamera.camera.getPosition();
			auto shootVelocity = freeCamera.camera.getCameraDirection() * 30.f;

			enum Type { Sphere, Box, Cylinder, Tire } type{};
			const char* meshes[] = { "sphere.mesh", "centeredBox.mesh", "cylinder.mesh", "wheel.mesh" };

			type = (Type) int(getRandomFloat01() * (float)std::size(meshes));
			const char* meshName = meshes[type];
			auto model = app.resources.models.getCoreModel(meshName);
			auto material = app.resources.materials.getMaterial(type == Tire ? "Tire" : "General");

			auto ent = app.renderWorld.createEntity();
			ent->setPosition(pos);
			ent->setOrientation(getRandomQuaternion());
			ent->geometry.fromModel(*model);
			ent->material = material;

			if (type != Tire)
				ent->Material().setParam("MaterialColor", getRandomSrgbColor());

			// --- Physics body ---
			JPH::BodyID id{};
			BodyParams bodyParams;
			bodyParams.mass = getRandomFloat(1.0f, 3.0f);
			bodyParams.velocity = shootVelocity;

			Vector3 scale;

			if (type == Sphere)
				scale = Vector3(getRandomFloat(1.0f, 2.0f));
			else if (type == Cylinder)
			{
				float xz = getRandomFloat(1.0f, 3.0f);
				scale = Vector3(xz, getRandomFloat(1.0f, 3.0f), xz);
			}
			else if (type == Tire)
			{
				float xz = getRandomFloat(2.0f, 4.0f);
				scale = Vector3(xz, getRandomFloat(1.0f, 2.0f), xz);

				bodyParams.restitution = 0.6f;
			}
			else
				scale = Vector3(getRandomFloat(1.0f, 3.0f), getRandomFloat(1.0f, 3.0f), getRandomFloat(1.0f, 3.0f));

			ent->setScale(scale);

			if (type == Sphere)
			{
				float radius = model->bbox.Extents.x * scale.x;
				id = app.physicsMgr.createSphere(radius, pos, bodyParams);
			}
			else if (type == Cylinder)
			{
				id = app.physicsMgr.createCylinder(1.0f, 1.0f, ent->getTransformation(), {}, bodyParams);
			}
			else if (type == Tire)
			{
				id = app.physicsMgr.createConvexBody(model->positions, ent->getTransformation(), bodyParams);
			}
			else //box
			{
				id = app.physicsMgr.createBox({ 0.5f, 0.5f, 0.5f }, ent->getTransformation(), {}, bodyParams);
			}

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
