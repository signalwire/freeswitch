#include <winsock2.h>


int
xmlrpc_win32_socketpair(int    const domain,
                        int    const type,
                        int    const protocol,
                        SOCKET       socks[2]) {
    bool error;

    error = false;  // initial value

    SOCKET listener;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET)
        error = true;
    else {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(0x7f000001);
        addr.sin_port = 0;

        int rc;
        rc = bind(listener, (const struct sockaddr*) &addr, sizeof(addr));
        if (rc == SOCKET_ERROR)
            error = true;
        else {
            int addrlen;
            int rc;
            addrlen = sizeof(addr);  // initial value
            rc = getsockname(listener, (struct sockaddr*) &addr, &addrlen);
            if (rc == SOCKET_ERROR)
                error = true;
            else {
                int rc;

                rc = listen(listener, 1);
                if (rc == SOCKET_ERROR)
                    error = true;
                else {
                    socks[0] = socket(AF_INET, SOCK_STREAM, 0);
                    if (socks[0] == INVALID_SOCKET)
                        error = true;
                    else {
                        int rc;
                        rc = connect(socks[0],
                                     (const struct sockaddr*) &addr,
                                     sizeof(addr));
                        if (rc == SOCKET_ERROR)
                            error = true;
                        else {
                            socks[1] = accept(listener, NULL, NULL);
                            if (socks[1] == INVALID_SOCKET)
                                error = true;
                        }
                        if (error)
                            closesocket(socks[0]);
                    }
                }
            }
        }
        closesocket(listener);
    }
    
    return error ? -1 : 0;
}




