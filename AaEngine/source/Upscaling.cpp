#include "Upscaling.h"
#include "RenderSystem.h"

Upscaling::Upscaling(RenderSystem& rs) : dlss(rs), fsr(rs), renderSystem(rs)
{
}

void Upscaling::shutdown()
{
	dlss.shutdown();
	fsr.shutdown();
}

void Upscaling::onResize()
{
	dlss.onResize();
	fsr.onResize();
}

bool Upscaling::enabled() const
{
	return dlss.enabled() || fsr.enabled();
}

XMUINT2 Upscaling::getRenderSize() const
{
	if (dlss.enabled())
		return dlss.getRenderSize();

	if (fsr.enabled())
		return fsr.getRenderSize();

	return {};
}

float Upscaling::getMipLodBias() const
{
	float bias = 0;
	if (dlss.enabled())
		bias = std::log2f(getRenderSize().x / float(renderSystem.getOutputSize().x)) - 1.0f;
	else if (fsr.enabled())
		bias = fsr.getMipLodBias();

	return bias;
}
