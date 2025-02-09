#include "..\network.h"

#include <assert.h>
#include <stdio.h>
#include <ws2tcpip.h>
#include <winsock2.h>

struct net_socket {
    SOCKET socket;
};

static FILE* log_file;

// TODO: use __thread or similar if we go multithreaded!
static char err_str[NET_ERROR_MESSAGE_LEN] = "No error";
static char windows_error_str[NET_ERROR_MESSAGE_LEN];

static void set_err(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(err_str, NET_ERROR_MESSAGE_LEN, format, args);
    va_end(args);
}

static const char* get_windows_network_error(int last_error) {
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  last_error,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  windows_error_str,
                  NET_ERROR_MESSAGE_LEN,
                  NULL);
    return windows_error_str;
}

const char* net_get_error(void) {
    return err_str;
}

bool net_init(void) {
    WORD wsa_version_requested = MAKEWORD(2, 2);
    WSADATA wsa_data = {0};
    int rc = WSAStartup(wsa_version_requested, &wsa_data);
    if (rc != 0) {
        int last_error = WSAGetLastError();
        set_err("WSAStartup() failed with %s\n", get_windows_network_error(last_error));
        return false;
    }

    log_file = fopen("net.log", "w");
    if (log_file == NULL) {
        set_err("Failed to open net.log\n");
        return false;
    }

    return true;
}

NetSocket*  net_create_client(const char* ip, const char* port) {
    // TODO: Consider consolidating with mac version of func.
    assert(port != NULL);
    assert(ip != NULL);

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC, // don't care IPv4 or IPv6
        .ai_socktype = SOCK_STREAM // TCP stream sockets
    };

    struct addrinfo *server_info;  // will point to the results

    // get ready to connect
    int rc = getaddrinfo(ip, port, &hints, &server_info);
    if (rc != 0) {
        set_err("getaddrinfo error: %s\n", gai_strerror(rc));
        return NULL;
    }

    // Create client socket file descriptor.
    NetSocket* sock = malloc(sizeof(*sock));
    if ( sock == NULL ) {
        set_err("malloc failed: %s\n", strerror(errno));
        return NULL;
    }

    sock->socket = socket(server_info->ai_family,
                      server_info->ai_socktype,
                      (int)server_info->ai_protocol);
    if (sock->socket < 0) {
        int last_error = WSAGetLastError();
        set_err("socket() failed: %s\n", get_windows_network_error(last_error));
        return NULL;
    }

    // TODO: Set to non-blocking?

    rc = connect(sock->socket, server_info->ai_addr, (int)server_info->ai_addrlen);
    if (rc == -1) {
        int last_error = WSAGetLastError();
        set_err("connect error: %s\n", get_windows_network_error(last_error));
        return NULL;
    }

    freeaddrinfo(server_info);
    return sock;
}

NetSocket* net_create_server(const char* port) {
    assert(port != NULL);

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC, // don't care IPv4 or IPv6
        .ai_socktype = SOCK_STREAM, // TCP stream sockets
        .ai_flags = AI_PASSIVE
    };

    struct addrinfo *server_info;  // will point to the results

    // See beej's network guide for more details.
    // Lookup network info for server type socket.
    int rc = getaddrinfo("0.0.0.0", port, &hints, &server_info);
    if (rc != 0) {
        set_err("getaddrinfo error: %s\n", gai_strerror(rc));
        return NULL;
    }

    // Create server socket file descriptor.
    NetSocket* sock = malloc(sizeof(*sock));
    if ( sock == NULL ) {
        set_err("malloc failed: %s\n", strerror(errno));
        return NULL;
    }

    sock->socket = socket(server_info->ai_family,
                      server_info->ai_socktype,
                      server_info->ai_protocol);
    if (sock->socket < 0) {
        int last_error = WSAGetLastError();
        set_err("socket() failed: %s\n", get_windows_network_error(last_error));
        return NULL;
    }

    // Set socket file descriptor to non blocking, so we can call accept() without blocking.
    u_long server_socket_flags = 1;
    rc = ioctlsocket(sock->socket, FIONBIO, &server_socket_flags);
    if (rc != NO_ERROR) {
        int last_error = WSAGetLastError();
        set_err("ioctlsocket() failed with error: %s\n", get_windows_network_error(last_error));
        return NULL;
    }

    // Bind to a specific port.
    rc = bind(sock->socket,
              server_info->ai_addr,
              (int)server_info->ai_addrlen);
    if (rc != 0) {
        int last_error = WSAGetLastError();
        set_err("bind() failed: %s\n", get_windows_network_error(last_error));
        return NULL;
    }

    // Listen on the socket for incoming connections.
    rc = listen(sock->socket, SERVER_ACCEPT_QUEUE_LIMIT);
    if (rc != 0) {
        int last_error = WSAGetLastError();
        set_err("listen() failed: %s\n", get_windows_network_error(last_error));
        return NULL;
    }

    freeaddrinfo(server_info);
    return sock;
}

bool net_accept(NetSocket* server, NetSocket** out) {
    assert(server != NULL);
    assert(out != NULL);

    SOCKET sock = accept(server->socket, NULL, NULL);

    // socket is still -1 on error:
    if (sock == -1) {
        int last_error = WSAGetLastError();
        if (last_error == WSAEWOULDBLOCK) {
            // accept didn't fail, but there was no connection:
            *out = NULL;
            return true;
        }

        // accept failed:
        set_err("accept() failed: %s\n", get_windows_network_error(last_error));
        return false;
    }

    // there was a connection.
    *out = malloc(sizeof(**out));
    if ( *out == NULL ) {
        set_err("malloc failed: %s\n", strerror(errno));
        return false;
    }

    (*out)->socket = sock;
    return true;
}

int net_send(NetSocket* sock, void* buf, int size) {
    assert(socket != NULL);
    assert(buf != NULL);
    assert(size > 0);

    int size_sent = send(sock->socket, (char*)buf, size, 0);
    if (size_sent == -1) {
        int last_error = WSAGetLastError();
        if (last_error == WSAEWOULDBLOCK) {
            return 0;
        }

        set_err("Failed to send data: %s\n", get_windows_network_error(last_error));
        return -1;
    }

    return size_sent;
}

int net_receive(NetSocket* sock, void* buf, int size) {
    assert(socket != NULL);
    assert(buf != NULL);
    assert(size > 0);

    int received = recv(sock->socket, (char*)buf, size, 0);
    if ( received < 0 ) {
        int last_error = WSAGetLastError();
        if (last_error == WSAEWOULDBLOCK) {
            // Received nothing, but socket was non-blocking so it's okay.
            return 0;
        }

        set_err("Error receiving data: %s\n", get_windows_network_error(last_error));
        return -1;
    }

    return received;
}

void net_destroy_socket(NetSocket* socket) {
    assert(socket != NULL);
    closesocket(socket->socket);
    free(socket);
}

void net_shutdown(void) {
    fclose(log_file);
}

void net_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
}
