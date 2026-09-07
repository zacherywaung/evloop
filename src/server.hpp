#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "log.hpp"

#define DEFAULT_SIZE 1024
class Buffer
{
private:
    std::vector<char> _buffer;
    uint64_t _ridx;
    uint64_t _widx;
public:
    Buffer()
        :_buffer(DEFAULT_SIZE)
        ,_ridx(0)
        ,_widx(0)
    {}
    char* GetBegin()
    {
        return &*_buffer.begin();
    }
    char* WritePos()
    {
        return GetBegin() + _widx;
    }
    char* ReadPos()
    {
        return GetBegin() + _ridx;
    }
    uint64_t HeadIdleSize()
    {
        return _ridx;
    }
    uint64_t TailIdleSize()
    {
        return _buffer.size() - _widx;
    }
    uint64_t ReadableSize()
    {
        return _widx - _ridx;
    }
    void MoveRead(uint64_t len)
    {
        assert(len <= ReadableSize());
        _ridx += len;
    }
    void MoveWrite(uint64_t len)
    {
        assert(len <= TailIdleSize());
        _widx += len;
    }
    void EnsureWriteSpace(uint64_t len)
    {
        if(len <= TailIdleSize()) return;
        if(len <= HeadIdleSize() + TailIdleSize())
        {
            uint64_t rsz = ReadableSize();
            std::copy(ReadPos(), ReadPos() + rsz, GetBegin());
            _ridx = 0;
            _widx = rsz;
        }
        else{
            _buffer.resize(_widx + len);
        }
    }
    // Write
    void Write(const void* data, uint64_t len)
    {
        if(len == 0) return;
        EnsureWriteSpace(len);
        const char* d = static_cast<const char*>(data);
        std::copy(d, d + len, WritePos());
    }
    void WriteAndMove(const void* data, uint64_t len)
    {
        Write(data, len);
        MoveWrite(len);
    }
    void WriteString(const std::string& data)
    {
        Write(data.c_str(), data.size());
    }
    void WriteAndMoveString(const std::string& data)
    {
        WriteString(data);
        MoveWrite(data.size());
    }
    void WriteBuffer(Buffer& buf)
    {
        Write(buf.ReadPos(), buf.ReadableSize());
    }
    void WriteAndMoveBuffer(Buffer& buf)
    {
        WriteBuffer(buf);
        MoveWrite(buf.ReadableSize());
    }
    // Read
    void Read(void* buf, uint64_t len)
    {
        assert(len <= ReadableSize());
        std::copy(ReadPos(), ReadPos() + len, static_cast<char*>(buf));
    }
    void ReadAndMove(void* buf, uint64_t len)
    {
        Read(buf, len);
        MoveRead(len);
    }
    std::string ReadAsString(uint64_t len)
    {
        assert(len <= ReadableSize());
        std::string ret;
        ret.resize(len);
        Read(&ret[0], len);
        return ret;
    }
    std::string ReadAsStringAndMove(uint64_t len)
    {
        assert(len <= ReadableSize());
        std::string ret = ReadAsString(len);
        MoveRead(len);
        return ret;
    }
    char* FindLF()
    {
        char* ret = static_cast<char*>(memchr(ReadPos(), '\n', ReadableSize()));
        return ret;
    }
    std::string GetLine()
    {
        char* pos = FindLF();
        if(pos == nullptr) return "";
        return ReadAsString(pos - ReadPos() + 1);
    }
    std::string GetLineAndMove()
    {
        std::string ret = GetLine();
        MoveRead(ret.size());
        return ret;
    }
    void Clear()
    {
        _widx = 0;
        _ridx = 0;
    }
};

