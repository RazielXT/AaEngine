#include "InputHandler.h"
#include "App/TargetWindow.h"
#include "FreeCamera.h"
#include "FollowCamera.h"
#include "FirstPersonCamera.h"
#include "ApplicationCore.h"
#include <shellscalingapi.h>
#include "Editor/Editor.h"
#include "Utils/ColorUtils.h"
#include "imgui.h"
#include "Physics/Render/PhysicsRenderTask.h"
#include "Objects/Vehicle/JoltMotorcycle.h"
#include "Objects/Vehicle/ArcadeMotorcycle.h"
#include "Objects/PlayerBody.h"

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

		camera.setPosition(XMFLOAT3(0, 0, 0));
		camera.lookAt(Vector3(0, -1, 0));

		activeCameraHandler = std::make_unique<FreeCamera>(camera);
		activeCameraHandler->activate(renderPanelViewport);

		app.loadScene();
	}

	~ApplicationObject()
	{
		editorUi.deinit();
	}

	bool isPlayMode() const
	{
		return editorUi.state.interactionMode != InteractionMode::Editor;
	}

	bool isWalkingMode() const
	{
		return editorUi.state.interactionMode == InteractionMode::WalkingFirstPerson
			|| editorUi.state.interactionMode == InteractionMode::WalkingThirdPerson;
	}

	void start()
	{
		app.beginRendering([this](float timeSinceLastFrame)
			{
				handleModeChange();
				updateDebugState();

				if (motorcycle)
					motorcycle->update(timeSinceLastFrame);

				if (playerBody)
					playerBody->update(timeSinceLastFrame, camera);

				app.physicsMgr.update(timeSinceLastFrame);

				if (isPlayMode())
					InputHandler::consumeInput(*this);
				else
					editorUi.isRendererActive() ? InputHandler::consumeInput(*this) : InputHandler::clearInput();

				editorUi.updateViewportSize();

				if (editorUi.state.interactionMode == InteractionMode::Editor)
				{
					if (!editorUi.isRendererHovered())
					{
						auto* freeCamera = dynamic_cast<FreeCamera*>(activeCameraHandler.get());
						if (freeCamera)
							freeCamera->stop();
					}
				}

				// Handle visual body toggle
				if (playerBody)
				{
					if (editorUi.state.showPlayerBody && !playerBody->hasVisualBody())
						playerBody->createVisualBody(app.renderWorld, app.resources);
					else if (!editorUi.state.showPlayerBody && playerBody->hasVisualBody())
						playerBody->destroyVisualBody(app.renderWorld);
				}

				activeCameraHandler->update(timeSinceLastFrame);

				app.renderFrame(camera);

				editorUi.render(camera);

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

	void handleModeChange()
	{
		if (!editorUi.state.interactionModeChanged)
			return;

		editorUi.state.interactionModeChanged = false;

		auto spawnPos = camera.getPosition();

		if (activeCameraHandler)
			activeCameraHandler->deactivate(renderPanelViewport);

		// Cleanup previous play mode objects
		activePlayInputListener = nullptr;
		if (playerBody)
			playerBody->destroyVisualBody(app.renderWorld);
		motorcycle.reset();
		playerBody.reset();

		// Restore cursor when returning to editor
		ShowCursor(TRUE);

		switch (editorUi.state.interactionMode)
		{
		case InteractionMode::Editor:
			activeCameraHandler = std::make_unique<FreeCamera>(camera);
			activeCameraHandler->activate(renderPanelViewport);
			break;

		case InteractionMode::Motorbike:
		{
			motorcycle = std::make_unique<ArcadeMotorcycle>(app.physicsMgr);

			ResourceUploadBatch batch(app.renderSystem.core.device);
			batch.Begin();
			motorcycle->initialize(app.renderWorld, app.resources, batch);
			motorcycle->setPositionOrientation(spawnPos, camera.getOrientation());
			auto uploadResourcesFinished = batch.End(app.renderSystem.core.commandQueue);
			uploadResourcesFinished.wait();

			auto cameraHandler = std::make_unique<FollowCamera>(camera, app.physicsMgr);
			cameraHandler->setTarget(motorcycle.get());
			cameraHandler->activate(renderPanelViewport);
			activeCameraHandler = std::move(cameraHandler);
			activePlayInputListener = motorcycle.get();
			ShowCursor(FALSE);
			break;
		}

		case InteractionMode::WalkingFirstPerson:
		case InteractionMode::WalkingThirdPerson:
		{
			playerBody = std::make_unique<PlayerBody>(app.physicsMgr);
			playerBody->initialize(spawnPos);

			auto cameraHandler = std::make_unique<FirstPersonCamera>(camera, app.physicsMgr);
			cameraHandler->thirdPerson = (editorUi.state.interactionMode == InteractionMode::WalkingThirdPerson);
			cameraHandler->setTarget(playerBody.get());
			cameraHandler->activate(renderPanelViewport);
			activeCameraHandler = std::move(cameraHandler);
			activePlayInputListener = playerBody.get();
			ShowCursor(FALSE);
			break;
		}
		}
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
		if (isPlayMode())
		{
			if (key == VK_ESCAPE)
			{
				editorUi.state.interactionMode = InteractionMode::Editor;
				editorUi.state.interactionModeChanged = true;
				return true;
			}

			if (activePlayInputListener)
				activePlayInputListener->keyPressed(key);

			if (activeCameraHandler)
				activeCameraHandler->keyPressed(key);

			return true;
		}

		if (key == VK_ESCAPE)
			continueRendering = false;

		editorUi.keyPressed(key);

		if (key == 'F')
		{
			auto pos = camera.getPosition();
			auto shootVelocity = camera.getCameraDirection() * 10.f;
			shootVelocity += camera.getOrientation() * Vector3(0,3,0);

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
				scale = Vector3(getRandomFloat(0.5f, 1.0f));
			else if (type == Cylinder)
			{
				float xz = getRandomFloat(0.5f, 1.0f);
				scale = Vector3(xz, getRandomFloat(0.5f, 1.0f), xz);
			}
			else if (type == Tire)
			{
				float yz = getRandomFloat(0.5f, 1.0f);
				scale = Vector3(getRandomFloat(0.2f, 0.5f), yz, yz);

				bodyParams.restitution = 0.6f;
			}
			else
				scale = Vector3(getRandomFloat(0.5f, 1.0f), getRandomFloat(0.5f, 1.0f), getRandomFloat(0.5f, 1.0f));

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

		if (activeCameraHandler)
			return activeCameraHandler->keyPressed(key);

		return false;
	}

	bool keyReleased(int key) override
	{
		if (isPlayMode())
		{
			if (activePlayInputListener)
				activePlayInputListener->keyReleased(key);

			if (activeCameraHandler)
				activeCameraHandler->keyReleased(key);

			return true;
		}

		editorUi.keyReleased(key);

		if (activeCameraHandler)
			return activeCameraHandler->keyReleased(key);

		return false;
	}

	bool mousePressed(MouseButton button) override
	{
		if (!isPlayMode())
			editorUi.onClick(button);

		bool handled = false;
		if (activeCameraHandler)
			handled = activeCameraHandler->mousePressed(button) || handled;
		if (activePlayInputListener)
			handled = activePlayInputListener->mousePressed(button) || handled;

		return handled;
	}

	bool mouseReleased(MouseButton button) override
	{
		bool handled = false;
		if (activeCameraHandler)
			handled = activeCameraHandler->mouseReleased(button) || handled;
		if (activePlayInputListener)
			handled = activePlayInputListener->mouseReleased(button) || handled;

		return handled;
	}

	bool mouseMoved(int x, int y) override
	{
		bool handled = false;
		if (activeCameraHandler)
			handled = activeCameraHandler->mouseMoved(x, y) || handled;
		if (activePlayInputListener)
			handled = activePlayInputListener->mouseMoved(x, y) || handled;

		return handled;
	}

	bool mouseWheel(float change) override
	{
		bool handled = false;
		if (activeCameraHandler)
			handled = activeCameraHandler->mouseWheel(change) || handled;
		if (activePlayInputListener)
			handled = activePlayInputListener->mouseWheel(change) || handled;

		return handled;
	}

	std::unique_ptr<TargetWindow> window;

	ImguiPanelViewport renderPanelViewport;

	ApplicationCore app;
	Camera camera;
	std::unique_ptr<CameraHandler> activeCameraHandler;

	Editor editorUi;

	std::unique_ptr<ArcadeMotorcycle> motorcycle;
	std::unique_ptr<PlayerBody> playerBody;
	InputListener* activePlayInputListener{};

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
