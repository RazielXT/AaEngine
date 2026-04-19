#pragma once

#include "RenderCore/DLSS.h"
#include "RenderCore/FSR.h"

class RenderSystem;

struct Upscaling
{
	Upscaling(RenderSystem& renderSystem);

	DLSS dlss;
	FSR fsr;

	void shutdown();
	void onResize();

	bool enabled() const;
	XMUINT2 getRenderSize() const;
	float getMipLodBias() const;

	RenderSystem& renderSystem;
};