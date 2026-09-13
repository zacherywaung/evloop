#include "../src/server.hpp"
#include <iostream>
#include <unistd.h>

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient(8080, "127.0.0.1");
    for(int i = 0; i < 3; i++)
    {
        std::string msg = "hello";
        cli_sock.Send(msg.c_str(), msg.size());
        char buf[1024] = {0};
        cli_sock.Recv(buf, 1023);
        std::cout << buf << std::endl;
        sleep(2);
    }
    while(1)
    {
        sleep(1);
    }
}