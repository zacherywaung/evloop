#include "http.hpp"
#include <iostream>

int main()
{
    // std::string tmp;
    // Util::ReadFile("./http.hpp", &tmp);
    // Util::WriteFile("./fortest.cc", tmp);
    std::cout << Util::IsDirectory("testdir") << std::endl;
    std::cout << Util::IsRegular("testdir") << std::endl;
    std::cout << Util::IsDirectory("http.hpp") << std::endl;
    std::cout << Util::IsRegular("http.hpp") << std::endl;
    return 0;
}
