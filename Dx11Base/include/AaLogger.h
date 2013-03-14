#ifndef _AA_LOGGER_
#define _AA_LOGGER_

#include <iostream>
#include <fstream>
#include <time.h>

class AaLogger
{

public:

	AaLogger()
    {
		myfile.open("Logger.log");
    }

	~AaLogger()
    {
		writeMessage("======================Closing======================");
		myfile.close();
    }
	
	static AaLogger* getLogger()
	{
		if (!single)   // Only allow one instance of class to be generated.
	      single = new AaLogger();

		return single;
	}

	void writeMessage(std::string text)
	{
		time_t rawtime;
		struct tm * timeinfo;
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		
		char timeString[10];

		if(timeinfo->tm_min<10 && timeinfo->tm_sec>=10)
		sprintf(timeString,"%d:0%d:%d\0",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
		else
		if(timeinfo->tm_sec<10 && timeinfo->tm_min>=10)
		sprintf(timeString,"%d:%d:0%d\0",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
		else
		if(timeinfo->tm_sec<10 && timeinfo->tm_min<10)
		sprintf(timeString,"%d:0%d:0%d\0",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
		else
		sprintf(timeString,"%d:%d:%d\0",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);

		myfile << timeString << ": " << text.c_str() << "\n" ;

		myfile.flush();
	}

private:

	std::ofstream myfile;
	static AaLogger *single;
	
};

#endif