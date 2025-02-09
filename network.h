#ifndef network_h
#define network_h

#include <stdbool.h>
#include <stddef.h>

#include "ints.h"

#define NET_ERROR_MESSAGE_LEN 128
#define SERVER_ACCEPT_QUEUE_LIMIT 5

typedef struct net_socket NetSocket;

bool        net_init(void);
NetSocket*  net_create_client(const char* ip, const char* port);
NetSocket*  net_create_server(const char* port);
bool        net_accept(NetSocket* server, NetSocket** out);
int         net_send(NetSocket* socket, void* buf, int size);
int         net_receive(NetSocket* sock, void* buf, int size);
void        net_destroy_socket(NetSocket* socket);
const char* net_get_error(void);
void        net_shutdown(void);
void        net_log(const char* format, ...);

#endif /* network_h */
