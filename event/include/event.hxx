#ifndef LINUX_EVENT_EVENT_HXX
#define LINUX_EVENT_EVENT_HXX

#include <sys/epoll.h>
#include <pthread.h>

#include <cstdint>

#ifdef DEBUG
#include <cstdio>
#endif

#include <functional>
#include <queue>
#include <exception>
#include <memory>
#include <string>
#include <map>
#include <thread>
#include <vector>
#include <utility>

namespace linux {
    namespace event {

        struct deleter4fd {
            void operator()(int* pfd);
        };

        class timer_exception: public std::runtime_error {
        public:
            timer_exception(const std::string& msg): runtime_error(msg) {
            }
        };

        class trigger {
        public:
            trigger() {
            }
            trigger(const trigger& tgr) = delete;
            trigger& operator=(const trigger& tgr) = delete;
            virtual const std::function<void()>& get_task() const = 0;
            virtual ~trigger() {
            }
        };

        class timer_trigger: public trigger {
        public:
            timer_trigger(uint64_t sec, std::function<void()>&& tsk);
            timer_trigger(timer_trigger&& tgr);
            timer_trigger& operator=(timer_trigger&& tgr);
            timer_trigger(const timer_trigger& tgr) = delete;
            timer_trigger& operator=(const timer_trigger& tgr) = delete;
            int native_handle() const {
                return timerfd;
            }
            const std::function<void()>& get_task() const override {
                return std::ref(task);
            }
            uint32_t get_events() const {
                return events;
            }
            virtual ~timer_trigger() {
#ifdef DEBUG
                std::printf("timer_trigger deconstructor, id = %d, timerfd = %d\n", id, timerfd);
#endif
            }
        private:
            int timerfd;
            std::unique_ptr<int, deleter4fd> timerfd_raii;
            std::function<void()> task;
            std::uint32_t events;
#ifdef DEBUG
            static std::uint32_t count;
            std::uint32_t id;
#endif
        };

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

        class event_loop_exception: public std::runtime_error {
        public:
            event_loop_exception(const std::string& msg): runtime_error(msg) {
            }
        };

        class event_loop {
        public:
            event_loop();
            void operator()();
            event_loop(event_loop&& ep);
            event_loop(const event_loop& ep) = delete;
            event_loop& operator=(const event_loop& ep) = delete;
            event_loop& operator=(event_loop&& ep);
            template<typename T>
            void register_trigger(T&& tgr) {
                struct epoll_event ev;
                ev.data.fd = tgr.native_handle();
                ev.events = tgr.get_events();
                auto task = std::bind(event_loop::do_register, epollfd, ev);

                std::unique_ptr<T> trigger(new T(std::move(tgr)));
                auto fd = ev.data.fd;
                triggers.insert(make_pair(std::move(fd), std::move(trigger)));
                async_call(std::move(task));
            }
            void async_call(std::function<void()>&& task);
        private:
            static void do_register(int epfd, struct epoll_event ev);
            int epollfd;
            std::unique_ptr<int, deleter4fd> epollfd_raii;
            int async_eventfd;
            std::unique_ptr<int, deleter4fd> async_eventfd_raii;
            int sigfd;
            std::unique_ptr<int, deleter4fd> sigfd_raii;
            bool exit;
            constexpr static uint32_t MAX_EVENTS = 10;
            struct epoll_event events[MAX_EVENTS];
            typedef std::map<int, std::unique_ptr<trigger>> trigger_container_t;
            trigger_container_t triggers;
            std::queue<std::function<void()>> async_queue;
        };
    }
}
#endif
