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

    static bool ReadFile(const std::string& filename, std::string* buf)
    {
        std::ifstream ifs(filename, std::ios::binary);
        if(ifs.is_open() == false)
        {
            ERR_LOG("OPEN FILE %s FAIL!!!", filename.c_str());
            return false;
        }
        size_t sz = 0;
        ifs.seekg(0, ifs.end);
        sz = ifs.tellg();
        ifs.seekg(0, ifs.beg);
        buf->resize(sz);
        ifs.read(&(*buf)[0], sz);
        if(ifs.good() == false)
        {
            ERR_LOG("READ FILE %s FAIL!!!", filename.c_str());
            return false;
        }
        return true;
    }
};