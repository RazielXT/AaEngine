#pragma once

#include "Camera.h"
#include "MathUtils.h"

const auto FrontDirection = XMFLOAT3(0, 0, 1);
const auto UpVector = XMFLOAT3(0, 1, 0);

Camera::Camera()
{
	view_m = XMMatrixIdentity();

	position = XMFLOAT3(0, 0, 0);
	direction = FrontDirection;
	upVector = UpVector;
}

Camera::~Camera()
{
}

void Camera::setOrthographicCamera(float width, float height, float nearZ, float farZ)
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

	dirty = true;
	orthographic = true;
	zProjection = (farZ - nearZ) / nearZ;

	params = {};
	params.nearZ = nearZ;
	params.farZ = farZ;
	params.aspectRatio = width / height;
	params.height = height;
	params.width = width;
}

void Camera::setPerspectiveCamera(float fov, float aspectRatio, float nearZ, float farZ)
{
	projection_m = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspectRatio, nearZ, farZ);
	{
		float Y = 1.0f / std::tanf(XMConvertToRadians(fov) * 0.5f);
		float X = Y * 1/aspectRatio;

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

	params = {};
	params.nearZ = nearZ;
	params.farZ = farZ;
	params.aspectRatio = aspectRatio;
	params.fov = fov;
}

bool Camera::isOrthographic() const
{
	return orthographic;
}

XMMATRIX Camera::getProjectionMatrix() const
{
	XMMATRIX proj = reversedProjection_m;

	if (hasOffset)
		proj = XMMatrixMultiply(proj, m_PixelOffsetMatrix);

	return proj;
}

XMMATRIX Camera::getProjectionMatrixNoReverse() const
{
	return projection_m;
}

XMMATRIX Camera::getProjectionMatrixNoOffset() const
{
	return reversedProjection_m;
}

const XMMATRIX& Camera::getViewMatrix() const
{
	return view_m;
}

XMMATRIX Camera::getViewProjectionMatrix() const
{
	return view_projection_m;
}

void Camera::updateMatrix()
{
	if (dirty)
	{
		view_m = XMMatrixMultiply(XMMatrixLookToLH(XMLoadFloat3(&position), XMLoadFloat3(&direction), XMLoadFloat3(&upVector)), XMMatrixRotationY(yaw_));
		view_m = XMMatrixMultiply(view_m, XMMatrixRotationX(pitch_));
		view_m = XMMatrixMultiply(view_m, XMMatrixRotationZ(roll_));

// 		auto quatMatrix = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));
// 		view_m = XMMatrixMultiply(XMMatrixLookToLH(XMLoadFloat3(&position), XMLoadFloat3(&direction), XMLoadFloat3(&upVector)), quatMatrix);
	}
	if (dirty || hasOffset)
	{
		view_projection_m = XMMatrixMultiply(view_m, getProjectionMatrix());
		dirty = false;
	}
}

void Camera::move(Vector3 p)
{
	position += p;

	dirty = true;
}

void Camera::setPosition(Vector3 p)
{
	position = p;
	dirty = true;
}

void Camera::setInCameraRotation(Vector3& direction) const
{
	direction.x *= -1;

	XMVECTOR dir = XMLoadFloat3(&direction);
	dir = XMVector3Transform(dir, XMMatrixRotationX(pitch_));
	dir = XMVector3Transform(dir, XMMatrixRotationY(yaw_));
	XMStoreFloat3(&direction, dir);

	direction.x *= -1;
	direction.y *= -1;
}

DirectX::SimpleMath::Vector3 Camera::getCameraDirection() const
{
	Vector3 dir{ 0, 0, 1 };
	setInCameraRotation(dir);

	return dir;
}

void Camera::yaw(float Yaw)
{
	Vector3 yAxis = rotation * Vector3::UnitY;

	rotate(yAxis, Yaw);
	yaw_ += Yaw;
	dirty = true;
}

void Camera::pitch(float Pitch)
{
	rotate(Vector3::UnitX, Pitch);
	pitch_ += Pitch;
	dirty = true;
}

