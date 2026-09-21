#include "echoserver.hpp"

int main()
{
    EchoServer svr(8080);
    svr.Start();
    return 0;
}