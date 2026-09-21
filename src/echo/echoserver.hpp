#include "../server.hpp"

class EchoServer
{
private:
    TcpServer _server;
private:
    void OnConnected(const PtrConnection& conn)
    {
        DEBUG_LOG("New Connection: %p", conn.get());
    }
    void OnClosed(const PtrConnection& conn)
    {
        DEBUG_LOG("Close Connection: %p", conn.get());
    }
    void OnMessage(const PtrConnection& conn, Buffer* buf)
    {
        conn->Send(buf->ReadPos(), buf->ReadableSize());
        buf->MoveRead(buf->ReadableSize());
        conn->ShutDown();
    }
public:
    EchoServer(int port)
        :_server(port)
    {
        _server.SetThreadCount(2);
        _server.EnableInactiveRelease(10);
        _server.SetConnectedCb([this](const PtrConnection& conn){OnConnected(conn);});
        _server.SetCloseCb([this](const PtrConnection& conn){OnClosed(conn);});
        _server.SetMessageCb([this](const PtrConnection& conn, Buffer* buf){OnMessage(conn, buf);});
    }
    void Start()
    {
        _server.Start();
    }
};