#include "common.hxx"

extern "C" {
#include <unistd.h>

#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
}

#include <cerrno>
#include <cstring>

#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
    auto sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UNIX_DOMAIN_SOCK, sizeof(addr.sun_path) - 1);
    bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    listen(sfd, 10);
    bool stop = false;
    while(!stop) {
        auto cfd = accept(sfd, nullptr, nullptr);
        int num = 0;
        char buf[256]{};
        const char* exit_cmd = "exit";
        while((num = read(cfd, buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, num);
            if(num == strlen(exit_cmd)) {
                stop = true;
                break;
            }
        }
        close(cfd);
    }
    close(sfd);
    return 0;
}
