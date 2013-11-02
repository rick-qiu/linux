#include "event.hxx"

#include <unistd.h>
#include <sys/types.h>

#include <cstdlib>
#include <cstdio>

#include <utility>
#include <thread>

int main(int argc, char *argv[]) {
    using namespace std;
    using namespace linux::event;
    printf("start, process %d\n", getpid());
    try {
        event_loop ep;
        timer_trigger tt(5, [](){ printf("timer fired in thread\n");});
        ep.register_trigger(move(tt));
        std::thread t(std::move(ep));
        while(true) {
            sleep(5);
            //ep.async_call([](){ printf("async call\n");});
        }
    } catch(event_loop_exception& e) {
        printf("ERROR: %s\n", e.what());
    } catch(timer_exception& e) {
        printf("ERROR: %s\n", e.what());
    }
    return 0;
}
