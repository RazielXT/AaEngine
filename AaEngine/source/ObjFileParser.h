#pragma once

#include "AaModelInfo.h"

class ObjFileParser
{
public:

	ObjFileParser()
	{
	}

	AaModelInfo* loadOBJ(std::string filename);
	
private:

	bool loadFile(std::string filename, AaModelInfo*);
};
