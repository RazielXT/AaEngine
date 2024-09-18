#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>

using namespace DirectX;

class AaCamera
{
public:

	AaCamera();
	~AaCamera();

	void setOrthograhicCamera(float width, float height, float nearZ, float farZ);
	void setPerspectiveCamera(float fov, float aspectRatio, float nearZ, float farZ);

	const XMMATRIX& getProjectionMatrix() const;
	const XMMATRIX& getViewMatrix();
	const XMMATRIX& getViewProjectionMatrix();

	void setPosition(XMFLOAT3 position);
	void setInCameraRotation(XMFLOAT3* direction);

	void yaw(float Yaw);
	void pitch(float Pitch);
	void roll(float Roll);
	void resetRotation();

	void lookTo(XMFLOAT3 Direction);
	void lookAt(XMFLOAT3 target);

	XMFLOAT3 getPosition() const;

	DirectX::BoundingFrustum prepareFrustum();

private:

	bool dirty;
	float yaw_, pitch_, roll_;
	XMMATRIX view_m;
	XMMATRIX projection_m;
	XMMATRIX view_projection_m;
	XMFLOAT3 position;
	XMFLOAT3 direction;
	XMFLOAT3 upVector;
};
