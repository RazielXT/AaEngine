#include "SkySection.h"

#include "ApplicationCore.h"
#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "imgui.h"
#include "Utils/MathUtils.h"
#include <algorithm>

static float getEclipseFactor(SkyParameters& Sky)
{
	auto pixelLookDir = Sky.MoonDirection;

	// 1. Get normalized sun direction (invert Sky.SunDirection if it points *toward* light)
	auto sunDir = -Sky.SunDirection;

	// 2. Find how closely this pixel's look direction aligns with the sun
	float sunAlignment = pixelLookDir.Dot(sunDir);

	// 3. Define the eclipse range using dot product values.
	// Since cos(0.5 degrees) is roughly 0.99996, we use very high thresholds.
	// Adjust these two values to make the dimming zone wider or sharper.
	float eclipseStart = 0.9991f; // Begins dimming when very close to the sun
	float eclipseFull = 0.99995f; // Reaches maximum dimming/totality

	// smoothstep returns 0 at start, 1 at full alignment
	float eclipseFactor = smoothstep(eclipseStart, eclipseFull, sunAlignment);

	// 4. Determine how dark the moon gets at peak eclipse (e.g., 0.0 = pure black silhouette)
	float minimumMoonBrightness = 0.05f;
	float moonDimming = std::lerp(1.0f, minimumMoonBrightness, eclipseFactor);

	return moonDimming;
}

void SkySection::draw(ApplicationCore& app)
{
	bool change = ImGui::SliderFloat("Time of Day", &timeOfDay, 0.0f, 24.0f);
	change |= ImGui::SliderFloat("Latitude", &latitude, -XM_PIDIV2, XM_PIDIV2);
	change |= ImGui::SliderInt("Day of Year", &dayOfYear, 1, 365);
	change |= ImGui::SliderFloat("Moon Orbit Angle", &moonOrbitAngle, 0.0f, 360.0f);
	change |= ImGui::SliderFloat("Moon Orbit Tilt", &moonOrbitTilt, -10.0f, 10.0f);

	if (change)
	{
		float declination = XMConvertToRadians(-23.44f) * std::cos(XM_2PI / 365.0f * (dayOfYear + 10));
		float hourAngle = (timeOfDay - 12.0f) * (XM_PI / 12.0f);

		float sinAlt = std::sin(latitude) * std::sin(declination) +
			std::cos(latitude) * std::cos(declination) * std::cos(hourAngle);
		float altitude = std::asin(std::clamp(sinAlt, -1.0f, 1.0f));

		float cosAz = (std::sin(declination) - std::sin(altitude) * std::sin(latitude)) /
			(std::cos(altitude) * std::cos(latitude));

		float azimuth = std::acos(std::clamp(cosAz, -1.0f, 1.0f));
		if (hourAngle > 0)
			azimuth = XM_2PI - azimuth;

		Vector3 sunPos;
		sunPos.x = std::cos(altitude) * std::sin(azimuth);
		sunPos.y = std::sin(altitude);
		sunPos.z = std::cos(altitude) * std::cos(azimuth);

		app.lights.directionalLight.direction = -sunPos;
		app.lights.directionalLight.direction.Normalize();

		// Moon direction - same astronomical formula with orbit offset
		float moonHourAngle = hourAngle + XMConvertToRadians(moonOrbitAngle);
		float moonDeclination = declination + XMConvertToRadians(moonOrbitTilt);

		float sinMoonAlt = std::sin(latitude) * std::sin(moonDeclination) +
			std::cos(latitude) * std::cos(moonDeclination) * std::cos(moonHourAngle);
		float moonAltitude = std::asin(std::clamp(sinMoonAlt, -1.0f, 1.0f));

		float cosMoonAz = (std::sin(moonDeclination) - std::sin(moonAltitude) * std::sin(latitude)) /
			(std::cos(moonAltitude) * std::cos(latitude));
		float moonAzimuth = std::acos(std::clamp(cosMoonAz, -1.0f, 1.0f));
		if (std::sin(moonHourAngle) > 0)
			moonAzimuth = XM_2PI - moonAzimuth;

		Vector3 moonPos;
		moonPos.x = std::cos(moonAltitude) * std::sin(moonAzimuth);
		moonPos.y = std::sin(moonAltitude);
		moonPos.z = std::cos(moonAltitude) * std::cos(moonAzimuth);

		app.params.sky.MoonDirection = moonPos;
		app.params.sky.EclipseFactor = getEclipseFactor(app.params.sky);

		VoxelizeSceneTask::Get().revoxelize();
	}

	if (ImGui::ColorEdit3("Sun Color", &app.lights.directionalLight.color.x))
	{
		VoxelizeSceneTask::Get().revoxelize();
	}

	ImGui::SliderFloat("Clouds amount", &app.params.sky.CloudsAmount, -1, 1);
	ImGui::SliderFloat("Clouds density", &app.params.sky.CloudsDensity, 0, 1);
	ImGui::SliderFloat("Clouds speed", &app.params.sky.CloudsSpeed, 0, 0.02f);
}
