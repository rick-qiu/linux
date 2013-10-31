#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cerrno>

#include <memory>
#include <vector>
#include <utility>
#icnlude <algorithm>

using namespace std;

struct deleter {
    void operator()(int *pfd) const {
        auto ret = close(*pfd);
        while(-1 == ret && EINTR == errno) {
            ret = close(*pfd);
        }
    }
};

class client {
public:
    client(int fd): socketfd(fd), size(0), raii_socketfd(&socketfd) {
    }

    void set_data(void* src, int sz) {
        memcpy(buf, src, min(sizeof(buf), sz));
        size = sz;
    }
    void get_data(void* des, int& sz) const {
        memcpy(des, buf, min(sz, size));
        sz = size;
    }
    int getsocket() const {
        return socketfd;
    }
private:
    int socketfd;
    constexpr static int BUF_SIZE = 512;
    char buf[BUF_SIZE];
    int size;
    unique_ptr<int, deleter> raii_socketfd;
};

int main(int argc, char *argv[]) {

    constexpr int MAX_EVENTS = 10;
    struct epoll_event ev, events[MAX_EVENTS];
    auto epollfd = epoll_create1(EPOLL_CLOEXEC);
    if(-1 == epollfd) {
        perror("failed to create epoll file descriptor");
        return EXIT_FAILURE;
    }
    unique_ptr<int, deleter> raii_epollfd(&epollfd);

    // add a listening socket to monitor
    auto listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(-1 == listenfd) {
        perror("failed to create listening socket");
        return EXIT_FAILURE;
    }
    unique_ptr<int, deleter> raii_listenfd(&listenfd);
    int enable = 1;
    if(-1 == setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) {
        perror("failed to set socket option");
        return EXIT_FAILURE;

    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);
    if(-1 == bind(listenfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr))) {
        perror("failed to bind listening socket");
        return EXIT_FAILURE;
    }
    constexpr int BACKLOG = 10;
    if(-1 == listen(listenfd, BACKLOG)) {
        perror("failed to listening socket");
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev)) {
        perror("failed to add listening socket file descriptor to epoll monitoring");
        return EXIT_FAILURE;
    }

    typedef vector<unique_ptr<client>> container_t;
    container_t clients;

    // start event loop
    while(true) {
        auto nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        for(auto i = 0; i < nfds; ++i) {
            if(listenfd == events[i].data.fd) {
                auto fd = accept(listenfd, nullptr, nullptr);
                unique_ptr<client> raii_client(new client(fd));
                auto flags = fcntl(fd, F_GETFL);
                flags |= O_NONBLOCK;
                if(-1 == fcntl(fd, F_SETFL, flags)) {
                    perror("failed to set NONBLOCK on file descriptor");
                    continue;
                }
                memset(&ev, 0, sizeof(ev));
                ev.events = EPOLLIN;
                ev.data.ptr = raii_client.get();
                if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev)) {
                    perror("failed to add file descriptor to epoll monitor");
                }
                clients.push_back(move(raii_client));
            } else if((EPOLLIN & events[i].events) != 0) {
                constexpr int BUF_SIZE = 256;
                char buf[BUF_SIZE];
                auto p = reinterpret_cast<client*>(events[i].data.ptr);
                auto num = read(p->getsocket(), buf, sizeof(buf));
                
            } else if((EPOLLOUT & events[i].events) != 0) {
            } else {
            }
        }
    }

    return EXIT_SUCCESS;
}
