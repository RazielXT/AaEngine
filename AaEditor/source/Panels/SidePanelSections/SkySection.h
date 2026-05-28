#pragma once

class ApplicationCore;

class SkySection
{
public:
	void draw(ApplicationCore& app);

private:
	float latitude = 0.0f;
	float timeOfDay = 9.0f;
	int dayOfYear = 172;
	float moonOrbitAngle = 180.0f;
	float moonOrbitTilt = 5.14f;
};
