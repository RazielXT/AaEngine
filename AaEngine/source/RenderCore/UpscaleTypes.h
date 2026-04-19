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

static const char* UpscaleModeNames[] = {
	"UltraPerformance",
	"Performance",
	"Balanced",
	"Quality",
	"AA",
	"Off"
};
