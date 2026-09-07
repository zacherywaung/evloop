#include "server.hpp"
#include <iostream>

int main()
{
    Buffer buf;
    for(int i = 0; i <= 300; i++)
    {
        std::string tmp = "hello!" + std::to_string(i) + "\n";
        buf.WriteAndMoveString(tmp);
    }
    // while(buf.ReadableSize() > 0)
    // {
    //     std::cout << buf.GetLineAndMove() << std::endl;
    // }
    // std::string out = buf.ReadAsStringAndMove(buf.ReadableSize());
    // std::cout << out << std::endl;
    for(int i = 0; i < 5; i++)
    {
        INFO_LOG("hello");
        ERR_LOG("hello");
    }
}