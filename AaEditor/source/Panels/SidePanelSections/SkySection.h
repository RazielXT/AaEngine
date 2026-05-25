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
	float moonPhase = 0.0f;
};
