/* long-connection test2 */
/* client1 send GET request every 15 seconds */
/* Expected: 1.server log demonstrate connection created and release after first response 10 seconds */
/* Expected: 2.client log demonstrate full response received once and failed next time */
#include "../src/server.hpp"
#include <unistd.h>

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient(8080, "127.0.0.1");
    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    while(1)
    {
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        char buffer[1024] = {0};
        assert(cli_sock.Recv(buffer, 1023));
        DEBUG_LOG("[%s]", buffer);
        sleep(15);
    }
    return 0;
}