#include "http.hpp"
#include <cstdlib>
#include <iostream>

#define WWWROOT "./wwwroot/"

std::string RequestStr(const HttpRequest& req)
{
    std::stringstream ss;
    ss << req._method << " " << req._path << " " << req._version << "\r\n";
    for(auto& [k, v] : req._params)
    {
        ss << k << ": " << v << "\r\n";
    }
    for(auto& [k, v] : req._headers)
    {
        ss << k << ": " << v << "\r\n";
    }
    ss << "\r\n";
    ss << req._body << "\r\n";
    return ss.str();
}

void Hello(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req), "text/plain");
    // sleep(15);
}
void Login(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req), "text/plain");
}
void PutFile(const HttpRequest& req, HttpResponse* resp)
{
    if(Util::ValidPath(req._path) == false)
    {
        resp->_stat_code = 403; // forbidden
        return;
    }
    std::string path = WWWROOT + req._path;
    if(Util::WriteFile(path, req._body) == false) resp->_stat_code = 500;
    return;
}
void DelFile(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req), "text/plain");
}

int main(int argc, char* argv[])
{
    int threads = (argc > 1) ? std::atoi(argv[1]) : 3;
    HttpServer svr(8080);
    svr.SetBaseDir(WWWROOT);
    svr.SetThreadCount(threads);

    svr.Get("/bench", [](const HttpRequest&, HttpResponse* resp){resp->SetContent("ok", "text/plain");});

    svr.Get("/hello", Hello);
    svr.Post("/login", Login);
    svr.Put("/1234.txt", PutFile);
    svr.Delete("/1234.txt", DelFile);
    svr.Listen();
    return 0;
}
