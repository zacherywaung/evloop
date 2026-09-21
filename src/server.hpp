#pragma once
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <any>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

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
        if(ret == 0) return -2; // errcode -2 : peer close
        if(ret < 0)
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
        if(len == 0) return 0;
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
        if(len == 0) return 0;
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
        ReuseAddress();
        if(Bind(ip, port) == false) return false;
        if(Listen() == false) return false;
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

class EventLoop;
using EventCb = std::function<void()>;
class Channel
{
private:
    int _fd;
    EventLoop* _loop;
    uint32_t _events;
    uint32_t _revents;
    EventCb _read_cb;
    EventCb _write_cb;
    EventCb _error_cb;
    EventCb _close_cb;
    EventCb _event_cb;
public:
    Channel(EventLoop* loop, int fd)
        :_fd(fd)
        ,_loop(loop)
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
            DEBUG_LOG("epoll ready fd=%d events=%u", fd, _evs[i].events);
            auto it = _channels.find(fd);
            assert(it != _channels.end());
            it->second->SetREvents(_evs[i].events);
            active.push_back(it->second);
        }
    }
};

using TaskCb = std::function<void()>;
using ReleaseCb = std::function<void()>;

class Timertask
{
private:
    uint64_t _taskid;
    uint32_t _timeout;
    bool _iscanceled;
    TaskCb _tcb;
    ReleaseCb _rcb;
public:
    Timertask(uint64_t id, uint32_t timeout, const TaskCb& tcb)
        :_taskid(id)
        ,_timeout(timeout)
        ,_tcb(tcb)
        ,_iscanceled(false)
    {}
    void SetRelease(const ReleaseCb& rcb)
    {
        _rcb = rcb;
    }
    uint32_t GetTimeout()
    {
        return _timeout;
    }
    void Cancel()
    {
        _iscanceled = true;
    }
    ~Timertask()
    {
        if(!_iscanceled) _tcb();
        _rcb();
    }
};

using PtrTask = std::shared_ptr<Timertask>;
using WeakTask = std::weak_ptr<Timertask>;

class TimerWheel
{
private:
    int _tick;
    int _capacity;
    std::vector<std::vector<PtrTask>> _wheel;
    std::unordered_map<uint64_t, WeakTask> _mp;

    EventLoop* _loop;
    int _timerfd;
    std::unique_ptr<Channel> _timer_channel;
private:
    void RemoveTask(uint64_t id)
    {
        auto it = _mp.find(id);
        if(it != _mp.end())
        {
            _mp.erase(it);
        }
    }
    static int CreateTimerFd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if(timerfd < 0)
        {
            ERR_LOG("CREATE TIMERFD FAIL!!!");
            abort();
        }
        struct itimerspec itime;
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0;
        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0;
        timerfd_settime(timerfd, 0, &itime, nullptr);
        return timerfd;
    }
    void ReadTimeFd()
    {
        uint64_t times;
        int ret = read(_timerfd, &times, sizeof(times));
        if(ret < 0)
        {
            ERR_LOG("READTIMEFD FAIL!!!");
            abort();
        }
    }
    void Ontime()
    {
        ReadTimeFd();
        Run();
    }
public:
    TimerWheel(EventLoop* loop)
        :_capacity(60)
        ,_wheel(_capacity)
        ,_tick(0)
        ,_loop(loop)
        ,_timerfd(CreateTimerFd())
        ,_timer_channel(new Channel(loop, _timerfd))
    {
        DEBUG_LOG("timerfd = %d", _timerfd);
        _timer_channel->SetReadCb([this](){Ontime();});
        _timer_channel->EnableRead();
    }
    void AddTaskInLoop(uint64_t id, uint32_t timeout, const TaskCb& tcb)
    {
        PtrTask pt(new Timertask(id, timeout, tcb));
        // pt->SetRelease(std::bind(&TimerWheel::RemoveTask, this, id));
        pt->SetRelease([this, id](){RemoveTask(id);});
        int pos = (_tick + timeout) % _capacity;
        _wheel[pos].push_back(pt);
        _mp[id] = WeakTask(pt);
    }
    void AddTask(uint64_t id, uint32_t timeout, const TaskCb& tcb);
    void RefreshTaskInLoop(uint64_t id)
    {
        auto it = _mp.find(id);
        if(it == _mp.end())
            return;
        PtrTask pt = it->second.lock();
        if(!pt) return;
        int timeout = pt->GetTimeout();
        int pos = (_tick + timeout) % _capacity;
        _wheel[pos].push_back(pt);
    }
    void RefreshTask(uint64_t id);
    void CancelInLoop(uint64_t id)
    {
        auto it = _mp.find(id);
        if(it == _mp.end())
            return;
        PtrTask pt = it->second.lock();
        if(!pt) return;
        pt->Cancel();
    }
    void Cancel(uint64_t id);
    void Run()
    {
        _tick = (_tick + 1) % _capacity;
        DEBUG_LOG("tick=%d slot_size=%zu", _tick, _wheel[_tick].size());
        _wheel[_tick].clear();
    }
    bool HasTimerTask(uint64_t id)
    {
        auto it = _mp.find(id);
        if(it == _mp.end())
        {
            return false;
        }
        return true;
    }
};


