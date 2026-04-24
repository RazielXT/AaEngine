#include "LogPanel.h"
#include "imgui.h"
#include "Utils/Logger.h"

void LogPanel::draw()
{
	const auto& history = Logger::getHistory();

	for (int i = (int)history.size() - 1; i >= 0; i--)
	{
		const auto& entry = history[i];

		ImVec4 color;
		const char* prefix = "";
		switch (entry.severity)
		{
		case Logger::Severity::Error:
			color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
			prefix = "[ERROR] ";
			break;
		case Logger::Severity::Warning:
			color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
			prefix = "[WARNING] ";
			break;
		default:
			color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
			break;
		}

		ImGui::TextColored(color, "%s %s%s", entry.time.c_str(), prefix, entry.text.c_str());
	}
}
