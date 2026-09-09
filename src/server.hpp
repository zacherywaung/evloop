#pragma once
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

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

class Poller;
using EventCb = std::function<void()>;
class Channel
{
private:
    int _fd;
    Poller* _poller;
    uint32_t _events;
    uint32_t _revents;
    EventCb _read_cb;
    EventCb _write_cb;
    EventCb _error_cb;
    EventCb _close_cb;
    EventCb _event_cb;
public:
    Channel(Poller* poller, int fd)
        :_fd(fd)
        ,_poller(poller)
        ,_events(0)
        ,_revents(0)
    {}
    int Fd() {return _fd;}
    uint32_t Events() {return _events;}
    void SetREvents(uint32_t events) {_revents = events;}

    void SetReadCb(const EventCb& cb)
    {
        _read_cb = cb;
    }
    void SetWriteCb(const EventCb& cb)
    {
        _write_cb = cb;
    }
    void SetErrorCb(const EventCb& cb)
    {
        _error_cb = cb;
    }
    void SetCloseCb(const EventCb& cb)
    {
        _close_cb = cb;
    }
    void SetEventCb(const EventCb& cb)
    {
        _event_cb = cb;
    }
    bool MonitorRead()
    {
        return _events & EPOLLIN;
    }
    bool MonitorWrite()
    {
        return _events & EPOLLOUT;
    }
    void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    }
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    }
    void DisableRead()
    {
        _events &= ~EPOLLIN;
        Update();
    }
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    }
    void DisableAll()
    {
        _events = 0;
        Update();
    }
    void Remove();
    void Update();
    void HandleEvent()
    {
        if(_revents & (EPOLLIN | EPOLLRDHUP | EPOLLPRI))
        {
            if(_read_cb) _read_cb();
        }
        if(_revents & EPOLLOUT)
        {
            if(_write_cb) _write_cb();
        }
        else if(_revents & EPOLLERR)
        {
            if(_error_cb) _error_cb();
        }
        else if(_revents & EPOLLHUP)
        {
            if(_close_cb) _close_cb();
        }
        if(_event_cb) _event_cb();
    }
};

#define DEFAULT_EPOLLEVENTS 1024
class Poller
{
private:
    int _epfd;
    struct epoll_event _evs[DEFAULT_EPOLLEVENTS];
    std::unordered_map<int, Channel*> _channels;
private:
    void Update(Channel* channel, int op)
    {
        // int epoll_ctl(int epfd, int op, int fd, struct epoll_event* pev)
        struct epoll_event ev;
        ev.data.fd = channel->Fd();
        ev.events = channel->Events();
        int ret = epoll_ctl(_epfd, op, channel->Fd(), &ev);
        if(ret < 0)
        {
            ERR_LOG("EPOLLCTL FAIL!!!");
        }
        return;
    }
    bool ChannelExists(Channel* channel)
    {
        auto it = _channels.find(channel->Fd());
        if(it == _channels.end())
        {
            return false;
        }
        return true;
    }
public:
    Poller()
    {
        _epfd = epoll_create(1);
        if(_epfd < 0)
        {
            ERR_LOG("EPOLL CREATE FAIL!!!");
            abort();
        }
        INFO_LOG("EPOLL CREATE SUCCESS");
    }
    void UpdateEvent(Channel* channel)
    {
        if(!ChannelExists(channel))
        {
            _channels[channel->Fd()] = channel;
            Update(channel, EPOLL_CTL_ADD);
        }
        else{
            Update(channel, EPOLL_CTL_MOD);
        }
    }
    void RemoveEvent(Channel* channel)
    {
        if(ChannelExists(channel))
        {
            _channels.erase(channel->Fd());
            Update(channel, EPOLL_CTL_DEL);
        }
    }
    void Poll(std::vector<Channel*>& active)
    {
        // int epoll_wait(int epfd, struct epoll_event* evs, int maxevents, int timeout)
        int nfds = epoll_wait(_epfd, _evs, DEFAULT_EPOLLEVENTS, -1);
        if(nfds < 0)
        {
            if(errno == EINTR)
            {
                return;
            }
            ERR_LOG("EPOLLWAIT FAIL!!!");
            abort();
        }
        for(int i = 0; i < nfds; i++)
        {
            int fd = _evs[i].data.fd;
            auto it = _channels.find(fd);
            assert(it != _channels.end());
            it->second->SetREvents(_evs[i].events);
            active.push_back(it->second);
        }
    }
};

void Channel::Remove() {_poller->RemoveEvent(this);}
void Channel::Update() {_poller->UpdateEvent(this);}