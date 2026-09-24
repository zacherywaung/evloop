#pragma once
#include "../server.hpp"
#include "statu.hpp"
#include "mime.hpp"
#include <fstream>
#include <regex>
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
    // check the path requested is valid (in / dir);
    static bool ValidPath(const std::string& path)
    {
        std::vector<std::string> subdir;
        Split(path, "/", &subdir);
        int level = 0;
        for(auto& e : subdir)
        {
            if(e == "..")
            {
                if(--level < 0) return false;
            }
            else{
                level++;
            }
        }
        return true;
    }
};

class HttpRequest
{
public:
    std::string _method;
    std::string _path;
    std::string _version;
    std::string _body;
    std::smatch _matches;
    std::unordered_map<std::string, std::string> _params;
    std::unordered_map<std::string, std::string> _headers;
public:
    HttpRequest()
        :_version("HTTP/1.1")
    {}
    void Reset()
    {
        _method.clear();
        _path.clear();
        _version = "HTTP/1.1";
        _body.clear();
        std::smatch tmp;
        _matches.swap(tmp);
        _params.clear();
        _header.clear();
    }
    void SetHeader(const std::string& key, const std::string& val)
    {
        _headers.insert({key, val});
    }
    bool HasHeader(const std::string& key) const
    {
        auto it = _headers.find(key);
        if(it == _headers.end())
        {
            return false;
        }
        return true;
    }
    std::string GetHeader(const std::string& key) const
    {
        auto it = _headers.find(key);
        if(it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    void SetParam(const std::string& key, const std::string& val)
    {
        _params.insert({key, val});
    }
    bool HasParam(const std::string& key) const
    {
        auto it = _params.find(key);
        if(it == _params.end())
        {
            return false;
        }
        return true;
    }
    std::string GetParam(const std::string& key) const
    {
        auto it = _params.find(key);
        if(it == _params.end())
        {
            return "";
        }
        return it->second;
    }
    size_t ContentLength() const
    {
        if(HasHeader("Content-Length") == false) return 0;
        std::string len = GetHeader("Content-Length");
        return std::stol(len);
    }
    // check short link 
    bool IsClose() const
    {
        if(HasHeader("Connection") && GetHeader("Connection") == "keep-alive")
        {
            return false;
        }
        return true;
    }
};

class HttpResponse
{
public:
    int _stat_code;
    std::string _body;
    unordered_map<std::string, std::string> _headers;
    bool _redirect;
    std::string _redirect_url;
public:
    HttpResponse()
        :_stat_code(200)
        ,_redirect(false)
    {}
    HttpResponse(int stat_code)
        :_stat_code(stat_code)
        ,_redirect(false)
    {}
    void Reset()
    {
        _stat_code = 200;
        _body.clear();
        _headers.clear();
        _redirect = false;
        _redirect_url.clear();
    }
    void SetHeader(const std::string& key, const std::string& val)
    {
        _headers.insert({key, val});
    }
    bool HasHeader(const std::string& key) const
    {
        auto it = _headers.find(key);
        if(it == _headers.end())
        {
            return false;
        }
        return true;
    }
    std::string GetHeader(const std::string& key) const
    {
        auto it = _headers.find(key);
        if(it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    void SetContent(const std::string& body, const std::string& type)
    {
        _body = body;
        SetHeader("Content-Type", type);
    }
    void SetRedirect(const std::string& redirect_url, int stat_code = 302)
    {
        _redirect_url = redirect_url;
        _redirect = true;
        _stat_code = stat_code;
    }
    // check short link 
    bool IsClose() const
    {
        if(HasHeader("Connection") && GetHeader("Connection") == "keep-alive")
        {
            return false;
        }
        return true;
    }
};

enum class RecvStatu
{
    ERROR,
    LINE,
    HEAD,
    BODY,
    OVER
};

#define MAX_LINE 8192
class HttpContext
{
private:
    RecvStatu _recv_stat;
    int _resp_code;
    HttpRequest _req;
private:
    bool RecvLine(Buffer* buf)
    {
        if(_recv_stat != RecvStatu::LINE) return false;
        std::string line = buf->GetLineAndMove();
        if(line.size() == 0) // not found \r\n
        {
            // check length security
            if(buf->ReadableSize() > MAX_LINE)
            {
                _recv_stat = RecvStatu::ERROR;
                _resp_code = 414; // URI Too Long
                return false;
            }
            return true; // continue for receiving line
        }
        // check length security
        if(line.size() > MAX_LINE)
        {
            _recv_stat = RecvStatu::ERROR;
            _resp_code = 414; // URI Too Long
            return false;
        }
        if(ParseLine(line) == false) return false;
        // success, move to next statu
        _recv_stat = RecvStatu::HEAD;
        return true;
    }

    // POST /api/login?from=home&username=alice HTTP/1.1
    bool ParseLine(const std::string& line)
    {
        std::smatch matches;
        static const std::regex e(
            "(GET|HEAD|POST|PUT|DELETE)"    // 1.method
            " "                             
            "([^?]*)"                       // 2.path
            "(?:\\?(.*))?"                  // 3.query
            " "
            "(HTTP/1\\.[01])"               // 4.version
            "(?:\n|\r\n)?",
            std::regex::icase
        );
        bool ret = std::regex_match(line, matches, e);
        if(ret == false)
        {
            _recv_stat = RecvStatu::ERROR;
            _resp_code = 400; //Bad Request
            return false;
        }
        _req._method = matches[1];                          // 1.method
        _req._path = Util::UrlDecode(matches[2], false);    // 2.path
        _req._version = matches[4];                         // 4.version
        std::string query_string = matches[3];              // 3.query->request_params
        std::vector<std::string> query_string_arry;
        Util::Split(query_string, "&", &query_string_arry);
        for(auto& kv : query_string_arry)
        {
            size_t pos = kv.find('=');
            if(pos == std::string::npos)
            {
                _recv_stat = RecvStatu::ERROR;
                _resp_code = 400; //Bad Request
                return false;
            }
            std::string key = Util::UrlDecode(kv.substr(0, pos), true);
            std::string val = Util::UrlDecode(kv.substr(pos + 1), true);
            _req.SetParam(key, val);
        }
        return true;
    }
    bool RecvHead(Buffer* buf)
    {
        if(_recv_stat != RecvStatu::HEAD) return false;
        // key: val\r\nkey: val\r\n
        while(1)
        {
            std::string line = buf->GetLineAndMove();
            if(line.size() == 0)
            {
                if(buf->ReadableSize() > MAX_LINE)
                {
                    _recv_stat = RecvStatu::ERROR;
                    _resp_code = 414; // URI Too Long
                    return false;
                }
                return true;
            }
            if(line.size() > MAX_LINE)
            {
                _recv_stat = RecvStatu::ERROR;
                _resp_code = 414; // URI Too Long
                return false;
            }
            if(line == "\n" || line == "\r\n")
            {
                break;
            }
            if(ParseHead(line) == false) return false;
        }
        _recv_stat = RecvStatu::BODY;
        return true;
    }
    bool ParseHead(std::string& line)
    {
        if(line.back() == '\n') line.pop_back();
        if(line.back() == '\r') line.pop_back();
        size_t pos = line.find(": ");
        if(pos = std::string::npos)
        {
            _recv_stat = RecvStatu::ERROR;
            _resp_code = 400; // Bad Request
            return false;
        }
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 2);
        _req.SetHeader(key, val);
        return true;
    }

public:
    HttpContext()
        :_recv_stat(RecvStatu::LINE)
        ,_resp_code(200)
    {}
    void Reset()
    {
        _recv_stat = RecvStatu::LINE;
        _resp_code = 200;
        _req.Reset();
    }
};