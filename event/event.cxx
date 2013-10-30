#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/eventfd.h>
#include <pthread.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>

using namespace std;

void* thread_start(void* args) {
    int evfd = reinterpret_cast<uint64_t>(args);
    while(true) {
        sleep(5);
        uint64_t value = 5;
        write(evfd, &value, sizeof(value));
    }
    return nullptr;
}

int main(int argc, char *argv[]) {
    printf("event processing demonstration, process id: %d\n", getpid());
    constexpr int MAX_EVENTS = 10;
    struct epoll_event ev, events[MAX_EVENTS];
    auto epollfd = epoll_create1(EPOLL_CLOEXEC);
    if(-1 == epollfd) {
        printf("failed to create epoll file descriptor\n");
        return EXIT_FAILURE;
    }
    // create a notify between two threads
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
    timerspec.it_value.tv_sec = 10;
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
    // create another thread here so that SIGUSR1 will also be blocked in new thread
    pthread_t thread;
    if(pthread_create(&thread, nullptr, thread_start, reinterpret_cast<void*>(evfd)) == -1) {
        printf("failed to create a new thread\n");
        return EXIT_FAILURE;
    }

    while(true) {
        auto nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        for(auto i = 0; i < nfds; ++i) {
            if(timerfd == events[i].data.fd) {
                printf("timer fired\n");
                uint64_t value;
                read(timerfd, &value, sizeof(value));
            } else if(sigfd ==  events[i].data.fd) {
                printf("SIGUSR1 event received\n");
                struct signalfd_siginfo fdsi;
                read(sigfd, &fdsi, sizeof(fdsi));
            } else if(evfd == events[i].data.fd) {
                printf("thread event received\n");
                uint64_t value;
                read(evfd, &value, sizeof(value));
            } else {
                printf("unknown event received\n");
            }
        }
        if(-1 == nfds) {
            printf("error on epoll waiting\n");
            break;
        }
    }
    timerfd_settime(timerfd, 0, nullptr, nullptr);
    close(timerfd);
    close(sigfd);
    close(epollfd);
    pthread_join(thread, nullptr);
    close(evfd);
    return EXIT_SUCCESS;
}
