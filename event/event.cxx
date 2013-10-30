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

using namespace std;

volatile bool stop = false;

void* thread_start(void* args) {
    int evfd = reinterpret_cast<uint64_t>(args);
    while(!stop) {
        sleep(5);
        uint64_t value = 5;
        write(evfd, &value, sizeof(value));
    }
    return nullptr;
}

int main(int argc, char *argv[]) {
    printf("=======================================================================\n");
    printf("   *****event processing demonstration, process id: [%d]*****\n", getpid());
    printf("   **send SIGUSR1 to this process:\n");
    printf("   while true; do kill -s SIGUSR1 %d 1>/dev/null 2>&1; if [ $? -ne 0 ]; then break; fi; sleep 5; done\n", getpid());
    printf("   **connect to port 8080:\n");
    printf("   while true; do nc localhost 8080 1>/dev/null 2>&1; if [ $? -ne 0 ]; then break; fi; sleep 5; done\n", getpid());
    printf("=======================================================================\n");
    constexpr int MAX_EVENTS = 10;
    struct epoll_event ev, events[MAX_EVENTS];
    auto epollfd = epoll_create1(EPOLL_CLOEXEC);
    if(-1 == epollfd) {
        printf("failed to create epoll file descriptor\n");
        return EXIT_FAILURE;
    }
    // create a notify between two threads
    // potentially mimic Event on Windows platform
    auto evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(-1 == evfd) {
        printf("failed to create event file descriptor\n");
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = evfd;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, evfd, &ev)) {
        printf("failed to event file descriptor to epoll monitoring\n");
        return EXIT_FAILURE;
    }
    // add a timer firing every 60 seconds
    auto timerfd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC | TFD_NONBLOCK);
    if(-1 == timerfd) {
        printf("failed to create file descriptor for timer\n");
        return EXIT_FAILURE;
    }
    struct itimerspec timerspec;
    memset(&timerspec, 0, sizeof(timerspec));
    timerspec.it_interval.tv_sec = 5;
    timerspec.it_value.tv_sec = 2;
    if(-1 == timerfd_settime(timerfd, 0, &timerspec, nullptr)) {
        printf("failed to start timer\n");
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = timerfd;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, timerfd, &ev)) {
        printf("failed to add timer file descriptor to epoll monitoring\n");
        return EXIT_FAILURE;
    }
    // add signal SIGUSR1
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, nullptr);
    auto sigfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if(-1 == sigfd) {
        printf("failed to create file descriptor for signal\n");
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = sigfd;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, sigfd, &ev)) {
        printf("failed to add signal file descriptor to epoll monitoring\n");
        return EXIT_FAILURE;
    }
    // IMPORTANT!
    // create another thread here so that SIGUSR1 will also be blocked in new thread
    pthread_t thread;
    if(pthread_create(&thread, nullptr, thread_start, reinterpret_cast<void*>(evfd)) == -1) {
        printf("failed to create a new thread\n");
        return EXIT_FAILURE;
    }

    // add standard input file descriptor
    auto flags = fcntl(STDIN_FILENO, F_GETFL);
    flags |= O_NONBLOCK;
    if(-1 == fcntl(STDIN_FILENO, F_SETFL, flags)) {
        printf("failed to set NONBLOCK on standard input file descriptor\n");
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev)) {
        printf("failed to add stardard input file descriptor to epoll monitoring\n");
        return EXIT_FAILURE;
    }
    // add a listening socket to monitor
    auto sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(-1 == sockfd) {
        printf("failed to create listening socket\n");
        return EXIT_FAILURE;
    }
    int enable = 1;
    if(-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) {
        printf("failed to set socket option\n");
        return EXIT_FAILURE;

    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);
    if(-1 == bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr))) {
        printf("failed to bind listening socket\n");
        return EXIT_FAILURE;
    }
    constexpr int BACKLOG = 10;
    if(-1 == listen(sockfd, BACKLOG)) {
        printf("failed to listening socket\n");
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev)) {
        printf("failed to add listening socket file descriptor to epoll monitoring\n");
        return EXIT_FAILURE;
    }

    // start event loop
    while(true) {
        auto nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        for(auto i = 0; i < nfds; ++i) {
            if(timerfd == events[i].data.fd) {
                uint64_t value;
                read(timerfd, &value, sizeof(value));
                printf("timer event received, value: %lld\n", value);
            } else if(sigfd ==  events[i].data.fd) {
                struct signalfd_siginfo fdsi;
                read(sigfd, &fdsi, sizeof(fdsi));
                printf("signal event received, sender pid: %d, signal no: %d\n", fdsi.ssi_pid, fdsi.ssi_signo);
            } else if(evfd == events[i].data.fd) {
                uint64_t value;
                read(evfd, &value, sizeof(value));
                printf("thread event received, value: %lld\n", value);
            } else if(STDIN_FILENO == events[i].data.fd) {
                constexpr int BUF_SIZE = 10;
                char buf[BUF_SIZE];
                auto n = read(STDIN_FILENO, buf, sizeof(buf));
                memset(buf + n, 0, sizeof(buf) - n);
                buf[sizeof(buf)-1] = '\0';
                printf("standard input ready event received, value: %s", buf);
            } else if(sockfd == events[i].data.fd) {
                auto cfd = accept(sockfd, nullptr, nullptr);
                close(cfd);
                printf("client connection event received, connection closed\n");
            } else {
                printf("unknown event received\n");
            }
        }
        if(-1 == nfds) {
            printf("error on epoll waiting or interrupted, exit event loop\n");
            break;
        }
    }
    stop = true;
    timerfd_settime(timerfd, 0, nullptr, nullptr);
    close(timerfd);
    close(sigfd);
    close(epollfd);
    pthread_join(thread, nullptr);
    close(evfd);
    close(sockfd);
    return EXIT_SUCCESS;
}
