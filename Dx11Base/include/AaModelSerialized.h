#ifndef _AA_MODEL_SERIALIZED_
#define _AA_MODEL_SERIALIZED_

#include "AaLogger.h"
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/lexical_cast.hpp>
#include <vector>
#include <boost/serialization/vector.hpp>

class AaModelSerialized
{
public:
	AaModelSerialized() 
	{
		useindices=false;
		positions_=NULL;
		normals_=NULL;
		tangents_=NULL;
		texCoords_=NULL;
		indices_=NULL;
		index_count=0;
		vertex_count=0;
		doCleanup=true;
	}

	~AaModelSerialized() 
	{
		if(doCleanup)
		{
			if(positions_) delete [] positions_;
			if(normals_) delete [] normals_;
			if(tangents_) delete [] tangents_;
			if(texCoords_) delete [] texCoords_;
			if(indices_) delete [] indices_;
		}
	}

	float *positions_;
	float *normals_;
	float *tangents_;
	float *texCoords_;
	WORD *indices_;
	int index_count;
	int vertex_count;
	bool useindices;

	bool doCleanup;

private:

	friend class boost::serialization::access;

	template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & index_count;
        ar & vertex_count;
		ar & useindices;
		if(!positions_) positions_= new float[vertex_count*3];
		ar & boost::serialization::make_array(positions_,vertex_count*3);
		if(!normals_) normals_= new float [vertex_count*3];
		ar & boost::serialization::make_array(normals_,vertex_count*3);
		if(!tangents_) tangents_= new float [vertex_count*3];
		ar & boost::serialization::make_array(tangents_,vertex_count*3);
		if(!texCoords_) texCoords_= new float [vertex_count*2];
		ar & boost::serialization::make_array(texCoords_,vertex_count*2);
		if(useindices)
		{
			if(!indices_) indices_= new WORD [index_count];
			ar & boost::serialization::make_array(indices_,index_count);
		}

		//for(int i=0;i<vertex_count;i++)
		//	AaLogger::getLogger()->writeMessage("Pos : "+boost::lexical_cast<std::string>(*(normals_+i*3))+" , "+boost::lexical_cast<std::string>(*(normals_+i*3+1))+" , "+boost::lexical_cast<std::string>(*(normals_+i*3+2)));
    }
    
};


#endif