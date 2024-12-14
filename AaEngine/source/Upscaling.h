#pragma once

#include "DLSS.h"
#include "FSR.h"

class RenderSystem;

struct Upscaling
{
	RenderSystem& renderSystem;
	DLSS dlss;
	FSR fsr;

	void init();
	void shutdown();
	void onScreenResize();

	bool enabled() const;
	XMUINT2 getRenderSize() const;
	float getMipLodBias() const;
};