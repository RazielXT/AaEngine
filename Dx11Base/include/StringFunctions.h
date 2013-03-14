#ifndef _AA_STRING_FUN_
#define _AA_STRING_FUN_

#include <string>
#include <boost/algorithm/string/predicate.hpp>


class StringFunctions
{
public:

int i;

std::string toNextWord(std::string line)
{
	if(i<0) i=0;

	i++;
	if(i==107)
		i=5;

	std::string word;

	if(line.empty())
		return word;

	while(!line.empty() && ( line[0]==' ' || line[0]=='\t'))
		line.erase(0,1);

	if(line.empty())
		return word;

	word=line;
	return word;
}

std::string onlyNextWord(std::string line)
{
	std::string word;

	if(line.empty())
		return word;

	while(!line.empty() && (line[0]==' ' || line[0]=='\t'))
		line.erase(0,1);

	if(line.empty())
		return word;

	int i;
	for(i=0;i<line.length();i++)
	{
		if(line[i]==' ' || line[i]=='\t' || line[i]=='\n')
			break;
	}

	word=line;
	word.erase(i,word.length()-i);
	return word;
}

std::string onlyNextWordToSeparatorAndContinue(std::string* line,char separator)
{
	std::string word;

	if(line->empty())
		return word;

	while(!line->empty() && ((*line)[0]==' ' || (*line)[0]=='\t'))
		line->erase(0,1);

	if(line->empty())
		return word;

	int i;
	for(i=0;i<line->length();i++)
	{
		if((*line)[i]==' ' || (*line)[i]=='\t' || (*line)[i]=='\n' || (*line)[i]==separator)
			break;
	}

	word=*line;
	word.erase(i,word.length()-i);
	line->erase(0,i);

	if((*line)[0]==separator)
	line->erase(0,1);

	return word;
}

std::string onlyNextWordAndContinue(std::string* line)
{
	std::string word;

	if(line->empty())
		return word;

	while(!line->empty() && ((*line)[0]==' ' || (*line)[0]=='\t'))
		line->erase(0,1);

	if(line->empty())
		return word;

	int i;
	for(i=0;i<line->length();i++)
	{
		if((*line)[i]==' ' || (*line)[i]=='\t' || (*line)[i]=='\n')
			break;
	}

	word=*line;
	word.erase(i,word.length()-i);
	line->erase(0,i);

	return word;
}


bool getToken(std::string* line,std::string token)
{
	*line=toNextWord(*line);

	 if (boost::starts_with(*line, token))
	 {
		 line->erase(0,token.length());
		 return true;
	 }

	 return false;
}

};

#endif