using Functor = std::function<void()>;
class EventLoop
{
private:
    std::thread::id _threadid;
    int _event_fd;
    std::unique_ptr<Channel> _event_channel;
    Poller _poller;
    std::vector<Functor> _tasks;
    std::mutex _mutex;
    TimerWheel _wheel;
private:
    static int CreateEventFd()
    {
        // int eventfd(unsigned int val, int flags)
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if(efd < 0)
        {
            ERR_LOG("CREATE EVENTFD FAIL!!!");
            abort();
        }
        return efd;
    }
    void EventFdReadCb()
    {
        uint64_t res = 0;
        int ret = read(_event_fd, &res, sizeof(res));
        if(ret < 0)
        {
            if(errno == EAGAIN || errno == EINTR)
            {
                return;
            }
            ERR_LOG("EVENTFD READ FAIL!!!");
            abort();
        }
    }
    void WakeUpEventFd()
    {
        uint64_t val = 1;
        int ret = write(_event_fd, &val, sizeof(val));
        if(ret < 0)
        {
            if(errno == EINTR)
            {
                return;
            }
            ERR_LOG("EVENTFD WRITE FAIL!!!");
            abort();
        }
    }
    void RunAllTasks()
    {
        std::vector<Functor> local;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.swap(local);
        }
        for(auto& f : local)
        {
            f();
        }
    }
public:
    EventLoop()
        :_threadid(std::this_thread::get_id())
        ,_event_fd(CreateEventFd())
        ,_event_channel(new Channel(this, _event_fd))
        ,_wheel(this)
    {
        // _event_channel->SetReadCb(std::bind(&EventLoop::EventFdReadCb, this));
        _event_channel->SetReadCb([this](){EventFdReadCb();});
        _event_channel->EnableRead();
    }
    void Start()
    {
        while(1)
        {
            // epoll
            std::vector<Channel*> actives;
            _poller.Poll(actives);
            // excute active events
            for(auto channel : actives)
            {
                channel->HandleEvent();
            }
            // excute all tasks
            RunAllTasks();
        }
    }
    bool IsInLoop()
    {
        return _threadid == std::this_thread::get_id();
    }
    void AssertInLoop()
    {
        assert(_threadid == std::this_thread::get_id());
    }
    void RunInLoop(const Functor& cb) // ensure callback is excuted in same thread
    {
        if(IsInLoop()) cb();
        else{
            PushInLoop(cb);
        }
    }
    void PushInLoop(const Functor& cb)
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.push_back(cb);
        }
        WakeUpEventFd();
    }
    void UpdateEvent(Channel* channel) {_poller.UpdateEvent(channel);}
    void RemoveEvent(Channel* channel) {_poller.RemoveEvent(channel);}
    void AddTask(uint64_t id, uint32_t timeout, const TaskCb& tcb)
    {
        _wheel.AddTask(id, timeout, tcb);
    }
    void RefreshTask(uint64_t id)
    {
        _wheel.RefreshTask(id);
    }
    void CancelTask(uint64_t id)
    {
        _wheel.Cancel(id);
    }
    bool HasTimerTask(uint64_t id)
    {
        return _wheel.HasTimerTask(id);
    }

};


class LoopThread
{
private:
    std::mutex _mutex;
    std::condition_variable _cond;
    EventLoop* _loop;
    std::thread _thread;
private:
    void ThreadEntry()
    {
        EventLoop loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _loop = &loop;
            _cond.notify_all();
        }
        _loop->Start();
    }
public:
    LoopThread()
        :_loop(nullptr)
        ,_thread(&LoopThread::ThreadEntry, this)
    {}
    EventLoop* GetLoop()
    {
        EventLoop* ret = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [&](){ return _loop != nullptr;});
            ret = _loop;
        }
        return ret;
    }
};

