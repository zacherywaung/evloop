#include "http.hpp"

int main()
{
    std::string tmp;
    Util::ReadFile("./http.hpp", &tmp);
    Util::WriteFile("./fortest.cc", tmp);
    return 0;
}
