#include "../src/server.hpp"
#include <iostream>

void CloseCb(Channel* channel)
{
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
void EvCb(Channel* channel)
{
    std::cout << "test event callback" << std::endl;
}

void Acceptor(Poller* poller, Channel* listen_channel)
{
    int fd = listen_channel->Fd();
    int newfd = accept(fd, nullptr, nullptr);
    if(newfd < 0) return;
    Channel* conn_channel = new Channel(poller, newfd);
    conn_channel->SetReadCb(std::bind(ReadCb, conn_channel));
    conn_channel->SetWriteCb(std::bind(WriteCb, conn_channel));
    conn_channel->SetCloseCb(std::bind(CloseCb, conn_channel));
    conn_channel->SetErrorCb(std::bind(ErrorCb, conn_channel));
    conn_channel->SetEventCb(std::bind(EvCb, conn_channel));
    conn_channel->EnableRead(); // add connection channel into poller map
}

int main()
{
    Socket svr_sock;
    svr_sock.CreateServer(8080, "0.0.0.0", false);
    Poller poller;
    Channel listen_channel(&poller, svr_sock.Fd());
    listen_channel.SetReadCb(std::bind(Acceptor, &poller, &listen_channel));
    listen_channel.EnableRead();
    while(1)
    {
        std::vector<Channel*> active;
        poller.Poll(active);
        for(auto channel : active)
        {
            channel->HandleEvent();
        }
    }
}