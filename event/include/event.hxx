#ifndef LINUX_EVENT_EVENT_HXX
#define LINUX_EVENT_EVENT_HXX

#include <sys/epoll.h>
#include <pthread.h>

#include <cstdint>

#include <functional>
#include <queue>
#include <exception>
#include <memory>
#include <string>
#include <map>

namespace linux {
    namespace event {

        struct trigger {
            trigger(int f, std::uint32_t evs, const std::function<void()>& callee):
                fd(f), events(evs), handler(callee) {
            }
            int fd;
            std::uint32_t events;
            std::function<void()> handler;
        };

        struct deleter4fd {
            void operator()(int* pfd);
        };

        class timer_exception: public std::runtime_error {
        public:
            timer_exception(const std::string& msg): runtime_error(msg) {
            }
        };

        class timer_trigger: public trigger {
        public:
            timer_trigger(uint64_t sec, const std::function<void()>& handler);
        private:
            std::unique_ptr<int, deleter4fd> fd_raii;
        };

        trigger make_trigger(int fd, std::uint32_t events, const std::function<void()>& handler);

        class thread_exception: public std::runtime_error {
        public:
            thread_exception(const std::string& msg): runtime_error(msg) {
            }
        };

        class thread {
        public:
            thread();
            void register_trigger(const trigger& t);
            void async_call(const std::function<void()>& callee);
        private:
            static void* thread_start(void* arg);
            void run();
            std::queue<std::function<void()>> async_queue;
            pthread_mutex_t async_queue_guard;
            pthread_t thread_handle;
            int epollfd;
            int async_eventfd;
            int sigfd;
            std::unique_ptr<int, deleter4fd> epollfd_raii;
            std::unique_ptr<int, deleter4fd> async_eventfd_raii;
            std::unique_ptr<int, deleter4fd> sigfd_raii;
            bool stop;
            constexpr static int MAX_EVENTS = 10;
            struct epoll_event events[MAX_EVENTS];
            struct event_helper {
                struct epoll_event event;
                std::function<void()> task;
            };
            typedef std::map<int, event_helper> hcontainer_t;
            hcontainer_t event_helpers;
        };
    }
}

#endif
