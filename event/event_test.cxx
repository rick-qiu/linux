#include "event.hxx"

#include <unistd.h>

#include <cstdlib>
#include <cstdio>

int main(int argc, char *argv[]) {
    using namespace std;
    using namespace linux::event;
    try {
        timer_trigger tt(5, [](){ printf("timer fired\n");});
        thread t;
        t.register_trigger(tt);
        sleep(60);
    } catch(thread_exception& e) {
        printf("ERROR: %s\n", e.what());
    } catch(timer_exception& e) {
        printf("ERROR: %s\n", e.what());
    }
    return 0;
}
