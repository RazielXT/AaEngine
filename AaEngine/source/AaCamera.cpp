#pragma once

#include "AaCamera.h"
#include "AaMath.h"

const auto FrontDirection = XMFLOAT3(0, 0, 1);
const auto UpVector = XMFLOAT3(0, 1, 0);

AaCamera::AaCamera()
{
	view_m = XMMatrixIdentity();

	position = XMFLOAT3(0, 0, 0);
	direction = FrontDirection;
	upVector = UpVector;
}

AaCamera::~AaCamera()
{
}

void AaCamera::setOrthographicCamera(float width, float height, float nearZ, float farZ)
{
	projection_m = XMMatrixOrthographicLH(width, height, nearZ, farZ);
	this->width = width;
	this->height = height;
	this->nearZ = nearZ;
	this->farZ = farZ;
	dirty = true;
	orthographic = true;
}

void AaCamera::setPerspectiveCamera(float fov, float aspectRatio, float nearZ, float farZ)
{
	projection_m = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspectRatio, nearZ, farZ);
	dirty = true;
	orthographic = false;
}

bool AaCamera::isOrthographic() const
{
	return orthographic;
}

const XMMATRIX& AaCamera::getProjectionMatrix() const
{
	return projection_m;
}

const XMMATRIX& AaCamera::getViewMatrix() const
{
	return view_m;
}

const XMMATRIX& AaCamera::getViewProjectionMatrix() const
{
	return view_projection_m;
}

void AaCamera::updateMatrix()
{
	if (dirty)
	{
		view_m = XMMatrixMultiply(XMMatrixLookToLH(XMLoadFloat3(&position), XMLoadFloat3(&direction), XMLoadFloat3(&upVector)), XMMatrixRotationY(yaw_));
		view_m = XMMatrixMultiply(view_m, XMMatrixRotationX(pitch_));
		view_m = XMMatrixMultiply(view_m, XMMatrixRotationZ(roll_));

// 		auto quatMatrix = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));
// 		view_m = XMMatrixMultiply(XMMatrixLookToLH(XMLoadFloat3(&position), XMLoadFloat3(&direction), XMLoadFloat3(&upVector)), quatMatrix);

		view_projection_m = XMMatrixMultiply(view_m, projection_m);
		dirty = false;
	}
}

void AaCamera::move(Vector3 p)
{
	position += p;

	dirty = true;
}

void AaCamera::setPosition(Vector3 p)
{
	position = p;
	dirty = true;
}

void AaCamera::setInCameraRotation(Vector3& direction) const
{
	direction.x *= -1;

	XMVECTOR dir = XMLoadFloat3(&direction);
	dir = XMVector3Transform(dir, XMMatrixRotationX(pitch_));
	dir = XMVector3Transform(dir, XMMatrixRotationY(yaw_));
	XMStoreFloat3(&direction, dir);

	direction.x *= -1;
	direction.y *= -1;
}

void AaCamera::yaw(float Yaw)
{
	Vector3 yAxis = rotation * Vector3::UnitY;

	rotate(yAxis, Yaw);
	yaw_ += Yaw;
	dirty = true;
}

void AaCamera::pitch(float Pitch)
{
	rotate(Vector3::UnitX, Pitch);
	pitch_ += Pitch;
	dirty = true;
}

void AaCamera::roll(float Roll)
{
	rotate(Vector3::UnitZ, Roll);
	roll_ += Roll;
	dirty = true;
}

void AaCamera::rotate(Vector3 axis, float angle)
{
// 	rotation *= Quaternion::CreateFromAxisAngle(axis, angle);
// 	pitch_ = angle * axis.x;
// 	yaw_ = angle * axis.y;
// 	roll_ = angle * axis.z;
// 	dirty = true;
}

void AaCamera::resetRotation()
{
	//rotation = Quaternion::Identity;
	yaw_ = pitch_ = roll_ = 0;
	dirty = true;
}

void AaCamera::setDirection(Vector3 Direction)
{
	resetRotation();
	//rotation = Quaternion::FromToRotation(FrontDirection, Direction);

	if (Direction == UpVector)
	{
		direction = FrontDirection;
		pitch_ = XM_PIDIV2;
	}
	else if (Direction == -1 * UpVector)
	{
		direction = FrontDirection;
		pitch_ = -XM_PIDIV2;
	}
	else
		direction = Direction;

	dirty = true;
}

void AaCamera::lookAt(Vector3 target)
{
	setDirection(target - position);
}

Vector3 AaCamera::getPosition() const
{
	return position;
}

DirectX::BoundingFrustum AaCamera::prepareFrustum() const
{
	DirectX::BoundingFrustum frustum;
	DirectX::BoundingFrustum::CreateFromMatrix(frustum, projection_m);
	frustum.Transform(frustum, DirectX::XMMatrixInverse(nullptr, view_m));

	return frustum;
}

DirectX::BoundingOrientedBox AaCamera::prepareOrientedBox() const
{
	DirectX::BoundingOrientedBox orientedBox;
	orientedBox.Extents = { width / 2.0f, height / 2.0f, (farZ - nearZ) / 2.0f };
	orientedBox.Transform(orientedBox, DirectX::XMMatrixInverse(nullptr, view_m));

	return orientedBox;
}
