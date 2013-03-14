#ifndef _AA_OBJ_LOADER_
#define _AA_OBJ_LOADER_

#include "AaRenderSystem.h"
#include "AaModelInfo.h"
#include "StringFunctions.h"
#include <boost/algorithm/string/predicate.hpp>
#include <iostream>
#include <fstream>
#include <vector>

class ObjFileParser
{
public:

	ObjFileParser(AaRenderSystem* mRenderSystem)
	{
		vertices_=0;
		normals_=0;
		texCoords_=0;
		totalVerts_=0;
		d3dDevice=mRenderSystem->getDevice();
	}

	AaModelInfo* loadOBJ(std::string filename);
	
private:

	
	bool loadFile(std::string filename);
	bool loadContent(AaModelInfo* model);
	void clearBuffers();

	StringFunctions strFunctions;
	float *vertices_;
	float *normals_;
	float *texCoords_;
	int totalVerts_;
	ID3D11Device* d3dDevice;
};


#endif