#if defined(PLATFORM_WINDOWS)
#include "network.h"
#include <stdio.h>
#include <ws2tcpip.h>
#include <winsock2.h>

NetInitResult net_init(void) {
    NetInitResult result;
    WORD wsa_version_requested = MAKEWORD(2, 2);
    WSADATA wsa_data = {0};
    int rc = WSAStartup(wsa_version_requested, &wsa_data);
    if (rc != 0) {
        snprintf(result.error_message, NET_ERROR_MESSAGE_LEN, "WSAStartup() failed with %d", rc);
        result.succeeded = false;
    } else {
        result.succeeded = true;
    }
    
    return result;
}
#endif
