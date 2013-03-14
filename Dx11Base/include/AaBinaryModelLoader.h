#ifndef _AA_BINARY_MODEL_LOADER_
#define _AA_BINARY_MODEL_LOADER_

#include "AaRenderSystem.h"
#include "AaModelInfo.h"
#include "AaLogger.h"
#include <boost/algorithm/string/predicate.hpp>
#include <iostream>



class AaBinaryModelLoader
{
public:

	AaBinaryModelLoader(AaRenderSystem* mRenderSystem)
	{
		d3dDevice=mRenderSystem->getDevice();
	}

	AaModelInfo* load(std::string filename);
	
private:

	ID3D11Device* d3dDevice;
};


#endif

