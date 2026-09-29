#include "http.hpp"
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
    resp->SetContent(RequestStr(req), "text/plain");
}
void DelFile(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req), "text/plain");
}

int main()
{
    HttpServer svr(8080);
    svr.SetBaseDir(WWWROOT);
    svr.SetThreadCount(3);
    svr.Get("/hello", Hello);
    svr.Post("/login", Login);
    svr.Put("/1234.txt", PutFile);
    svr.Delete("/1234.txt", DelFile);
    svr.Listen();
    return 0;
}
