#include "event.hxx"

#include <unistd.h>
#include <sys/types.h>

#include <cstdlib>
#include <cstdio>

int main(int argc, char *argv[]) {
    using namespace std;
    using namespace linux::event;
    printf("start, process %d\n", getpid());
    try {
        timer_trigger tt(5, [](){ printf("timer fired in thread\n");});
        event_loop ep;
        ep();
    } catch(event_loop_exception& e) {
        printf("ERROR: %s\n", e.what());
    } catch(timer_exception& e) {
        printf("ERROR: %s\n", e.what());
    }
    return 0;
}
