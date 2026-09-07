#include "../src/server.hpp"

int main()
{
    Socket svr_sock;
    svr_sock.CreateServer(8080, "0.0.0.0", false);
    while(1)
    {
     
        int newfd = svr_sock.Accept();
        if(newfd < 0) continue;
        Socket cli(newfd);
        char buf[1024] = {0};
        int ret = cli.Recv(buf, 1023);
        if(ret < 0)
        {
            cli.Close();
            continue;
        }
        cli.Send(buf, ret);
    }
}