#pragma once

#include "CameraHandler.h"
#include "InputHandler.h"

class FreeCamera : public CameraHandler
{
public:

	FreeCamera(Camera& camera);

	void update(float time) override;

	bool keyPressed(int key) override;
	bool keyReleased(int key) override;
	bool mouseMoved(int x, int y) override;
	bool mousePressed(MouseButton button) override;
	bool mouseReleased(MouseButton button) override;
	bool mouseWheel(float change) override;
	void stop();

	void activate(TargetViewport& viewport) override;
	void deactivate(TargetViewport& viewport) override;

private:

	float wheelDiff{};

	float mouseX{}, mouseY{};
	bool w{}, s{}, a{}, d{}, turbo{}, slow{}, move{};
	bool strafe{};
};
