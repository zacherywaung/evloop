#include "../src/server.hpp"
#include <iostream>
#include <unordered_map>

uint64_t conn_id = 0;
std::unordered_map<uint64_t, PtrConnection> _conns;

void ConnectionDestroy(const PtrConnection& conn)
{
    _conns.erase(conn->Id());
}
void OnConnected(const PtrConnection& conn)
{
    DEBUG_LOG("new connection: %p", conn.get());
}
void OnMessage(const PtrConnection& conn, Buffer* buf)
{
    std::string msg = buf->ReadAsStringAndMove(buf->ReadableSize());
    DEBUG_LOG("%s", msg.c_str());
    std::string s = "hello";
    conn->Send(s.c_str(), s.size());
}

void Acceptor(EventLoop* loop, Channel* listen_channel)
{
    int fd = listen_channel->Fd();
    int newfd = accept(fd, nullptr, nullptr);
    if(newfd < 0) return;
    conn_id++;
    PtrConnection conn(new Connection(conn_id, newfd, loop));
    conn->SetMessageCb(std::bind(OnMessage, std::placeholders::_1, std::placeholders::_2));
    conn->SetConnectedCb(std::bind(OnConnected, std::placeholders::_1));
    conn->SetServerCloseCb(std::bind(ConnectionDestroy, std::placeholders::_1));
    conn->RegisterInactiveRelease(10);
    conn->Established();
    _conns[conn->Id()] = conn;
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