class LoopThreadPool
{
private:
    int _thread_cnt;
    int _next_idx;
    EventLoop* _baseloop;
    std::vector<LoopThread*> _threads;
    std::vector<EventLoop*> _loops;
public:
    LoopThreadPool(EventLoop* baseloop)
        :_thread_cnt(0)
        ,_next_idx(0)
        ,_baseloop(baseloop)
    {}
    void SetThreadCnt(int cnt)
    {
        _thread_cnt = cnt;
    }
    void CreateLoops()
    {
        if(_thread_cnt > 0)
        {
            _threads.resize(_thread_cnt);
            _loops.resize(_thread_cnt);
            for(int i = 0; i < _thread_cnt; i++)
            {
                _threads[i] = new LoopThread();
                _loops[i] = _threads[i]->GetLoop();
            }
        }
    }
    EventLoop* NextLoop()
    {
        if(_thread_cnt == 0) return _baseloop;
        _next_idx = (_next_idx + 1) % _thread_cnt;
        return _loops[_next_idx];
    }
};

enum class ConnStatus
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};
class Connection;
using PtrConnection = std::shared_ptr<Connection>;
class Connection : public std::enable_shared_from_this<Connection>
{
private:
    uint64_t _conn_id; // connection id & inactive release task id
    int _sockfd;
    bool _enable_inactive_release;
    EventLoop* _loop;
    ConnStatus _stat;
    Socket _socket;
    Channel _channel;
    Buffer _in_buffer;
    Buffer _out_buffer;
    std::any _context;

