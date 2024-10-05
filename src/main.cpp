#include <signal.h>
#include <string.h>
#include "server/webserver.h"

// 通过信号来关闭服务器
void addsig(int sig, void(handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main()
{
    Webserver server;
    addsig(SIGINT, Webserver::setCloseServer);
    server.eventLoop();
    return 0;
}