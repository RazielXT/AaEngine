#pragma once

#include <fstream>
#include <ctime>
#include <string>
#include <windows.h>

class AaLogger
{
public:

	AaLogger()
    {
		myfile.open("Logger.log");
    }

	~AaLogger()
    {
		log("======================Closing======================");
    }

	enum class Severity { Info, Warning, Error };

	static void logWarning(std::string text)
	{
		log(text, Severity::Warning);
	}

	static void logError(std::string text)
	{
		log(text, Severity::Error);
	}

	static void logErrorD3D(std::string text, HRESULT result)
	{
		log("D3D " + text + "\n" + GetErrorMessage(result), Severity::Error);
	}

	static void log(std::string text, Severity severity = Severity::Info)
	{
		auto timer = time(nullptr);
		auto tm_info = localtime(&timer);

		char timeString[26]{};
		strftime(timeString, std::size(timeString), "%H:%M:%S", tm_info);

		std::string severityInfo;
		if (severity == Severity::Error)
			severityInfo += "[ERROR] ";
		else if (severity == Severity::Warning)
			severityInfo += "[WARNING] ";

		get()->myfile << timeString << ": " << severityInfo << text << std::endl;
	}

private:

	static AaLogger* get()
	{
		static AaLogger instance;

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
};