    using ConnectedCb = std::function<void(const PtrConnection&)>;
    using MessageCb = std::function<void(const PtrConnection&, Buffer*)>;
    using CloseCb = std::function<void(const PtrConnection&)>;
    using AnyEventCb = std::function<void(const PtrConnection&)>;
    ConnectedCb _connected_cb;
    MessageCb _msg_cb;
    CloseCb _close_cb;
    AnyEventCb _any_event_cb;
    CloseCb _server_close_cb;
private:
    void HandleRead()
    {
        // 1. socket recv input and load into in_buffer
        // (TO OPTIMIZE)current version: we are unsure size of input, so we use a temporary buffer store data
        // to avoid increase too much extra space in inbuffer
        char buffer[65536] = {0};
        ssize_t ret = _socket.RecvNonBlock(&buffer, 65535);
        if(ret == -2) return HandleClose(); // peer close
        if(ret < 0) return ShutDownInLoop();
        // 2. callback msgcb
        _in_buffer.WriteAndMove(&buffer, ret);
        if(_in_buffer.ReadableSize() > 0)
        {
            _msg_cb(shared_from_this(), &_in_buffer);
        }
    }
    void HandleWrite()
    {
        // 1. socket send outbuffer data
        ssize_t ret = _socket.SendNonBlock(_out_buffer.ReadPos(), _out_buffer.ReadableSize());
        if(ret < 0) // send err, close connection
        {
            HandleClose();
        }
        _out_buffer.MoveRead(ret);
        // 2. check outbuffer empty
        if(_out_buffer.ReadableSize() == 0)
        {
            _channel.DisableWrite(); // close write monitor
            if(_stat == ConnStatus::DISCONNECTING)
            {
                Release();
            }
        }
    }
    void HandleClose()
    {
        // check inbuffer empty
        if(_in_buffer.ReadableSize() > 0)
        {
            _msg_cb(shared_from_this(), &_in_buffer);
        }
        Release();
    }
    void HandleError()
    {
        HandleClose();
    }
    void HandleEvent()
    {
        // 1. refresh timertask
        if(_enable_inactive_release) _loop->RefreshTask(_conn_id);
        // 2. excute any event callback supplied by user
        if(_any_event_cb) _any_event_cb(shared_from_this());
    }
    void EstablishInLoop()
    {
        // 1. change status
        assert(_stat == ConnStatus::CONNECTING);
        _stat = ConnStatus::CONNECTED;
        // 2. enable read
        _channel.EnableRead();
        // 3. use initial callback
        if(_connected_cb) _connected_cb(shared_from_this());
    }
    void ReleaseInLoop()
    {
        // 1. change status
        _stat = ConnStatus::DISCONNECTED;
        // 2. remove event monitor
        _channel.Remove();
        // 3. close sockfd
        _socket.Close();
        // 4. remove release timer task
        if(_loop->HasTimerTask(_conn_id)) CancelInactiveReleaseInLoop();
        // 5. use close callback(user & server)
        if(_close_cb) _close_cb(shared_from_this());
        if(_server_close_cb) _server_close_cb(shared_from_this());
    }
    void SendInLoop(Buffer& buf)
    {
        // 1. check status
        if(_stat == ConnStatus::DISCONNECTED) return;
        // 2. load data into outbuffer
        _out_buffer.WriteAndMoveBuffer(buf);
        // 3. enable wirte
        if(_channel.MonitorWrite() == false) _channel.EnableWrite();
    }
    void ShutDownInLoop()
    {
        // 1. change status
        _stat = ConnStatus::DISCONNECTING;
        // 2. check inbuffer
        if(_in_buffer.ReadableSize() > 0)
        {
            _msg_cb(shared_from_this(), &_in_buffer);
        }
        // 3. checkoutbuffer
        if(_out_buffer.ReadableSize() > 0)
        {
            if(_channel.MonitorWrite() == false) _channel.EnableWrite();
        }
        if(_out_buffer.ReadableSize() == 0) Release();
    }
    void RegisterInactiveReleaseInLoop(int sec)
    {
        _enable_inactive_release = true;
        if(_loop->HasTimerTask(_conn_id))
        {
            _loop->RefreshTask(_conn_id);
        }
        else{
            _loop->AddTask(_conn_id, sec, [this](){Release();});
        }
    }
    void CancelInactiveReleaseInLoop()
    {
        _enable_inactive_release = false;
        if(_loop->HasTimerTask(_conn_id))
        {
            _loop->CancelTask(_conn_id);
        }
    }
    void UpgradeProtocolInLoop(const std::any& context, const ConnectedCb& connected_cb, 
        const MessageCb& msg_cb, const CloseCb& close_cb, const AnyEventCb& any_event_cb)
    {
        _context = context;
        _connected_cb = connected_cb;
        _msg_cb = msg_cb;
        _close_cb = close_cb;
        _any_event_cb = any_event_cb;
    }
public:
    Connection(uint64_t id, int sockfd, EventLoop* loop)
        :_conn_id(id)
        ,_sockfd(sockfd)
        ,_enable_inactive_release(false)
        ,_loop(loop)
        ,_stat(ConnStatus::CONNECTING)
        ,_socket(_sockfd)
        ,_channel(_loop, _sockfd)
    {
        _channel.SetReadCb([this](){HandleRead();});
        _channel.SetWriteCb([this](){HandleWrite();});
        _channel.SetErrorCb([this](){HandleError();});
        _channel.SetCloseCb([this](){HandleClose();});
        _channel.SetEventCb([this](){HandleEvent();});
    }
    int Fd() {return _sockfd;}
    uint64_t Id() {return _conn_id;}
    bool IsConnected() {return _stat == ConnStatus::CONNECTED;}
    void SetContext(const std::any& context) {_context = context;}
    std::any* GetContext() {return &_context;}
    void SetConnectedCb(const ConnectedCb& cb) {_connected_cb = cb;}
    void SetMessageCb(const MessageCb& cb) {_msg_cb = cb;}
    void SetCloseCb(const CloseCb& cb) {_close_cb = cb;}
    void SetAnyEventCb(const AnyEventCb& cb) {_any_event_cb = cb;}
    void SetServerCloseCb(const CloseCb& cb) {_server_close_cb = cb;}
    void Established()
    {
        _loop->RunInLoop([this](){EstablishInLoop();});
    }
    void Send(const char* buf, size_t len)
    {
        Buffer buffer;
        buffer.WriteAndMove(buf, len);
        _loop->RunInLoop([this, b = std::move(buffer)]()mutable{SendInLoop(b);});
    }
    void ShutDown()
    {
        _loop->RunInLoop([this](){ShutDownInLoop();});
    }
    void Release()
    {
        _loop->RunInLoop([this](){ReleaseInLoop();});
    }
    void RegisterInactiveRelease(int sec)
    {
        _loop->RunInLoop([this, sec](){RegisterInactiveReleaseInLoop(sec);});
    }
    void CancelInactiveRelease()
    {
        _loop->RunInLoop([this](){CancelInactiveReleaseInLoop();});
    }
    void UpgradeProtocol(const std::any& context, const ConnectedCb& connected_cb, 
        const MessageCb& msg_cb, const CloseCb& close_cb, const AnyEventCb& any_event_cb)
    {
        _loop->AssertInLoop(); // make sure protocol upgrade be excuted in loop and immediately
        // _loop->RunInLoop([this](){UpgradeProtocolInLoop(context, connected_cb, msg_cb, close_cb, any_event_cb);});
        _loop->RunInLoop(std::bind(&Connection::UpgradeProtocolInLoop, this, context, connected_cb, msg_cb, close_cb, any_event_cb));
    }
    ~Connection()
    {
        DEBUG_LOG("Connection Release: %p", this);
    }
};

