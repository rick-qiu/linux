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

using namespace std;

volatile bool stop = false; // although volatile variable has no happened-before sematic, it is sufficient here

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
    printf("   while true; do nc localhost 8080 1>/dev/null 2>&1; if [ $? -ne 0 ]; then break; fi; sleep 5; done\n");
    printf("   **shutdown gracefully:\n");
    printf("   kill -s SIGUSR2 %d\n", getpid());
    printf("=======================================================================\n");
    struct FDDeleter {
        void operator()(int* pfd) const {
            auto ret = close(*pfd);
            while(-1 == ret && EINTR == errno) {
                ret = close(*pfd);
            }
        }
    };
    constexpr int MAX_EVENTS = 10;
    struct epoll_event ev, events[MAX_EVENTS];
    auto epollfd = epoll_create1(EPOLL_CLOEXEC);
    if(-1 == epollfd) {
        fprintf(stderr, "failed to create epoll file descriptor\n");
        return EXIT_FAILURE;
    }
    unique_ptr<int, FDDeleter> raii_epollfd(&epollfd);
    // create a notify between two threads
    // potentially mimic Event on Windows platform
    auto evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(-1 == evfd) {
        fprintf(stderr, "failed to create event file descriptor\n");
        return EXIT_FAILURE;
    }
    unique_ptr<int, FDDeleter> raii_evfd(&evfd);
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = evfd;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, evfd, &ev)) {
        fprintf(stderr, "failed to event file descriptor to epoll monitoring\n");
        return EXIT_FAILURE;
    }
    // add a timer firing every 5 seconds
    auto timerfd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC | TFD_NONBLOCK);
    if(-1 == timerfd) {
        fprintf(stderr, "failed to create file descriptor for timer\n");
        return EXIT_FAILURE;
    }
    unique_ptr<int, FDDeleter> raii_timerfd(&timerfd);
    struct itimerspec timerspec;
    memset(&timerspec, 0, sizeof(timerspec));
    timerspec.it_interval.tv_sec = 5;
    timerspec.it_value.tv_sec = 2;
    if(-1 == timerfd_settime(timerfd, 0, &timerspec, nullptr)) {
        fprintf(stderr, "failed to start timer\n");
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = timerfd;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, timerfd, &ev)) {
        fprintf(stderr, "failed to add timer file descriptor to epoll monitoring\n");
        return EXIT_FAILURE;
    }
    // block all signals which could be blocked
    sigset_t mask;
    if(-1 == sigemptyset(&mask)) {
        fprintf(stderr, "failed to initialize signal set\n");
        return EXIT_FAILURE;
    }
    if(-1 == sigfillset(&mask)) {
        fprintf(stderr, "failed to fill signal set\n");
        return EXIT_FAILURE;
    }
    if(-1 == sigprocmask(SIG_BLOCK, &mask, nullptr)) {
        fprintf(stderr, "failed to mask signal set\n");
        return EXIT_FAILURE;
    }
    auto sigfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if(-1 == sigfd) {
        fprintf(stderr, "failed to create file descriptor for signal\n");
        return EXIT_FAILURE;
    }
    unique_ptr<int, FDDeleter> raii_sigfd(&sigfd);
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = sigfd;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, sigfd, &ev)) {
        fprintf(stderr, "failed to add signal file descriptor to epoll monitoring\n");
        return EXIT_FAILURE;
    }

    // add standard input file descriptor
    auto flags = fcntl(STDIN_FILENO, F_GETFL);
    flags |= O_NONBLOCK;
    if(-1 == fcntl(STDIN_FILENO, F_SETFL, flags)) {
        fprintf(stderr, "failed to set NONBLOCK on standard input file descriptor\n");
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev)) {
        fprintf(stderr, "failed to add stardard input file descriptor to epoll monitoring\n");
        return EXIT_FAILURE;
    }
    // add a listening socket to monitor
    auto sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(-1 == sockfd) {
        fprintf(stderr, "failed to create listening socket\n");
        return EXIT_FAILURE;
    }
    unique_ptr<int, FDDeleter> raii_sockfd(&sockfd);
    int enable = 1;
    if(-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) {
        fprintf(stderr, "failed to set socket option\n");
        return EXIT_FAILURE;

    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);
    if(-1 == bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr))) {
        fprintf(stderr, "failed to bind listening socket\n");
        return EXIT_FAILURE;
    }
    constexpr int BACKLOG = 10;
    if(-1 == listen(sockfd, BACKLOG)) {
        fprintf(stderr, "failed to listening socket\n");
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev)) {
        fprintf(stderr, "failed to add listening socket file descriptor to epoll monitoring\n");
        return EXIT_FAILURE;
    }

    // IMPORTANT!
    // create another thread here so that SIGUSR1 will also be blocked in new thread
    pthread_t thread;
    if(pthread_create(&thread, nullptr, thread_start, reinterpret_cast<void*>(evfd)) == -1) {
        fprintf(stderr, "failed to create a new thread\n");
        return EXIT_FAILURE;
    }

    // start event loop
    while(!stop) {
        auto nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        for(auto i = 0; i < nfds; ++i) {
            if(timerfd == events[i].data.fd) {
                uint64_t value;
                read(timerfd, &value, sizeof(value));
                printf("[PID: %d] timer event received, value: %lld\n", getpid(), value);
            } else if(sigfd ==  events[i].data.fd) {
                struct signalfd_siginfo fdsi;
                read(sigfd, &fdsi, sizeof(fdsi));
                if(SIGUSR2 == fdsi.ssi_signo) {
                    printf("[PID: %d] %s received, exit event loop\n", getpid(), strsignal(fdsi.ssi_signo));
                    stop = true;
                } else {
                    printf("[PID: %d] signal event received, sender pid: %d, signal: %s\n", getpid(), fdsi.ssi_pid, strsignal(fdsi.ssi_signo));
                }
            } else if(evfd == events[i].data.fd) {
                uint64_t value;
                read(evfd, &value, sizeof(value));
                printf("[PID: %d] thread event received, value: %lld\n", getpid(), value);
            } else if(STDIN_FILENO == events[i].data.fd) {
                constexpr int BUF_SIZE = 10;
                char buf[BUF_SIZE];
                auto n = read(STDIN_FILENO, buf, sizeof(buf));
                memset(buf + n, 0, sizeof(buf) - n);
                buf[sizeof(buf)-1] = '\0';
                printf("[PID: %d] standard input ready event received, value: %s", getpid(), buf);
            } else if(sockfd == events[i].data.fd) {
                auto cfd = accept(sockfd, nullptr, nullptr);
                unique_ptr<int, FDDeleter> raii_cfd(&cfd);
                printf("[PID: %d] client connection event received, connection closed\n", getpid());
            } else {
                printf("[PID: %d] unknown event received\n", getpid());
            }
        }
        // try "kill -s SIGSTOP $PID" and then "kill -s SIGCONT $PID"
        // if remove EINTR != errno
        if(-1 == nfds && EINTR != errno) {
            auto error_code = errno;
            fprintf(stderr, "[PID: %d] error: %s, exit event loop\n", getpid(), strerror(error_code));
            stop = true;
        }
    }
    pthread_join(thread, nullptr);
    return EXIT_SUCCESS;
}
