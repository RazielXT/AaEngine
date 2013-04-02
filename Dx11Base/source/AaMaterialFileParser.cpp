#include "AaMaterialFileParser.h"
#include "AaLogger.h"

#include <vector>
/*
short AaMaterialFileParser::getConstBufferSize(std::vector<std::string>* constants, BUFFER_TYPE type)
{
	short pos=0;

	if(type==BUFFER_TYPE_PER_MAT)
		for(int i=0;i<constants->size();i++)
		{
			std::string constant=constants->at(i);
			
			if(constant=="diffuse_colour")
			{
				pos+=3;
				continue;
			}

			if(constant=="specular_colour")
			{
				pos+=3;
				continue;
			}

			if(constant=="glow_power")
			{
				pos++;
				continue;
			}

			AaLogger::getLogger()->writeMessage("ERROR, unknown per_mat constant "+constant);
			return 0;
		}

	if(type==BUFFER_TYPE_PER_OBJ)
		for(int i=0;i<constants->size();i++)
		{
			std::string constant=constants->at(i);
			
			if(constant=="world_view_projection_matrix")
			{
				pos+=16;
				continue;
			}

			if(constant=="world_position")
			{
				pos+=3;
				continue;
			}


			AaLogger::getLogger()->writeMessage("ERROR, unknown per_obj constant "+constant);
			return 0;
		}

	return pos;
}*/

std::vector<materialInfo> AaMaterialFileParser::parseAllMaterialFiles(std::string directory, bool subFolders)
{
	std::vector<materialInfo> mats;

	if (!boost::filesystem::exists(directory)) return mats;

	boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
	for ( boost::filesystem::directory_iterator itr(directory); itr != end_itr; ++itr )
	{
		if ( is_directory(itr->path()) && subFolders)
		{
		   std::vector<materialInfo> subMats=parseAllMaterialFiles(itr->path().generic_string(),subFolders);
		   mats.insert(mats.end(),subMats.begin(),subMats.end());
		}
		else if ( boost::filesystem::is_regular_file(itr->path()) && itr->path().generic_string().find(".mat")!=std::string::npos) // see below
		{	
			AaLogger::getLogger()->writeMessage(itr->path().generic_string());
			std::vector<materialInfo> localMats=parseMaterialFile(itr->path().generic_string());
			mats.insert(mats.end(),localMats.begin(),localMats.end());
			AaLogger::getLogger()->writeMessage("Loaded materials: "+boost::lexical_cast<std::string,int>(localMats.size()));
		}
	}

	AaLogger::getLogger()->writeMessage(	"Total mats here: "+boost::lexical_cast<std::string,int>(mats.size()));
	return mats;
}


shaderRefMaps AaMaterialFileParser::parseAllShaderFiles(std::string directory, bool subFolders)
{
	shaderRefMaps shaders;
	
	if (!boost::filesystem::exists(directory)) return shaders;

	boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
	for ( boost::filesystem::directory_iterator itr(directory); itr != end_itr; ++itr )
	{
		if ( is_directory(itr->path()) && subFolders)
		{
		   shaderRefMaps subInfos=parseAllShaderFiles(itr->path().generic_string(),subFolders);
		   shaders.vertexShaderRefs.insert(subInfos.vertexShaderRefs.begin(),subInfos.vertexShaderRefs.end());
		   shaders.pixelShaderRefs.insert(subInfos.pixelShaderRefs.begin(),subInfos.pixelShaderRefs.end());
		}
		else if ( boost::filesystem::is_regular_file(itr->path()) && itr->path().generic_string().find(".program")!=std::string::npos) // see below
		{	
			AaLogger::getLogger()->writeMessage(itr->path().generic_string());
			shaderRefMaps localShaders=parseShaderFile(itr->path().generic_string());
			shaders.vertexShaderRefs.insert(localShaders.vertexShaderRefs.begin(),localShaders.vertexShaderRefs.end());
			shaders.pixelShaderRefs.insert(localShaders.pixelShaderRefs.begin(),localShaders.pixelShaderRefs.end());	
			AaLogger::getLogger()->writeMessage("Loaded shader references: "+boost::lexical_cast<std::string,int>(localShaders.pixelShaderRefs.size()+localShaders.vertexShaderRefs.size()));
		}
	}

	AaLogger::getLogger()->writeMessage(	"Total shader references here: "+boost::lexical_cast<std::string,int>(shaders.pixelShaderRefs.size()+shaders.vertexShaderRefs.size()));
	return shaders;
}


