#pragma once

class AaFrameListener
{
public:

	AaFrameListener() = default;
	virtual ~AaFrameListener() = default;

	virtual bool frameStarted(float timeSinceLastFrame) = 0;
};
