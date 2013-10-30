
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <cstdlib>
#include <cstdio>

using namespace std;

void sig_handler(int sig) {
    printf("signal %d received in thread with id %d\n", sig, pthread_self());
}

void* start_func(void* args) {
    printf("thread with id %d started\n", pthread_self());
    while(true) {
        sleep(3);
    }
}

int main(int argc, char *argv[]) {
    printf("thread with id %d started\n", pthread_self());
    signal(SIGUSR1, sig_handler);
    constexpr int num = 5;
    pthread_t threads[num];
    for(int i = 0; i < num; ++i) {
        pthread_create(&threads[i], nullptr, start_func, nullptr);
    }
    for(int i = 0; i < num; ++i) {
        pthread_join(threads[i], nullptr);
    }
    return 0;
}