#define DEFAULT_LISTEN_BACKLOG 1024
class Socket
{
private:
    int _sockfd;
public:
    Socket()
        :_sockfd(-1)
    {}
    Socket(int fd)
        :_sockfd(fd)
    {}
    int Fd() {return _sockfd;}
    bool Create()
    {
        // int socket(int domain, int type, int ptotocol)
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(_sockfd < 0)
        {
            ERR_LOG("SOCKET CREATE FAIL!!!");
            return false;
        }
        INFO_LOG("SOCKET CREATE SUCCESS");
        return true;
    }
    bool Bind(const std::string& ip, uint16_t port)
    {
        // int bind(int fd, const sockaddr*, socklen_t)
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        // int inet_pton(int domain, const char* src, void* dst)
        if(inet_pton(AF_INET, ip.c_str(), (void*)&addr.sin_addr) != 1)
        {
            ERR_LOG("Invalid IP Address: %s", ip.c_str());
            return false;
        }
        socklen_t len = sizeof(struct sockaddr_in);
        int ret = bind(_sockfd, (struct sockaddr*)&addr, len);
        if(ret < 0)
        {
            ERR_LOG("BIND FAIL!!!");
            return false;
        }
        INFO_LOG("BIND SUCCESS");
        return true;
    }
    bool Listen(int backlog = DEFAULT_LISTEN_BACKLOG)
    {
        // int listen(int fd, int backlog)
        int ret = listen(_sockfd, backlog);
        if(ret < 0)
        {
            ERR_LOG("LISTEN FAIL!!!");
            return false;
        }
        INFO_LOG("LISTEN SUCCESS");
        return true;
    }
    bool Connect(const std::string& ip, uint16_t port)
    {
        // int connect(int fd, const sockaddr*, socklen_t)
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if(inet_pton(AF_INET, ip.c_str(), (void*)&addr.sin_addr) != 1)
        {
            ERR_LOG("Invalid IP Address: %s", ip.c_str());
            return false;
        }
        socklen_t len = sizeof(struct sockaddr_in);
        int ret = connect(_sockfd, (struct sockaddr*)&addr, len);
        if(ret < 0)
        {
            ERR_LOG("CONNECT FAIL!!!");
            return false;
        }
        INFO_LOG("CONNECT SUCCESS");
        return true;
    }
    int Accept()
    {
        // int accept(int listenfd, const sockaddr*, socklen_t)
        int newfd = accept(_sockfd, nullptr, nullptr);
        if(newfd < 0)
        {
            ERR_LOG("ACCEPT FAIL!!!");
            return -1;
        }
        return newfd;
    }
    ssize_t Recv(void* buf, int len, int flag = 0)
    {
        // ssize_t recv(int fd, void* buf, int len, int flag)
        ssize_t ret = recv(_sockfd, buf, len, flag);
        if(ret <= 0)
        {
            if(errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("RECV FAIL!!!");
            return -1;
        }
        INFO_LOG("Recv success, length: %zd", ret);
        return ret;
    }
    ssize_t RecvNonBlock(void* buf, int len)
    {
        return Recv(buf, len, MSG_DONTWAIT);
    }
    ssize_t Send(const void* buf, int len, int flag = 0)
    {
        // ssize_t send(int fd, const void* buf, int len, int flag)
        ssize_t ret = send(_sockfd, buf, len, flag);
        if(ret < 0)
        {
            if(errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("SEND FAIL!!!");
            return -1;
        }
        INFO_LOG("Send success, length: %zd", ret);
        return ret;
    }
    ssize_t SendNonBlock(void* buf, int len)
    {
        return Send(buf, len, MSG_DONTWAIT);
    }
    void Close()
    {
        if(_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }
    bool CreateServer(uint16_t port, const std::string& ip = "0.0.0.0", bool isnonblock = true)
    {
        // socket, set nonblock, bind, listen, reuse address
        if(Create() == false) return false;
        if(isnonblock) SetNonBlock();
        if(Bind(ip, port) == false) return false;
        if(Listen() == false) return false;
        ReuseAddress();
        return true;
    }
    bool CreateClient(uint16_t port, const std::string& ip)
    {
        // socket, connect
        if(Create() == false) return false;
        if(Connect(ip, port) == false) return false;
        return true;
    }
    void SetNonBlock()
    {
        // int fcntl(int fd, int cmd, ...arg)
        int flag = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }
    void ReuseAddress()
    {
        // int setsockopt(int fd, int level, int optname, const void* val. socklen_t len)
        int val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&val, sizeof(int));
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (const void*)&val, sizeof(int));
    }
    ~Socket()
    {
        Close();
    }

};