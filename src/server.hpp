#include <vector>
#include <string>
#include <cstring>
#include <cassert>

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