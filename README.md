# evloop

A multi-reactor network library in C++17, with an HTTP/1.1 server built on top. One event loop per thread, level-triggered epoll, and no locks on the connection path.

## Usage

An echo server — the library's entire surface is four callbacks:

```cpp
#include "server.hpp"

int main() {
    TcpServer server(8080);
    server.SetThreadCount(4);
    server.SetMessageCb([](const PtrConnection& conn, Buffer* buf) {
        size_t n = buf->ReadableSize();
        conn->Send(buf->ReadPos(), n);
        buf->MoveRead(n);
    });
    server.Start();
}
```

The HTTP layer builds on it, adding routing and static files:

```cpp
#include "http.hpp"

int main() {
    HttpServer server(8080);
    server.SetThreadCount(4);
    server.SetBaseDir("./wwwroot");

    server.Get("/hello", [](const HttpRequest& req, HttpResponse* rsp) {
        rsp->SetContent("hello", "text/plain");
    });

    server.Post("/login", [](const HttpRequest& req, HttpResponse* rsp) {
        rsp->SetContent("welcome " + req.GetParam("username"), "text/plain");
    });

    server.Listen();
}
```

## Architecture

### Components

| | |
|---|---|
| **EventLoop** | Owns an epoll instance, a timer wheel, and a task queue. Runs the loop for one thread. |
| **Poller** | The only code that touches epoll. Translates ready fds back into Channels. |
| **Channel** | One fd plus the events it watches and the callbacks to run. |
| **Socket** | RAII wrapper for a fd: create, bind, listen, accept, recv, send. |
| **Buffer** | Growable byte buffer with separate read and write offsets. |
| **TimerWheel** | Timers driven by a timerfd; expiry fires on destruction of the task. |
| **Acceptor** | Listening socket plus its Channel; hands new fds to a callback. |
| **Connection** | One TCP connection: a Socket, a Channel, and in/out Buffers. |
| **TcpServer** | Owns the acceptor, the loop pool, and the connection table. |
| **HttpContext** | Resumable parser: keeps position across recv calls. |
| **HttpServer** | Routing and static file serving on top of TcpServer. |

### Threading

```
                        ┌──────────────┐
        clients ───────→│   Acceptor   │  base loop, main thread
                        └──────┬───────┘
                               │ new fd, round-robin
                 ┌─────────────┼─────────────┐
                 ↓             ↓             ↓
           ┌──────────┐  ┌──────────┐  ┌──────────┐
           │EventLoop │  │EventLoop │  │EventLoop │   sub loops
           │  epoll   │  │  epoll   │  │  epoll   │
           │  timerfd │  │  timerfd │  │  timerfd │
           │  eventfd │  │  eventfd │  │  eventfd │
           │  tasks   │  │  tasks   │  │  tasks   │
           └────┬─────┘  └──────────┘  └──────────┘
                │
                ├── Connection ── Socket │ Channel │ in/out Buffer
                ├── Connection
                └── Connection
```

Each connection is assigned to one loop when it is accepted and stays there for its whole life, so no connection state is ever touched by two threads. Locks are only needed for the task queues, which is how
the two cross-thread handoffs happen: the base loop passes a new connection to a sub loop, and a sub loop asks the base loop to remove a closed one. Each handoff is a function pushed onto the target loop's queue and an eventfd write to wake it.

### Request path

```
epoll_wait returns
  → Channel::HandleEvent
      → Connection::HandleRead
          Socket::RecvNonBlock → in_buffer
          message callback
            → HttpContext::RecvHttpRequest    resumable state machine
            → HttpServer::Route               static file or route table
            → Connection::Send                → out_buffer, enable EPOLLOUT
  → RunAllTasks                               deferred destruction
```

Destruction is queued rather than run inline: the call always originates inside HandleEvent, which keeps using `this` after the callback returns.

## Build

Linux only — the event loop uses epoll, timerfd, and eventfd. On
macOS or Windows, use the provided container.

### In a container

```bash
docker build -t evloop-dev .
./dev.sh
```

`dev.sh` mounts the repository at `/app` and raises the fd limit, so
you edit on the host and compile inside the container.

### Natively on Linux

```bash
sudo apt-get install -y build-essential
```

### Running the HTTP server

```bash
cd src/http
make
./main 4          # argument is the number of IO threads
```

Then, from another shell:

```bash
curl http://127.0.0.1:8080/hello
curl http://127.0.0.1:8080/            # serves wwwroot/index.html
```

### Tests

`test/` contains clients covering long connections, pipelined
requests, mismatched Content-Length, and large PUT uploads. Each file
documents what it expects the server to do.

```bash
cd test
make
./client5         # pipelining: three requests in one write
```

## Benchmarks

Measured on an AWS `c6g.2xlarge` (8 vCPU Graviton, 16 GB, Ubuntu 24.04) over loopback, with `ulimit -n 65535` and `somaxconn` raised. The client runs on the same instance and competes for the same cores,
so these figures understate what the server would do against a remote load generator.

Load: webbench, 500 concurrent clients, 60 seconds, against a route
returning a fixed 2-byte body so the numbers reflect the framework
rather than handler work. Short-lived connections — webbench sends
HTTP/1.0 without a `Connection` header, so every request opens a new
socket.

| IO threads | requests/sec |              |
|------------|--------------|--------------|
| 1          | 11,086       | ███          |
| 2          | 22,123       | ██████       |
| 4          | 31,480       | █████████    |
| 6          | 36,896       | ███████████  |
| 8          | 36,320       | ███████████  |
| 16         | 33,564       | ██████████   |

No failed requests in any run.

Throughput doubles from one thread to two and keeps climbing to a peak at six, which is what one-loop-per-thread predicts: each loop owns its connections outright, so nothing but available cores limits scaling. The peak falls short of eight because the base loop and the load generator also need CPU. Past the core count, context switching outweighs the remaining parallelism — sixteen threads are 9% slower than six.

At 10,000 concurrent connections (ApacheBench, 500k requests):

| | |
|---|---|
| requests/sec | 15,225 |
| failed | 0 |
| p50 latency | 652 ms |
| p99 latency | 694 ms |
| max | 3,741 ms |

Throughput drops because 10,000 client sockets on the same instance consume a large share of the cores, but the latency distribution stays tight: the gap between p50 and p99 is 42 ms, so no connection is starved.