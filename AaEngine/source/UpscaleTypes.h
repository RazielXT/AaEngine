#pragma once

enum class UpscaleMode
{
	UltraPerformance,
	Performance,
	Balanced,
	Quality,
	AA,
	Off
};

static const char* UpscaleModeNames[6] = {
	"UltraPerformance",
	"Performance",
	"Balanced",
	"Quality",
	"AA",
	"Off"
};
