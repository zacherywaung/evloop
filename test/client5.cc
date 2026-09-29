/* pipelining test1 */
/* client5 send 3 compelete GET requests together */
/* Expected: each request get corresponding response correctly */
#include "../src/server.hpp"
#include <unistd.h>

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient(8080, "127.0.0.1");
    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    req += "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    req += "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    while(1)
    {
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        char buffer[1024] = {0};
        assert(cli_sock.Recv(buffer, 1023));
        DEBUG_LOG("[%s]", buffer);
        sleep(8);
    }
    return 0;
}