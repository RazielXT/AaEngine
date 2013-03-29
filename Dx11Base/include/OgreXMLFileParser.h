#ifndef _AA_OGREXML_LOADER_
#define _AA_OGREXML_LOADER_

#include "AaRenderSystem.h"
#include "AaModelInfo.h"
#include "AaLogger.h"
#include <boost/algorithm/string/predicate.hpp>
#include <iostream>
#include "tinyxml.h"
#include <vector>
#include <map>


class OgreXMLFileParser
{
public:

	OgreXMLFileParser(AaRenderSystem* mRenderSystem)
	{
		d3dDevice=mRenderSystem->getDevice();
	}

	AaModelInfo* loadOgreXML(std::string filename, bool optimize=true, bool saveBinary=false );
	
private:

	bool parseOgreXMLFile(std::string filename, bool optimize);
	void parseFaces(const TiXmlElement* facesElement);
	void parseGeometry(const TiXmlElement* geometryElement, bool optimize);
	void saveBinaryFile(std::string filename, RawModelInfo* saveInfo = NULL);

	AaModelInfo* createBuffers(bool optimize);
	void clearBuffers();

	std::map<std::string,int> vertexMap;
	std::vector<int> vertexVector;
	std::map<int,int> optimizedindices;
	ID3D11Device* d3dDevice;
	VertexPos* vertices;
	VertexPos2* verticesNT;
	//char usage_flags;
	float *vertices_;
	float *normals_;
	float *tangents_;
	float *texCoords_;
	WORD *indices_;
	int index_count;
	int vertex_count;
};


#endif

