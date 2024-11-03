#pragma once

#include "FreeCamera.h"

FreeCamera::FreeCamera()
{
}

FreeCamera::~FreeCamera()
{
}

extern XMFLOAT3 currentCamPos;
void FreeCamera::update(float time)
{
	currentCamPos = camera.getPosition();

	if (!move)
		return;

	XMFLOAT3 dir(0, 0, 0);
	float speed = 30 * time;
	if (turbo) speed *= 5;

	if (w || s || a || d)
	{
		if (!a && !d)
		{
			if (w) { dir.z = speed; }
			if (s) { dir.z = -speed; }
		}
		else
		{
			if (d)
			{
				if (!w && !s)
				{
					dir.x = speed;
				}
				else if (w)
				{
					dir.x = 0.7f * speed;
					dir.z = 0.7f * speed;
				}
				else if (s)
				{
					dir.x = 0.7f * speed; dir.z = -0.7f * speed;
				}
			}

			if (a)
			{
				if (!w && !s)
				{
					dir.x = -speed;
				}
				else if (w)
				{
					dir.x = -0.7f * speed;
					dir.z = 0.7f * speed;
				}
				else if (s)
				{
					dir.x = -0.7f * speed;
					dir.z = -0.7f * speed;
				}
			}
		}

		camera.setInCameraRotation(dir);

		camera.move(dir);
	}
}

void FreeCamera::reload(float aspectRatio)
{
	camera.setPerspectiveCamera(70, aspectRatio, 0.3, 10000);
}

bool FreeCamera::keyPressed(int key)
{
	if (!move)
		return false;

	switch (key)
	{
	case 'W':
		w = true;
		break;
	case 'S':
		s = true;
		break;
	case 'A':
		a = true;
		break;
	case 'D':
		d = true;
		break;
	case VK_SHIFT:
		turbo = true;
		break;
	}

	return true;
}

bool FreeCamera::keyReleased(int key)
{
	if (!move)
		return false;

	switch (key)
	{
	case 'W':
		w = false;
		break;
	case 'S':
		s = false;
		break;
	case 'A':
		a = false;
		break;
	case 'D':
		d = false;
		break;
	case VK_SHIFT:
		turbo = false;
		break;
	}

	return true;
}

bool FreeCamera::mouseMoved(int x, int y)
{
	if (strafe)
	{
		XMFLOAT3 dir(x, y, 0);
		camera.setInCameraRotation(dir);
		camera.move(dir);
	}
	else if (move)
	{
		camera.yaw(-0.01f * x);
		camera.pitch(-0.01f * y);
	}

	return true;
}

bool FreeCamera::mousePressed(MouseButton button)
{
	if (button == MouseButton::Right)
	{
		move = true;
	}
	if (button == MouseButton::Left)
	{
		strafe = true;
	}

	return true;
}

bool FreeCamera::mouseReleased(MouseButton button)
{
	if (button == MouseButton::Right)
	{
		move = false;
		w = s = a = d = turbo = move = false;
	}
	if (button == MouseButton::Left)
	{
		strafe = false;
	}

	return true;
}
