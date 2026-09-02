#include <iostream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <unistd.h>

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
private:
    void RemoveTask(uint64_t id)
    {
        auto it = _mp.find(id);
        if(it != _mp.end())
        {
            _mp.erase(it);
        }
    }
public:
    TimerWheel()
        :_capacity(60)
        ,_wheel(_capacity)
        ,_tick(0)
    {}
    void AddTask(uint64_t id, uint32_t timeout, const TaskCb& tcb)
    {
        PtrTask pt(new Timertask(id, timeout, tcb));
        // pt->SetRelease(std::bind(&TimerWheel::RemoveTask, this, id));
        pt->SetRelease([this, id](){RemoveTask(id);});
        int pos = (_tick + timeout) % _capacity;
        _wheel[pos].push_back(pt);
        _mp[id] = WeakTask(pt);
    }
    void RefreshTask(uint64_t id)
    {
        auto it = _mp.find(id);
        if(it == _mp.end())
            return;
        PtrTask pt = it->second.lock();
        int timeout = pt->GetTimeout();
        int pos = (_tick + timeout) % _capacity;
        _wheel[pos].push_back(pt);
    }
    void Cancel(uint64_t id)
    {
        auto it = _mp.find(id);
        if(it == _mp.end())
            return;
        PtrTask pt = it->second.lock();
        pt->Cancel();
    }
    void Run()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear();
    }
};

class Test
{
public:
    Test()
    {
        std::cout << "Constructor" << std::endl;
    }
    ~Test()
    {
        std::cout << "Destructor" << std::endl;
    }
};

void DelTest(Test* t)
{
    delete t;
}

int main()
{
    TimerWheel tw;
    Test* t = new Test();
    tw.AddTask(100, 5, [t](){DelTest(t);});

    for(int i = 0; i < 5; i++)
    {
        sleep(1);
        tw.Run();
        tw.RefreshTask(100);
        std::cout << "refresh task" << std::endl;
    }
    // tw.Cancel(100);
    while(1)
    {
        std::cout << "***************" << std::endl;
        tw.Run();
        sleep(1);
    }
}