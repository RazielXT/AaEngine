#ifndef _AA_FRAME_LISTENER_
#define _AA_FRAME_LISTENER_

class AaFrameListener
{

public:

	AaFrameListener(){};
	virtual ~AaFrameListener(){};

	virtual bool frameStarted(float timeSinceLastFrame) = 0;

};

#endif