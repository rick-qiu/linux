#include "event.hxx"

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <pthread.h>
#include <sys/timerfd.h>

#include <cerrno>
#include <cstring>
#include <cstdint>

#ifdef DEBUG
#include <cstdio>
#endif

using namespace std;

namespace linux {
    namespace event {
        
#ifdef DEBUG
        uint32_t timer_trigger::count = 0;
#endif
        timer_trigger::timer_trigger(uint64_t sec, function<void()>&& tsk):
            timerfd(-1), timerfd_raii(&timerfd), task(tsk), events(EPOLLIN) {
            if((timerfd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC | TFD_NONBLOCK)) == -1) {
                throw timer_exception(strerror(errno));
            }
#ifdef DEBUG
            id = count;
            ++count;
            printf("timer_trigger::timerfd = %d, id = %d\n", timerfd, id);
#endif
            struct itimerspec timerspec;
            memset(&timerspec, 0, sizeof(timerspec));
            timerspec.it_interval.tv_sec = sec;
            timerspec.it_value.tv_sec = 1; // fire 1 sec later
            if(-1 == timerfd_settime(timerfd, 0, &timerspec, nullptr)) {
                throw timer_exception(strerror(errno));
            }
        }

        timer_trigger::timer_trigger(timer_trigger&& tgr) {
            timerfd_raii.reset(&timerfd);
            *this = move(tgr);
#ifdef DEBUG
            id = count;
            ++count;
            printf("timer_trigger::timerfd = %d, id = %d\n", timerfd, id);
#endif
        }

        timer_trigger& timer_trigger::operator=(timer_trigger&& tgr) {
            timerfd = tgr.timerfd;
            tgr.timerfd = -1;
            task = move(tgr.task);
            events = tgr.events;
            tgr.events = 0;
            return *this;
        }
        /*
        thread::thread(): epollfd(-1), async_eventfd(-1), sigfd(-1), 
                          epollfd_raii(&epollfd), async_eventfd_raii(&async_eventfd),
                          sigfd_raii(&sigfd), async_queue_guard(PTHREAD_MUTEX_INITIALIZER) {
            // there is race contiditon when calling strerror, using strerror_r instread
            if((epollfd = epoll_create1(EPOLL_CLOEXEC)) == -1) {
                throw thread_exception(strerror(errno));
            }
            if((async_eventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) == -1) {
                throw thread_exception(strerror(errno));
            }
            // register event here to avoid the thread run away when there is other interested events
            // DO NOT call register_trigger to do this here
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGUSR1);
            pthread_sigmask(SIG_BLOCK, &mask, nullptr);
            if((sigfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC)) == -1) {
                throw thread_exception(strerror(errno));
            }
            struct epoll_event ev;
            ev.data.fd = sigfd;
            ev.events = EPOLLIN;
            if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sigfd, &ev) == -1) {
                throw thread_exception(strerror(errno));
            }
            ev.data.fd = async_eventfd;
            ev.events = EPOLLIN;
            if(epoll_ctl(epollfd, EPOLL_CTL_ADD, async_eventfd, &ev) == -1) {
                throw thread_exception(strerror(errno));
            }
            auto ret = pthread_create(&thread_handle, nullptr, thread_start, this);
            if(ret != 0) {
                throw thread_exception(strerror(ret));
            }
        }

        void* thread::thread_start(void* arg) {
            auto p = reinterpret_cast<thread*>(arg);
            p->run();
            return nullptr;
        }

        void thread::register_trigger(const trigger& t) {
            event_helper helper;
            helper.event.data.fd = t.fd;
            helper.event.events = t.events;
            helper.task = t.handler;
            auto p = event_helpers.insert(make_pair(t.fd, helper));
            auto task = bind(epoll_ctl, epollfd, EPOLL_CTL_ADD, t.fd, &(p.first->second.event));
            async_call(task);
        }

        void thread::async_call(const function<void()>& callee) {
#ifdef DEBUG
            printf("async call received\n");
#endif
            // could use  RAII
            pthread_mutex_lock(&async_queue_guard);
            async_queue.push(callee);
            pthread_mutex_unlock(&async_queue_guard);
            // just put a small non-zero
            uint64_t value = 1;
            write(async_eventfd, &value, sizeof(value));
        }

        void thread::run() {
            while(!stop) {
                auto ret = epoll_wait(epollfd, events, sizeof(events)/sizeof(struct epoll_event), -1);
                for(auto i = 0; i < ret; ++i) {
                    if(events[i].data.fd == async_eventfd) {
#ifdef DEBUG
                        printf("start to process async call\n");
#endif
                        // a problem potentially, maybe hold the lock too long
                        pthread_mutex_lock(&async_queue_guard);
                        while(!async_queue.empty()) {
                            auto task = async_queue.front();
                            async_queue.pop();
                            task();
                        }
                        pthread_mutex_unlock(&async_queue_guard);
                        // just discard the value
                        uint64_t value;
                        read(async_eventfd, &value, sizeof(value));
                    } else if(events[i].data.fd == sigfd) {
                        stop = true;
                    } else {
                        event_helpers[events[i].data.fd].task();
                        // read and discard
                        uint64_t value;
                        read(events[i].data.fd, &value, sizeof(value));
                    }
                }
            }
        }*/