std::vector<materialInfo> AaMaterialFileParser::parseMaterialFile(std::string file)
{

	std::string line;

	std::vector<materialInfo> mats;

	std::ifstream inStream( file, std::ios::in);

	// exit program if unable to open file.
	if (!inStream)
	{
		std::cerr << "File could not be opened\n";
		return mats;
	}

	bool error=false;

	while(getline(inStream,line) && !error)
	{

		line=strFunctions.toNextWord(line);

		 //load material
		 if (boost::starts_with(line, "material"))
		 {
			 materialInfo mat;
			 
			 line.erase(0,8);
			 line=strFunctions.onlyNextWord(line);

			 mat.name=line;

			 bool valid=false;

			 //get material start
			 while(getline(inStream,line) && !error)
			 {
				 if(strFunctions.getToken(&line,"{"))
				 {
					valid=true;
					break;
				 }
			 }

			 if(!valid) { error=true; continue; }

			 valid=false;

			 //get material info
			 while(getline(inStream,line) && !error)
			 {
				 //vs
				 if(strFunctions.getToken(&line,"vertex_ref"))
				 {
					 line=strFunctions.onlyNextWord(line);
					 mat.vs_ref=line;
					 continue;
				 }

				 //ps
				 if(strFunctions.getToken(&line,"pixel_ref"))
				 {
					 line=strFunctions.onlyNextWord(line);
					 mat.ps_ref=line;
					 continue;
				 }

				 //rs
				 if(strFunctions.getToken(&line,"depth_write_off"))
				 {
					 mat.renderStateDesc.depth_write=0;
					 continue;
				 }
				 else
				 if(strFunctions.getToken(&line,"depth_check_off"))
				 {
					 mat.renderStateDesc.depth_check=0;
					 continue;
				 }
				 else
				 if(strFunctions.getToken(&line,"culling_none"))
				 {
					 mat.renderStateDesc.culling=0;
					 continue;
				 }
				 else
				 if(strFunctions.getToken(&line,"alpha_blend"))
				 {
					 mat.renderStateDesc.alpha_blend=1;
					 continue;
				 }


				 //params to mat cbuffer
				 if(strFunctions.getToken(&line,"default_params"))
				 {

					 //get texture start
					 while(getline(inStream,line) && !error)
					 {
						 if(strFunctions.getToken(&line,"{"))
						 {
							 valid=true;
							 break;
						 }
					 }

					 if(!valid) { error=true; continue; }

					 //get const value
					 while(getline(inStream,line) && !error)
					 {
						 line=strFunctions.toNextWord(line);

						 //ending
						 if(strFunctions.getToken(&line,"}"))
						 {
							 valid=true;
							 break;
						 }	
						 if(!boost::starts_with(line,"//") && !line.empty())
						 {
							 std::string name = strFunctions.onlyNextWordAndContinue(&line);
							 line=strFunctions.toNextWord(line);
							 std::vector<float> values;

							 while (!line.empty())
							 {
								 values.push_back(boost::lexical_cast<float>(strFunctions.onlyNextWordAndContinue(&line)));
								 line=strFunctions.toNextWord(line);
							 }

							 mat.defaultParams[name] = values;

							 continue;
						 }

						  

					 }

					 if(!valid) 
						 error=true;

					 valid=false;
				 }

				 //UAVs
				 if(strFunctions.getToken(&line,"UAV"))
				 {
					 mat.psuavs.push_back(strFunctions.onlyNextWord(line));
				 }

				 //textures
				 if(strFunctions.getToken(&line,"texture"))
				 {
					 textureInfo tex;
					 valid=false;
					 bool toPS = true;
					 bool toVS = false;

					 if(boost::starts_with(line, "_vs"))
					 {
						 toPS = false;
						 toVS = true;
					 }

					 //get texture start
					 while(getline(inStream,line) && !error)
					 {
						 if(strFunctions.getToken(&line,"{"))
						 {
							valid=true;
							break;
						 }
					 }
					 
					 if(!valid) { error=true; continue; }

					 //get texture info
					 while(getline(inStream,line) && !error)
					 {
						 
						 //file
						 if(strFunctions.getToken(&line,"name"))
						 {
							 line=strFunctions.onlyNextWord(line);
							 tex.file=line;
							 continue;
						 }

						 //filtering
						 if(strFunctions.getToken(&line,"filtering"))
						 {
							 line=strFunctions.toNextWord(line);
							 tex.defaultSampler = false;

							 if(boost::starts_with(line, "bil"))
								 tex.filter=Bilinear;
							 else
							 if(boost::starts_with(line, "aniso"))
								 tex.filter=Anisotropic;
							 else
							 if(boost::starts_with(line, "none"))
								tex.filter=None;
							 else
								tex.filter=Anisotropic;

							 continue;
						 }

						 //filtering
						 if(strFunctions.getToken(&line,"border"))
						 {
							 tex.defaultSampler = false;
							 line=strFunctions.toNextWord(line);

							 if(boost::starts_with(line, "clamp"))
								 tex.bordering=TextureBorder_Clamp;
							 else
							 if(boost::starts_with(line, "warp"))
								tex.bordering=TextureBorder_Clamp;
							 else
							if(strFunctions.getToken(&line,"color"))
							{
							    tex.bordering=TextureBorder_BorderColor;
								for (int i = 0; i<4;i++)
								{
									std::string c = strFunctions.onlyNextWordAndContinue(&line);
									tex.border_color[i] = boost::lexical_cast<int>(c);
								}
							}
							else
								tex.bordering=TextureBorder_Clamp;

							 continue;
						 }

						 //ending
						 if(strFunctions.getToken(&line,"}"))
						 {
							valid=true;
							break;
						 }	 

					 }

					 if(!valid) 
						 error=true;
					 else
					 {
						 if(toPS)
							mat.pstextures.push_back(tex);
						 if(toVS)
							mat.vstextures.push_back(tex);
					 }//valid tex

					 valid=false;
				 }

				 //ending
				 if(strFunctions.getToken(&line,"}"))
				 {
					valid=true;
					break;
				 }
			 }

			 if(!valid) 
			  error=true;
			 else
				 mats.push_back(mat);

		 }	
	}

	return mats;

}




