#include "SystemUtils.h"
#include <dxgi1_4.h>
#include <Psapi.h>

size_t GetGpuMemoryUsage()
{
	static IDXGIFactory4* pFactory{};
	if (!pFactory)
		CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&pFactory);

	static IDXGIAdapter3* adapter{};
	if (!adapter)
		pFactory->EnumAdapters(0, reinterpret_cast<IDXGIAdapter**>(&adapter));

	DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
	adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);

	return videoMemoryInfo.CurrentUsage / 1024 / 1024;
}

size_t GetSystemMemoryUsage()
{
	// Get the current process handle
	HANDLE processHandle = GetCurrentProcess();
	size_t usage{};

	// Memory usage
	PROCESS_MEMORY_COUNTERS memCounters;
	if (GetProcessMemoryInfo(processHandle, &memCounters, sizeof(memCounters))) {
		usage = memCounters.WorkingSetSize;
	}

	// Close the process handle
	CloseHandle(processHandle);

	return usage / 1024 / 1024;
}