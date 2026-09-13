#include "../src/server.hpp"
#include <iostream>

void CloseCb(Channel* channel)
{
    DEBUG_LOG("close fd: %d", channel->Fd());
    channel->Remove(); // clean epoll and clear mp
    delete channel;
}

void ReadCb(Channel* channel)
{
    int fd = channel->Fd();
    char buffer[1024] = {0};
    int ret = recv(fd, buffer, 1023, 0);
    if(ret < 0)
    {
        CloseCb(channel);
    }
    std::cout << buffer << std::endl;
    channel->EnableWrite();
}
void WriteCb(Channel* channel)
{
    int fd = channel->Fd();
    std::string tmp = "test for writecallback";
    int ret = send(fd, tmp.c_str(), tmp.size(), 0);
    if(ret < 0)
    {
        CloseCb(channel);
    }
    channel->DisableWrite();
}
void ErrorCb(Channel* channel)
{
    CloseCb(channel);
}
void EvCb(EventLoop* loop, Channel* channel, uint64_t id)
{
    loop->RefreshTask(id);
}

void Acceptor(EventLoop* loop, Channel* listen_channel)
{
    int fd = listen_channel->Fd();
    int newfd = accept(fd, nullptr, nullptr);
    if(newfd < 0) return;
    Channel* conn_channel = new Channel(loop, newfd);
    uint64_t taskid = rand() % 10000;
    conn_channel->SetReadCb(std::bind(ReadCb, conn_channel));
    conn_channel->SetWriteCb(std::bind(WriteCb, conn_channel));
    conn_channel->SetCloseCb(std::bind(CloseCb, conn_channel));
    conn_channel->SetErrorCb(std::bind(ErrorCb, conn_channel));
    conn_channel->SetEventCb(std::bind(EvCb, loop, conn_channel, taskid));
    loop->AddTask(taskid, 5, [conn_channel](){CloseCb(conn_channel);});
    conn_channel->EnableRead(); // add connection channel into poller map
}

int main()
{
    srand(time(nullptr));
    Socket svr_sock;
    svr_sock.CreateServer(8080, "0.0.0.0", false);
    EventLoop loop;
    Channel listen_channel(&loop, svr_sock.Fd());
    listen_channel.SetReadCb(std::bind(Acceptor, &loop, &listen_channel));
    listen_channel.EnableRead();
    while(1)
    {
        loop.Start();
    }
    return 0;
}