using AcceptCb = std::function<void(int)>;
class Acceptor
{
private:
    Socket _socket;
    std::unique_ptr<Channel> _channel;
    EventLoop* _loop;
    AcceptCb _accept_cb;
private:
    void HandleRead()
    {
        int newfd = _socket.Accept();
        if(newfd < 0) return;
        if(_accept_cb) _accept_cb(newfd);
    }
public:
    Acceptor(int port, EventLoop* loop)
        :_loop(loop)
    {
        bool ret = _socket.CreateServer(port);
        assert(ret);
        _channel.reset(new Channel(_loop, _socket.Fd()));
        _channel->SetReadCb([this](){HandleRead();});
    }
    void SetAcceptCb(const AcceptCb& cb) {_accept_cb = cb;}
    void StartListen() {_channel->EnableRead();}
};

class TcpServer
{
private:
    uint64_t _next_id;
    int _port;
    int _timeout;
    bool _enable_inactive_release;

    EventLoop _baseloop;
    Acceptor _acceptor;
    LoopThreadPool _pool;
    std::unordered_map<uint64_t, PtrConnection> _conns;

    using ConnectedCb = std::function<void(const PtrConnection&)>;
    using MessageCb = std::function<void(const PtrConnection&, Buffer*)>;
    using CloseCb = std::function<void(const PtrConnection&)>;
    using AnyEventCb = std::function<void(const PtrConnection&)>;
    ConnectedCb _connected_cb;
    MessageCb _msg_cb;
    CloseCb _close_cb;
    AnyEventCb _any_event_cb;
private:
    void NewConnection(int newfd)
    {
        _next_id++;
        PtrConnection conn(new Connection(_next_id, newfd, _pool.NextLoop()));
        conn->SetConnectedCb(_connected_cb);
        conn->SetMessageCb(_msg_cb);
        conn->SetCloseCb(_close_cb);
        conn->SetAnyEventCb(_any_event_cb);
        conn->SetServerCloseCb([this](const PtrConnection& c){RemoveConnection(c);});
        if(_enable_inactive_release) conn->RegisterInactiveRelease(_timeout);
        conn->Established();
        _conns.insert({_next_id, conn});
    }
    void RemoveConnectionInLoop(const PtrConnection& conn)
    {
        int id = conn->Id();
        auto it = _conns.find(id);
        if(it != _conns.end())
        {
            _conns.erase(it);
        }
    }
    void RemoveConnection(const PtrConnection& conn)
    {
        _baseloop.RunInLoop([this, conn](){RemoveConnectionInLoop(conn);});
    }
    void RunAfterInLoop(const Functor& func, int delay)
    {
        _next_id++;
        _baseloop.AddTask(_next_id, delay, func);
    }
public:
    TcpServer(uint64_t port)
        :_port(port)
        ,_next_id(0)
        ,_enable_inactive_release(false)
        ,_timeout(0)
        ,_acceptor(port, &_baseloop)
        ,_pool(&_baseloop)
    {
        _acceptor.SetAcceptCb([this](int newfd){NewConnection(newfd);});
        _acceptor.StartListen();
    }
    void SetThreadCount(int cnt)
    {
        _pool.SetThreadCnt(cnt);
    }
    void SetConnectedCb(const ConnectedCb& cb) {_connected_cb = cb;}
    void SetMessageCb(const MessageCb& cb) {_msg_cb = cb;}
    void SetCloseCb(const CloseCb& cb) {_close_cb = cb;}
    void SetAnyEventCb(const AnyEventCb& cb) {_any_event_cb = cb;}
    void EnableInactiveRelease(int timeout)
    {
        _enable_inactive_release = true;
        _timeout = timeout;
    }
    void RunAfter(const Functor& func, int delay)
    {
        _baseloop.RunInLoop([this, func, delay](){RunAfterInLoop(func, delay);});
    }
    void Start()
    {
        _pool.CreateLoops();
        _baseloop.Start();
    }
};


void Channel::Remove() {_loop->RemoveEvent(this);}
void Channel::Update() {_loop->UpdateEvent(this);}

void TimerWheel::AddTask(uint64_t id, uint32_t timeout, const TaskCb& tcb)
{
    _loop->RunInLoop([this, id, timeout, tcb](){AddTaskInLoop(id, timeout, tcb);});
}
void TimerWheel::RefreshTask(uint64_t id)
{
    _loop->RunInLoop([this, id](){RefreshTaskInLoop(id);});
}
void TimerWheel::Cancel(uint64_t id)
{
    _loop->RunInLoop([this, id](){CancelInLoop(id);});
}