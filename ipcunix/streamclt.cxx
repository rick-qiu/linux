#include "common.hxx"

// system header files
// already with extern "C"
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
//end of system header files

#include <cerrno>
#include <cstring>

#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
    auto sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path[1], UNIX_DOMAIN_SOCK, sizeof(addr.sun_path) - 1);
    connect(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    int num = 0;
    char buf[256];
    while((num = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        write(sfd, buf, num);
    }
    return 0;
}
