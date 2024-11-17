#pragma once

#include "AaMath.h"
#include <DirectXCollision.h>

using namespace DirectX;

class AaCamera
{
public:

	AaCamera();
	~AaCamera();

	void setOrthographicCamera(float width, float height, float nearZ, float farZ);
	void setPerspectiveCamera(float fov, float aspectRatio, float nearZ, float farZ);

	bool isOrthographic() const;

	const XMMATRIX& getProjectionMatrix() const;
	const XMMATRIX& getViewMatrix() const;
	const XMMATRIX& getViewProjectionMatrix() const;

	void updateMatrix();

	void move(Vector3 position);
	void setPosition(Vector3 position);
	void setInCameraRotation(Vector3& direction) const;

	void yaw(float Yaw);
	void pitch(float Pitch);
	void roll(float Roll);
	void rotate(Vector3 axis, float angle);
	void resetRotation();

	void setDirection(Vector3 Direction);
	void lookAt(Vector3 target);

	Vector3 getPosition() const;

	DirectX::BoundingFrustum prepareFrustum() const;
	DirectX::BoundingOrientedBox prepareOrientedBox() const;

	float getCameraZ() const;
	float getTanHalfFovH() const;

private:

	float width{}, height{}, nearZ{}, farZ{};

	bool dirty = true;
	float yaw_ = 0, pitch_ = 0, roll_ = 0;
	bool orthographic = false;
	float zProjection{};

	XMMATRIX view_m;
	XMMATRIX projection_m;
	XMMATRIX reversedProjection_m;
	XMMATRIX view_projection_m;
	Vector3 position;
	Vector3 direction;
	Vector3 upVector;

	Quaternion rotation;
};