void Camera::roll(float Roll)
{
	rotate(Vector3::UnitZ, Roll);
	roll_ += Roll;
	dirty = true;
}

void Camera::rotate(Vector3 axis, float angle)
{
// 	rotation *= Quaternion::CreateFromAxisAngle(axis, angle);
// 	pitch_ = angle * axis.x;
// 	yaw_ = angle * axis.y;
// 	roll_ = angle * axis.z;
// 	dirty = true;
}

void Camera::resetRotation()
{
	//rotation = Quaternion::Identity;
	yaw_ = pitch_ = roll_ = 0;
	dirty = true;
}

void Camera::setDirection(Vector3 Direction)
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

void Camera::lookAt(Vector3 target)
{
	setDirection(target - position);
}

void Camera::lookTo(FXMVECTOR vEyePt, FXMVECTOR vLookatPt)
{
	XMStoreFloat3(&position, vEyePt);

	XMStoreFloat3(&direction, vLookatPt);

	// Calc the view matrix
	view_m = XMMatrixLookAtLH(vEyePt, vLookatPt, g_XMIdentityR1);
	//XMStoreFloat4x4(&m_mView, mView);

// 	XMMATRIX mInvView = XMMatrixInverse(nullptr, mView);
// 
// 	// The axis basis vectors and camera position are stored inside the 
// 	// position matrix in the 4 rows of the camera's world matrix.
// 	// To figure out the yaw/pitch of the camera, we just need the Z basis vector
// 	XMFLOAT3 zBasis;
// 	XMStoreFloat3(&zBasis, mInvView.r[2]);
// 
// 	m_fCameraYawAngle = atan2f(zBasis.x, zBasis.z);
// 	float fLen = sqrtf(zBasis.z * zBasis.z + zBasis.x * zBasis.x);
// 	m_fCameraPitchAngle = -atan2f(zBasis.y, fLen);
}

void Camera::setProjection(XMMATRIX proj)
{
	orthographic = true;
	projection_m = reversedProjection_m = proj;
	view_projection_m = XMMatrixMultiply(view_m, proj);
	dirty = false;
}

Vector3 Camera::getPosition() const
{
	return position;
}

DirectX::BoundingFrustum Camera::prepareFrustum() const
{
	DirectX::BoundingFrustum frustum;
	DirectX::BoundingFrustum::CreateFromMatrix(frustum, projection_m);
	frustum.Transform(frustum, DirectX::XMMatrixInverse(nullptr, view_m));

	return frustum;
}

DirectX::BoundingOrientedBox Camera::prepareOrientedBox() const
{
	DirectX::BoundingOrientedBox orientedBox;
	//orientedBox.Extents = { params.width / 2.0f, params.height / 2.0f, (params.farZ - params.nearZ) / 2.0f };
	orientedBox.Transform(orientedBox, DirectX::XMMatrixInverse(nullptr, view_projection_m));

	return orientedBox;
}

float Camera::getCameraZ() const
{
	return zProjection;
}

float Camera::getTanHalfFovH() const
{
	return reversedProjection_m.r[0].m128_f32[0];
}

float jitterScaleX = -2.f;
float jitterScaleY = 2.f;

void Camera::setPixelOffset(XMFLOAT2 offset, XMUINT2 viewportSize)
{
	hasOffset = true;
	m_PixelOffset = offset;

	// Calculate the translation based on the viewport and offset
	float translateX = jitterScaleX * offset.x / float(viewportSize.x);
	float translateY = jitterScaleY * offset.y / float(viewportSize.y);

	// Create the translation matrix
	m_PixelOffsetMatrix = XMMatrixTranslation(translateX, translateY, 0.f);

	// Calculate the inverse of the translation matrix
	m_PixelOffsetMatrixInv = XMMatrixInverse(nullptr, m_PixelOffsetMatrix);
}

void Camera::clearPixelOffset()
{
	hasOffset = false;
}

const Camera::Params& Camera::getParams() const
{
	return params;
}
