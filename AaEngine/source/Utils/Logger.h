#pragma once

#include <fstream>
#include <ctime>
#include <string>
#include <vector>
#include <windows.h>
#include <filesystem>

class Logger
{
public:

	Logger()
	{
		myfile.open("Logger.log");
	}

	~Logger()
	{
		log("======================Closing======================");
	}

	enum class Severity { Info, Warning, Error };

	struct LogEntry
	{
		std::string time;
		std::string text;
		Severity severity;
	};

	static void logWarning(std::string text)
	{
		log(text, Severity::Warning);
	}

	static void logError(std::string text)
	{
		logError(text, "");
	}

	static void logError(std::string text, std::string path)
	{
		OutputDebugStringA(text.c_str());
		log(text, Severity::Error);

		if (path.empty())
		{
			MessageBoxA(0, text.c_str(), "Error", MB_OK | MB_ICONERROR);
		}
		else
		{
			int r = MessageBoxA(0, text.c_str(), "Error", MB_RETRYCANCEL | MB_ICONERROR);

			if (r == IDRETRY)
				ShellExecuteA(nullptr, "open", (std::filesystem::absolute(path).string()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}
	}

	static void logErrorD3D(std::string text, HRESULT result)
	{
		logError("D3D " + text + "\n" + GetErrorMessage(result));
	}

	static void log(std::string text, Severity severity = Severity::Info)
	{
		auto timer = time(nullptr);
		struct tm tm_info;
		localtime_s(&tm_info, &timer);

		char timeString[26]{};
		strftime(timeString, std::size(timeString), "%H:%M:%S", &tm_info);

		std::string severityInfo;
		if (severity == Severity::Error)
			severityInfo += "[ERROR] ";
		else if (severity == Severity::Warning)
			severityInfo += "[WARNING] ";

		auto instance = get();
		instance->myfile << timeString << ": " << severityInfo << text << std::endl;

		auto& history = instance->logHistory;
		history.push_back({ timeString, text, severity });
		if (history.size() > MaxHistory)
			history.erase(history.begin());
	}

	static const std::vector<LogEntry>& getHistory()
	{
		return get()->logHistory;
	}

private:

	static constexpr size_t MaxHistory = 20;

	static Logger* get()
	{
		static Logger instance;

		return &instance;
	}

	static std::string GetErrorMessage(HRESULT hr)
	{
		char* errorMsg = nullptr;
		FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			hr,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&errorMsg,
			0,
			nullptr
		);

		std::string message(errorMsg);
		LocalFree(errorMsg);
		return message;
	}

	std::ofstream myfile;
	std::vector<LogEntry> logHistory;
};
