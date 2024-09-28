#pragma once

#include "AaCamera.h"
#include "AaMath.h"

AaCamera::AaCamera()
{
	view_m = XMMatrixIdentity();

	position = XMFLOAT3(0, 0, 0);
	direction = XMFLOAT3(0, 0, 1);
	upVector = XMFLOAT3(0, 1, 0);
}

AaCamera::~AaCamera()
{
}

void AaCamera::setOrthograhicCamera(float width, float height, float nearZ, float farZ)
{
	projection_m = XMMatrixOrthographicLH(width, height, nearZ, farZ);
	this->width = width;
	this->height = height;
	this->nearZ = nearZ;
	this->farZ = farZ;
	dirty = true;
}

void AaCamera::setPerspectiveCamera(float fov, float aspectRatio, float nearZ, float farZ)
{
	projection_m = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspectRatio, nearZ, farZ);
	dirty = true;
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
		view_projection_m = XMMatrixMultiply(view_m, projection_m);
		dirty = false;
	}
}

void AaCamera::move(XMFLOAT3 position)
{
	this->position.x += position.x;
	this->position.y += position.y;
	this->position.z += position.z;
}

void AaCamera::setPosition(XMFLOAT3 position)
{
	this->position = position;
	dirty = true;
}

void AaCamera::setInCameraRotation(XMFLOAT3* direction)
{
	direction->x *= -1;

	XMVECTOR dir = XMLoadFloat3(direction);
	dir = XMVector3Transform(dir, XMMatrixRotationX(pitch_));
	dir = XMVector3Transform(dir, XMMatrixRotationY(yaw_));
	XMStoreFloat3(direction, dir);

	direction->x *= -1;
	direction->y *= -1;
}

void AaCamera::yaw(float Yaw)
{
	yaw_ += Yaw;
	dirty = true;
}

void AaCamera::pitch(float Pitch)
{
	pitch_ += Pitch;
	dirty = true;
}

void AaCamera::roll(float Roll)
{
	roll_ += Roll;
	dirty = true;
}

void AaCamera::resetRotation()
{
	yaw_ = pitch_ = roll_ = 0;
	dirty = true;
}

void AaCamera::lookTo(XMFLOAT3 Direction)
{
	direction = Direction;
	yaw_ = pitch_ = 0;
	dirty = true;
}

void AaCamera::lookAt(XMFLOAT3 target)
{
	direction.x = target.x - position.x;
	direction.y = target.y - position.y;
	direction.z = target.z - position.z;
	yaw_ = pitch_ = 0;
	dirty = true;
}

XMFLOAT3 AaCamera::getPosition() const
{
	return position;
}

DirectX::BoundingFrustum AaCamera::prepareFrustum()
{
	DirectX::BoundingFrustum frustum;
	DirectX::BoundingFrustum::CreateFromMatrix(frustum, projection_m);

	// Transform the frustum to camera space
	frustum.Transform(frustum, DirectX::XMMatrixInverse(nullptr, view_m));

	return frustum;
}

DirectX::BoundingOrientedBox AaCamera::prepareOrientedBox()
{
	DirectX::BoundingOrientedBox orientedBox;
	orientedBox.Extents = { width / 2.0f, height / 2.0f, (farZ - nearZ) / 2.0f };
	orientedBox.Transform(orientedBox, DirectX::XMMatrixInverse(nullptr, view_m));
// 
// 	{
// 		XMVECTOR axis = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // Common axis (0, 0, 1)
// 		XMVECTOR directionV = XMLoadFloat3(&direction);
// 
// 		// Calculate the cross product
// 		XMVECTOR crossProduct = DirectX::XMVector3Cross(axis, directionV);
// 
// 		// Calculate the dot product
// 		float dotProduct = DirectX::XMVector3Dot(axis, directionV).m128_f32[0];
// 
// 		// Calculate the angle
// 		float angle = acosf(dotProduct);
// 
// 		// Create the quaternion
// 		XMVECTOR quaternion = DirectX::XMQuaternionRotationAxis(crossProduct, angle);
// 
// 		XMStoreFloat4(&orientedBox.Orientation, quaternion);
// 	}
// 	Quaternion dir = orientedBox.Orientation;
// 	Vector3 dirBox = { 0,-1, 0 };
// 	dirBox = dir * dirBox;

	return orientedBox;
}
