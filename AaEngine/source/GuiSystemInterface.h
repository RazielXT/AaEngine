#ifndef ROCKETSYSTEMINTERFACE_H
#define ROCKETSYSTEMINTERFACE_H

#include <Rocket/Core/SystemInterface.h>


class GuiSystemInterface : public Rocket::Core::SystemInterface
{
public:

	GuiSystemInterface(float* elapsedTime)
	{
		this->elapsedTime = elapsedTime;
	}

	/// Get the number of seconds elapsed since the start of the application
	/// @returns Seconds elapsed
	virtual float GetElapsedTime()
	{
		return *elapsedTime;
	}

private:
	float* elapsedTime;
};

#endif
