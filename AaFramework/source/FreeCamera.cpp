#pragma once

#include "FreeCamera.h"

FreeCamera::FreeCamera()
{
}

FreeCamera::~FreeCamera()
{
}

void FreeCamera::update(float time)
{
	Vector3 dir{};

	if (w)
		dir.z += 1;
	else if (s)
		dir.z -= 1;

	if (a)
		dir.x -= 1;
	else if (d)
		dir.x += 1;

	dir.z += wheelDiff * 5;
	wheelDiff = 0;

	if (dir != Vector3::Zero)
	{
		dir.Normalize();

		float speed = 30 * time;
		if (turbo)
			speed *= 5;

		dir *= speed;

		camera.setInCameraRotation(dir);
		camera.move(dir);
	}
}

void FreeCamera::bind(TargetViewport& viewport)
{
	target = &viewport;
	viewport.listeners.push_back(this);

	onViewportResize(target->getWidth(), target->getHeight());
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
		Vector3 dir(float(x), float(y), 0);
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
	if (button == MouseButton::Middle)
	{
		strafe = true;
	}

	return true;
}

bool FreeCamera::mouseReleased(MouseButton button)
{
	if (button == MouseButton::Right)
	{
		stop();
	}
	if (button == MouseButton::Middle)
	{
		strafe = false;
	}

	return true;
}

bool FreeCamera::mouseWheel(float change)
{
	wheelDiff += change;
	return true;
}

void FreeCamera::stop()
{
	w = s = a = d = turbo = move = false;
}

void FreeCamera::onViewportResize(UINT width, UINT height)
{
	camera.setPerspectiveCamera(70, width / (float)height, 1, 10000);
}

void FreeCamera::onScreenResize(UINT, UINT)
{
}
