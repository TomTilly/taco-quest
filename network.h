#ifndef network_h
#define network_h

#include <stdbool.h>
#include <stddef.h>

#include "ints.h"

#define NET_ERROR_MESSAGE_LEN 128

#if defined(PLATFORM_WINDOWS)
#define NET_INVALID_SOCKET INVALID_SOCKET
typedef SOCKET NetSocket;
#else
#define NET_INVALID_SOCKET -1
typedef int NetSocket;
#endif

typedef struct {
    char error_message[NET_ERROR_MESSAGE_LEN];
    bool succeeded;
} NetInitResult;

typedef struct {
    char error_message[NET_ERROR_MESSAGE_LEN];
    bool succeeded;
    NetSocket socket;
} NetCreateSocketResult;

typedef struct {
    char error_message[NET_ERROR_MESSAGE_LEN];
    bool succeeded;
    size_t num_bytes;
} NetTransferResult;


NetInitResult net_init(void);
NetCreateSocketResult net_create_client_socket(const char* ip, const char* port);
// TODO: Consider supporting non-blocking parameter in the future
NetCreateSocketResult net_create_server_socket(const char* port);
NetCreateSocketResult net_server_accept(NetSocket socket);
// TODO: Add loop version of send and receive (?)
NetTransferResult net_send(NetSocket socket, void* data, size_t size);
NetTransferResult net_receive(NetSocket socket, void* data, size_t size);
void net_close(NetSocket* socket);

#endif /* network_h */
