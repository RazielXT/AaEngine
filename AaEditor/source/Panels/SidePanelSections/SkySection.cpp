#include "SkySection.h"

#include "ApplicationCore.h"
#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "imgui.h"
#include <algorithm>

void SkySection::draw(ApplicationCore& app)
{
	bool change = ImGui::SliderFloat("Time of Day", &timeOfDay, 0.0f, 24.0f);
	change |= ImGui::SliderFloat("Latitude", &latitude, -XM_PIDIV2, XM_PIDIV2);
	change |= ImGui::SliderInt("Day of Year", &dayOfYear, 1, 365);

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

		VoxelizeSceneTask::Get().revoxelize();
	}

	if (ImGui::ColorEdit3("Sun Color", &app.lights.directionalLight.color.x))
	{
		VoxelizeSceneTask::Get().revoxelize();
	}

	if (ImGui::SliderFloat("Moon phase", &moonPhase, -1, 1))
	{
		auto material = app.resources.materials.getMaterial("Moon");
		material->SetParameter("MoonPhase", &moonPhase, 1);
	}

	ImGui::SliderFloat("Clouds amount", &app.params.sun.CloudsAmount, -1, 1);
	ImGui::SliderFloat("Clouds density", &app.params.sun.CloudsDensity, 0, 1);
	ImGui::SliderFloat("Clouds speed", &app.params.sun.CloudsSpeed, 0, 0.02f);
}
