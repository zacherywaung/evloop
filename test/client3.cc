/* long-connection test3 */
/* client3 send GET request every 3 seconds, but Content-Length & body not match */
/* Expected: 1.send once, server think request is not completed, no resp, waiting until inactive release */
/* Expected: 2.send three times, server think the second and thrid(part of) request is first req's body, unable to parse the following request, return 400 */
#include "../src/server.hpp"
#include <unistd.h>

#define SLEEPTIME_1 15
#define SLEEPTIME_2 3

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient(8080, "127.0.0.1");
    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 100\r\n\r\n123456";
    while(1)
    {
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        char buffer[1024] = {0};
        assert(cli_sock.Recv(buffer, 1023));
        DEBUG_LOG("[%s]", buffer);
        sleep(SLEEPTIME_2);
    }
    return 0;
}