#pragma once
#include "../server.hpp"
#include "statu.hpp"
#include "mime.hpp"
#include <fstream>
#include <sys/stat.h>

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
    // read content from filename, fill buf
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
    // write content from src into filename
    static bool WriteFile(const std::string& filename, const std::string& src)
    {
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
        if(ofs.is_open() == false)
        {
            ERR_LOG("OPEN FILE %s FAIL!!!", filename.c_str());
            return false;
        }
        ofs.write(src.c_str(), src.size());
        if(ofs.good() == false)
        {
            ERR_LOG("Write FILE %s FAIL!!!", filename.c_str());
            return false;
        }
        return true;
    }
    // encode url, return result
    static std::string UrlEncode(const std::string& url, bool space_to_plus)
    {
        std::string ret;
        for(auto& c : url)
        {
            if(c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c))
            {
                ret += c;
            }
            else if(c == ' ' && space_to_plus)
            {
                ret += '+';
            }
            else{
                char tmp[4] = {0};
                snprintf(tmp, 4, "%%%02X", c);
                ret += tmp;
            }
        }
        return ret;
    }
    static int HexToDec(char c)
    {
        if(c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if(c >= 'a' && c <= 'z')
        {
            return c - 'a' + 10;
        }
        if(c >= 'A' && c <= 'Z')
        {
            return c - 'A' + 10;
        }
        else return -1;
    }
    // decode url, return result
    static std::string UrlDecode(const std::string& url, bool plus_to_space)
    {
        std::string ret;
        for(int i = 0; i < url.size(); i++)
        {
            if(url[i] == '+' && plus_to_space)
            {
                ret += ' ';
            }
            else if(url[i] == '%' && i + 2 < url.size())
            {
                int v1 = HexToDec(url[i + 1]);
                int v2 = HexToDec(url[i + 2]);
                if(v1 >= 0 && v2 >= 0)
                {
                    ret += static_cast<char>(v1 * 16 + v2);
                    i += 2;
                }
            }
            else{
                ret += url[i];
            }
        }
        return ret;
    }
    // describe a http status code
    static std::string StatusDesc(int statu)
    {
        auto it = statu_msg.find(statu);
        if(it != statu_msg.end())
        {
            return it->second;
        }
        return "Unknow";
    }
    // get media type from extend filename
    static std::string ExtToMime(const std::string& filename)
    {
        size_t pos = filename.find_last_of('.');
        if(pos == std::string::npos)
        {
            return "application/octet-stream";
        }
        std::string ext = filename.substr(pos);
        auto it = mime_msg.find(ext);
        if(it == mime_msg.end())
        {
            return "application/octet-stream";
        }
        return it->second;
    }
    // check a file is directory
    static bool IsDirectory(const std::string& filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if(ret < 0) return false;
        return S_ISDIR(st.st_mode);
    }
    // check a file is regular file
    static bool IsRegular(const std::string& filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if(ret < 0) return false;
        return S_ISREG(st.st_mode);
    }
};