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
	{
		float rcpWidth = 1.0f / width;
		float rcpHeight = 1.0f / height;
		float rcpDepth = 1.0f / (nearZ - farZ);

		// Create the orthographic projection matrix with reverse Z
		reversedProjection_m = XMMATRIX(
			2.0f * rcpWidth, 0.0f, 0.0f, 0.0f,
			0.0f, 2.0f * rcpHeight, 0.0f, 0.0f,
			0.0f, 0.0f, rcpDepth, 0.0f,
			0.0f, 0.0f, -farZ * rcpDepth, 1.0f
		);
	}
	this->width = width;
	this->height = height;
	this->nearZ = nearZ;
	this->farZ = farZ;
	dirty = true;
	orthographic = true;
	zProjection = (farZ - nearZ) / nearZ;
}

void AaCamera::setPerspectiveCamera(float fov, float aspectRatio, float nearZ, float farZ)
{
	projection_m = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspectRatio, nearZ, farZ);
	{
		float Y = 1.0f / std::tanf(XMConvertToRadians(fov) * 0.5f);
		float X = Y * 1/aspectRatio;

		// ReverseZ puts far plane at Z=0 and near plane at Z=1.  This is never a bad idea, and it's
		// actually a great idea with F32 depth buffers to redistribute precision more evenly across
		// the entire range.  It requires clearing Z to 0.0f and using a GREATER variant depth test.
		// Some care must also be done to properly reconstruct linear W in a pixel shader from hyperbolic Z.
		float Q1 = nearZ / (farZ - nearZ);
		float Q2 = nearZ * (farZ / (farZ - nearZ));

		reversedProjection_m = XMMATRIX(
			X, 0.0f, 0.0f, 0.0f,
			0.0f, Y, 0.0f, 0.0f,
			0.0f, 0.0f, Q1, 1.0f,
			0.0f, 0.0f, Q2, 0.0f
		);
	}
	dirty = true;
	orthographic = false;
	zProjection = (farZ - nearZ) / nearZ;
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

		view_projection_m = XMMatrixMultiply(view_m, reversedProjection_m);
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

float AaCamera::getCameraZ() const
{
	return zProjection;
}

float AaCamera::getTanHalfFovH() const
{
	XMFLOAT4X4 proj;
	XMStoreFloat4x4(&proj, reversedProjection_m);
	return proj._11;
}