shaderRefMaps AaMaterialFileParser::parseShaderFile(std::string file)
{

	std::string line;

	shaderRefMaps shds;
	std::string name;

	std::ifstream inStream( file, std::ios::in);

	// exit program if unable to open file.
	if (!inStream)
	{
		std::cerr << "File could not be opened\n";
		return shds;
	}

	bool error=false;

	while(getline(inStream,line) && !error)
	{

		line=strFunctions.toNextWord(line);

		 //load shader
		 if (boost::starts_with(line, "vertex_shader") || boost::starts_with(line, "pixel_shader"))
		 {
			 bool pixel;

			 if (boost::starts_with(line, "vertex_shader"))
			 {
				 pixel=false;
				 line.erase(0,13);
			 }
			 else
			 {
				 pixel=true;
				 line.erase(0,12);
			 }

			 shaderRef shader;
			 shader.shader=NULL;
			 shader.usedBuffersFlag=0;
			 shader.perMatConstBufferSize = 0;

			 line=strFunctions.onlyNextWord(line);

			 name=line;

			 bool valid=false;

			 //get shader start
			 while(getline(inStream,line) && !error)
			 {
				 if(strFunctions.getToken(&line,"{"))
				 {
					valid=true;
					break;
				 }
			 }

			 if(!valid) { error=true; continue; }

			 valid=false;

			 //get shader info
			 while(getline(inStream,line) && !error)
			 {

				 if(strFunctions.getToken(&line,"file"))
				 {
					 line=strFunctions.onlyNextWord(line);
					 shader.file=line;
					 continue;
				 }

				 if(strFunctions.getToken(&line,"entry"))
				 {
					 line=strFunctions.onlyNextWord(line);
					 shader.entry=line;
					 continue;
				 }

				 if(strFunctions.getToken(&line,"profile"))
				 {
					 line=strFunctions.onlyNextWord(line);
					 shader.profile=line;
					 continue;
				 }

				 if(strFunctions.getToken(&line,"cbuffer"))
				 {
					 if(strFunctions.getToken(&line,"per_frame"))
					 {
						 shader.usedBuffersFlag |= PER_FRAME;
					 }
					 else
					 if(strFunctions.getToken(&line,"per_object"))
					 {
						 shader.usedBuffersFlag |= PER_OBJECT;
					 }
					 else
					 if(strFunctions.getToken(&line,"per_material"))
					 {
						 shader.usedBuffersFlag |= PER_MATERIAL;

						 valid=false;

						 //get constants start
						 while(getline(inStream,line) && !error)
						 {
							 if(strFunctions.getToken(&line,"{"))
							 {
								valid=true;
								break;
							 }
						 }

						 if(!valid) { error=true; continue; }

						 valid=false;
						 int varPosition = 0;

						 //get consts info
						 while(getline(inStream,line))
						 {	 
							 line = strFunctions.toNextWord(line);

							 //ending
							 if(strFunctions.getToken(&line,"}"))
							 {
								valid=true;
								break;
							 }	
							 else
							 if(boost::starts_with(line, "float"))
							 {
								 line.erase(0,5);
								 ConstantInfo info; 
								 info.size = boost::lexical_cast<int>(strFunctions.onlyNextWordAndContinue(&line));
								 info.position = varPosition;
								 varPosition+=info.size;
								 std::string var = strFunctions.onlyNextWordAndContinue(&line);

								 std::string defautValue = strFunctions.onlyNextWordAndContinue(&line);
								 int ii=0;
								 while(!defautValue.empty())
								 {
									 info.defaultValue[ii] = boost::lexical_cast<float>(defautValue);
									 ii++;
									 defautValue = strFunctions.onlyNextWordAndContinue(&line);
								 }

								 shader.perMatConstBufferSize+=info.size;
								 info.size*=4;
								 info.position*=4;
								 shader.perMatVars[var] = info;

								 continue;
							 }
						 }

						 if(!valid) { error=true; continue; }

					 }
					 else
					 error=true;


					 continue;
				 }

				 //ending
				 if(strFunctions.getToken(&line,"}"))
				 {
					valid=true;
					break;
				 }
			 }

			 if(!valid) 
			  error=true;
			 else
			 {
 
				 if(pixel)
					 shds.pixelShaderRefs.insert(std::pair<std::string,shaderRef>(name,shader));
				 else
					 shds.vertexShaderRefs.insert(std::pair<std::string,shaderRef>(name,shader));
			 }
		 }	
	}

	return shds;
}