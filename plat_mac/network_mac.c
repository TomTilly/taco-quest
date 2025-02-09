#include "../network.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

struct net_socket {
    int fd;
};

static FILE* log_file;

// TODO: use __thread or similar if we go multithreaded!
static char err_str[NET_ERROR_MESSAGE_LEN] = "No error";

static void set_err(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(err_str, NET_ERROR_MESSAGE_LEN, format, args);
    va_end(args);
}

// TODO: make error messages more generic, don't mention fcntl etc?

bool net_init(void) {
    log_file = fopen("net.log", "w");

    if (log_file == NULL) {
        set_err("Failed to open net.log\n");
        return false;
    }

    return true;
}

NetSocket* net_create_client(const char* ip, const char* port) {
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

    sock->fd = socket(server_info->ai_family,
                      server_info->ai_socktype,
                      (int)server_info->ai_protocol);
    if (sock->fd < 0) {
        set_err("socket() failed: %s\n", strerror(errno));
        return NULL;
    }

    // TODO: Set to non-blocking?

    rc = connect(sock->fd, server_info->ai_addr, (int)server_info->ai_addrlen);
    if (rc == -1) {
        set_err("connect error: %s\n", strerror(errno));
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

    sock->fd = socket(server_info->ai_family,
                      server_info->ai_socktype,
                      server_info->ai_protocol);
    if (sock->fd < 0) {
        set_err("socket() failed: %s\n", strerror(errno));
        return NULL;
    }

    // Set socket file descriptor to non blocking, so we can call accept() without blocking.
    int server_socket_flags = fcntl(sock->fd, F_GETFL);
    if (server_socket_flags == -1){
        set_err("fcntl(F_GETFL) failed: %s\n", strerror(errno));
        return NULL;
    }

    server_socket_flags |= O_NONBLOCK;
    rc = fcntl(sock->fd, F_SETFL, server_socket_flags);
    if (rc != 0){
        set_err("fcntl(F_SETFL) failed: %s\n", strerror(errno));
        return NULL;
    }

    // Bind to a specific port.
    rc = bind(sock->fd,
              server_info->ai_addr,
              (int)server_info->ai_addrlen);
    if (rc != 0) {
        set_err("bind() failed: %s\n", strerror(errno));
        return NULL;
    }

    // Listen on the socket for incoming connections.
    rc = listen(sock->fd, SERVER_ACCEPT_QUEUE_LIMIT);
    if (rc != 0) {
        set_err("listen() failed: %s\n", strerror(errno));
        return NULL;
    }

    freeaddrinfo(server_info);
    return sock;
}

bool net_accept(NetSocket* server, NetSocket** out) {
    assert(server != NULL);
    assert(out != NULL);

    int fd = accept(server->fd, NULL, NULL);

    // socket is still -1 on error:
    if (fd == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // accept didn't fail, but there was no connection:
            *out = NULL;
            return true;
        }

        // accept failed:
        set_err("accept() failed: %s\n", strerror(errno));
        return false;
    }

    // there was a connection.
    *out = malloc(sizeof(**out));
    if ( *out == NULL ) {
        set_err("malloc failed: %s\n", strerror(errno));
        return false;
    }

    (*out)->fd = fd;
    return true;
}

int net_send(NetSocket* socket, void* buf, int size) {
    assert(socket != NULL);
    assert(buf != NULL);
    assert(size > 0);

    ssize_t size_sent = send(socket->fd, buf, size, 0);
    if (size_sent == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0;
        }

        set_err("Failed to send data: %s\n", strerror(errno));
        return -1;
    }

    return (int)size_sent;
}

int net_receive(NetSocket* socket, void* buf, int size) {
    assert(socket != NULL);
    assert(buf != NULL);
    assert(size > 0);

    ssize_t received = recv(socket->fd, buf, size, 0);
    if ( received < 0 ) {
        if ( errno == EWOULDBLOCK || errno == EAGAIN ) {
            // Received nothing, but socket was non-blocking so it's okay.
            return 0;
        }
        set_err("Error receiving data: %s\n", strerror(errno));
        return -1;
    }

    return (int)received;
}

void net_destroy_socket(NetSocket* socket) {
    assert(socket != NULL);

    close(socket->fd);
    free(socket);

}

const char* net_get_error(void) {
    return err_str;
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
