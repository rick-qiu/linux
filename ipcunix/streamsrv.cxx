#include "common.hxx"

#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>

#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
    auto sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], UNIX_DOMAIN_SOCK, sizeof(addr.sun_path) - 1);
    bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    listen(sfd, 10);
    bool stop = false;
    while(!stop) {
        auto cfd = accept(sfd, nullptr, nullptr);
        int num = 0;
        char buf[256]{};
        char echo[]{"echo: "};
        while((num = read(cfd, buf, sizeof(buf))) > 0) {
            write(cfd, echo, sizeof(echo));
            write(cfd, buf, num);
        }
        close(cfd);
    }
    close(sfd);
    return 0;
}
