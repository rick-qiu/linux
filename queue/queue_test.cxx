#include "queue.hxx"

#include <pthread.h>

#include <cstdio>

#include <limits>

using namespace linux::queue;
using namespace std;

struct object {
    int id;
    int age;
};

volatile bool stop = false;
sr_sw_queue<object> object_queue;
sr_mw_queue<object> object_queue2;

void* thread_func(void* arg) {
    printf("new thread is running!\n");
    object* p(nullptr);
    while(!stop) {
        if(object_queue.remove(p)) {
            printf("consume object id: %d\n", p->id);
            delete p;
        }
    }
}

int main(int argc, char *argv[]) {
    pthread_t thread_id;
    pthread_create(&thread_id, nullptr, thread_func, nullptr);
    int id = 0;
    while(id < numeric_limits<char>::max()) {
        auto p = new object();
        p->id = id;
        printf("produce object id: %d\n", p->id);
        while(!object_queue.add(p)) {
        }
        ++id;
    }
    stop = true;
    pthread_join(thread_id, nullptr);
    return 0;
}
