#include "main_poolminer.h"
#include "xpmclient.h"

#include <map>
#include <string>
#include <cmath>
#include <stdint.h>
#include <stdarg.h>

bool fDebug;
unsigned int pool_share_minimum; //init me
CBlockIndex* pindexBest;
std::map<std::string,std::string> mapArgs;

#if defined(__GNUG__) && !defined(__MINGW32__) && !defined(__MINGW64__)
int64
_atoi64 (const char *nptr)
{
   int c;
   int64 value;
   int sign;

   while (isspace((int)*nptr))
        ++nptr;

   c = (int)*nptr++;
   sign = c;
   if (c == '-' || c == '+')
        c = (int)*nptr++;

   value = 0;

   while (isdigit(c))
     {
        value = 10 * value + (c - '0');
        c = (int)*nptr++;
     }

   if (sign == '-')
       return -value;
   else
       return value;
}
#endif

std::string GetArg(const std::string& strArg, const std::string& strDefault)
{
    if (mapArgs.count(strArg))
        return mapArgs[strArg];
    return strDefault;
}

int64 GetArg(const std::string& strArg, int64 nDefault)
{
    if (mapArgs.count(strArg))
        return _atoi64(mapArgs[strArg].c_str());
    return nDefault;
}

bool GetBoolArg(const std::string& strArg, bool fDefault)
{
    if (mapArgs.count(strArg))
    {
        if (mapArgs[strArg].empty())
            return true;
        return (atoi(mapArgs[strArg].c_str()) != 0);
    }
    return fDefault;
}

void ParseParameters(int argc, const char* const argv[])
{
    //mapArgs.clear();
    for (int i = 1; i < argc; i++)
    {
        std::string str(argv[i]);
        std::string strValue;
        size_t is_index = str.find('=');
        if (is_index != std::string::npos)
        {
            strValue = str.substr(is_index+1);
            str = str.substr(0, is_index);
        }
        if (str[0] != '-')
            break;

        mapArgs[str] = strValue;
    }
}

#include <fstream>
void ParseConfigFile(const char* file_name) {
	std::ifstream cfg_file(file_name);
	if (!cfg_file) return;
	std::string cfg_line;
	while (std::getline(cfg_file,cfg_line)) {
		std::istringstream cfg_line_is(cfg_line);
		std::string key;
		if (std::getline(cfg_line_is,key,'=')) {
			std::string val;
			if (std::getline(cfg_line_is,val)) {
				mapArgs[std::string("-")+key] = val;
			}
		}
	}
}
