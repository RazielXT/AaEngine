#pragma once

#include "Camera.h"
#include "InputHandler.h"
#include "TargetWindow.h"

class FreeCamera : public ViewportListener
{
public:

	FreeCamera();
	~FreeCamera();

	void update(float time);
	void bind(TargetViewport&);

	bool keyPressed(int key);
	bool keyReleased(int key);
	bool mouseMoved(int x, int y);
	bool mousePressed(MouseButton button);
	bool mouseReleased(MouseButton button);
	bool mouseWheel(float change);
	void stop();

	Camera camera;

private:

	void onViewportResize(UINT, UINT) override;
	void onScreenResize(UINT, UINT) override;

	float wheelDiff{};

	float mouseX{}, mouseY{};
	bool w{}, s{}, a{}, d{}, turbo{}, move{};
	bool strafe{};

	TargetViewport* target{};
};
