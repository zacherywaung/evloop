#include "../server.hpp"

class Util
{
public:
    // split string src with sep, fill arry with each parts, return number of parts
    static size_t Split(const std::string& src, const std::string& sep, std::vector<std::string>* arry)
    {
        size_t offset = 0;
        while(offset < src.size())
        {
            // abc,def,,,ghi,
            int pos = src.find(sep, offset);
            if(pos == std::string::npos) // no sep after offset, take interval [offset, end())
            {
                arry->push_back(src.substr(offset));
                return arry->size();
            }
            if(pos == offset) // sepsep, skip
            {
                offset += sep.size();
                continue;
            }
            arry->push_back(src.substr(offset, pos - offset));
            offset += sep.size();
        }
        return arry->size();
    }
};