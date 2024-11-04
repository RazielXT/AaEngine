#pragma once

#include <vector>

#include "AaCamera.h"
#include "InputHandler.h"

class FreeCamera
{
public:

	FreeCamera();
	~FreeCamera();

	void update(float time);
	void reload(float aspectRatio);

	bool keyPressed(int key);
	bool keyReleased(int key);
	bool mouseMoved(int x, int y);
	bool mousePressed(MouseButton button);
	bool mouseReleased(MouseButton button);
	bool mouseWheel(float change);

	AaCamera camera;

private:

	float wheelDiff{};

	float mouseX{}, mouseY{};
	bool w{}, s{}, a{}, d{}, turbo{}, move{};
	bool strafe{};
};
