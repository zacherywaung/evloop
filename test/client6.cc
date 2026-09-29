/* large file transaction test1 */
/* client6 send large file put requests(100 MB) */
/* to create large file, use command [dd if=/dev/zero of=large_file.txt bs=1M count=100] */
/* Expected: server create 1234.txt correctly, and md5 code of two file are same or using [cmp /app/test/large_file.txt /app/src/http/wwwroot/1234.txt && echo IDENTICAL] in docker */
#include "../src/http/http.hpp"
#include <unistd.h>

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient(8080, "127.0.0.1");
    std::string req = "PUT /1234.txt HTTP/1.1\r\nConnection: keep-alive\r\n";
    std::string body;
    Util::ReadFile("large_file.txt", &body);
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    assert(cli_sock.Send(req.c_str(), req.size()) != -1);
    assert(cli_sock.Send(body.c_str(), body.size()) != -1);
    char buffer[1024] = {0};
    assert(cli_sock.Recv(buffer, 1023));
    DEBUG_LOG("[%s]", buffer);
    sleep(8);
    return 0;
}