        void deleter4fd::operator()(int* pfd) {
#ifdef DEBUG
            printf("release file descriptor %d\n", *pfd);
#endif
            if(*pfd < 0) {
                return;
            }
            while(-1 == close(*pfd) && EINTR == errno) {
            }
        }

        event_loop::event_loop(): epollfd(-1), async_eventfd(-1), sigfd(-1), 
                          epollfd_raii(&epollfd), async_eventfd_raii(&async_eventfd),
                                  sigfd_raii(&sigfd), exit(false) {
            // there is race contiditon when calling strerror, using strerror_r instread
            if((epollfd = epoll_create1(EPOLL_CLOEXEC)) == -1) {
                throw event_loop_exception(strerror(errno));
            }
#ifdef DEBUG
            printf("event_loop::epollfd = %d\n", epollfd);
#endif
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGUSR1);
            pthread_sigmask(SIG_BLOCK, &mask, nullptr);
            if((sigfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC)) == -1) {
                throw event_loop_exception(strerror(errno));
            }
#ifdef DEBUG
            printf("event_loop::sigfd = %d\n", sigfd);
#endif
            struct epoll_event ev;
            ev.data.fd = sigfd;
            ev.events = EPOLLIN;
            if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sigfd, &ev) == -1) {
                throw event_loop_exception(strerror(errno));
            }

            if((async_eventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) == -1) {
                throw event_loop_exception(strerror(errno));
            }           
#ifdef DEBUG
            printf("event_loop::async_eventfd = %d\n", async_eventfd);
#endif 
            ev.data.fd = async_eventfd;
            ev.events = EPOLLIN;
            if(epoll_ctl(epollfd, EPOLL_CTL_ADD, async_eventfd, &ev) == -1) {
                throw event_loop_exception(strerror(errno));
            }
        }

        void event_loop::operator()() {
            while(!exit) {
                auto ret = epoll_wait(epollfd, events, sizeof(events)/sizeof(struct epoll_event), -1);
                for(auto i = 0; i < ret; ++i) {
                    if(events[i].data.fd == async_eventfd) {
                        while(!async_queue.empty()) {
                            auto task = async_queue.front();
                            async_queue.pop();
                            task();
                        }
                        // just discard the value
                        uint64_t value;
                        read(async_eventfd, &value, sizeof(value));
                    } else if(events[i].data.fd == sigfd) {
                        exit = true;
                    } else {
                        triggers[events[i].data.fd]->get_task()();
                        // read and discard
                        uint64_t value;
                        read(events[i].data.fd, &value, sizeof(value));
                    }
                }
            }
        }

        void event_loop::do_register(struct epoll_event ev) {
            if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev)) {
                throw event_loop_exception(strerror(errno));
            }
        }

        void event_loop::async_call(function<void()>&& task) {
#ifdef DEBUG
            printf("async call received\n");
#endif
            async_queue.push(task);
            // just put a small non-zero
            uint64_t value = 1;
            write(async_eventfd, &value, sizeof(value));
        }
    }
}
