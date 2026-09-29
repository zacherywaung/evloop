/* deferred desturction test1 */
/* fork 10 child process for client sending request, assuming bussiness logic "hello" take 15 seconds */
/* fd1: connection fd2: timer fd3: connection fd4: connection */
/* fd3, fd4 should be excuted even if timeout, indicating connection release should be push in queue */
/*
void Start() {
    while(1) {
        _poller.Poll(actives);                          
        for (auto ch : actives) ch->HandleEvent();      // excute bussiness logic first
        RunAllTasks();                                  // release() with queue order
    }
}
*/
/* Expected: The server does NOT crash. Release queues ReleaseInLoop rather than destroying inline,
so the actives array stays valid for the rest of the dispatch loop; the objects die later in RunAllTasks. */

#include "../src/server.hpp"
#include <unistd.h>

int main()
{
    signal(SIGCHLD, SIG_IGN);
    for(int i = 0; i < 10; i++)
    {
        pid_t id = fork();
        if(id < 0)
        {
            ERR_LOG("FORK ERR!!!");
            return -1;
        }
        if(id == 0) // child
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
            }
            exit(0);
        }
    }
    while(1) sleep(1);
    return 0;
}