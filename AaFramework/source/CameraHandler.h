#pragma once

#include "Scene/Camera.h"
#include "App/TargetWindow.h"
#include "InputHandler.h"

class PhysicsManager;

class CameraHandler : public ViewportListener, public InputListener
{
public:

	CameraHandler(Camera& camera);
	virtual ~CameraHandler() = default;

	virtual void update(float deltaTime) = 0;

	virtual void activate() {}
	virtual void deactivate() {}

	bool keyPressed(int) override { return false; }
	bool keyReleased(int) override { return false; }
	bool mousePressed(MouseButton) override { return false; }
	bool mouseReleased(MouseButton) override { return false; }
	bool mouseMoved(int, int) override { return false; }
	bool mouseWheel(float) override { return false; }

	void bind(TargetViewport&);

	Camera& camera;

protected:

	Vector3 limitCameraPosition(PhysicsManager& physics, Vector3 pos, Vector3 target);

	void onViewportResize(UINT width, UINT height) override;
	void onScreenResize(UINT, UINT) override {}

	TargetViewport* target{};
};
