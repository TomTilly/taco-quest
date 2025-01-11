#include "network.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#define SET_ERR(result, ...) \
    snprintf(result.error_message, NET_ERROR_MESSAGE_LEN, __VA_ARGS__);

// TODO: make error messages more generic, don't mention fcntl etc.

NetInitResult net_init(void) {
    NetInitResult result;
    result.succeeded = true;

    return result;
}

NetCreateSocketResult net_create_client_socket(const char* ip, const char* port) {
    NetCreateSocketResult result = { 0 };

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC, // don't care IPv4 or IPv6
        .ai_socktype = SOCK_STREAM // TCP stream sockets
    };


    struct addrinfo *server_info;  // will point to the results

    // get ready to connect
    int rc = getaddrinfo(ip, port, &hints, &server_info);
    if (rc != 0) {
        snprintf(result.error_message, NET_ERROR_MESSAGE_LEN,
                 "getaddrinfo error: %s\n", gai_strerror(rc));
        result.succeeded = false;
        return result;
    }

    // Create client socket file descriptor.
    result.socket = socket(server_info->ai_family, server_info->ai_socktype, (int)server_info->ai_protocol);
    if (result.socket < 0) {
        snprintf(result.error_message, NET_ERROR_MESSAGE_LEN,
                 "socket() failed: %s\n", strerror(errno));
        result.succeeded = false;
        return result;
    }

    // TODO: Set to non-blocking?

    rc = connect(result.socket, server_info->ai_addr, (int)server_info->ai_addrlen);
    if (rc == -1) {
        snprintf(result.error_message, NET_ERROR_MESSAGE_LEN,
                 "connect error: %s\n", strerror(errno));
        result.succeeded = false;
        return result;
    }

    freeaddrinfo(server_info);

    result.succeeded = true;
    return result;
}

NetCreateSocketResult net_create_server_socket(const char* port) {
    NetCreateSocketResult result = { 0 };

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
        result.succeeded = false;
        snprintf(result.error_message, NET_ERROR_MESSAGE_LEN, "getaddrinfo error: %s\n", gai_strerror(rc));
        return result;
    }

    // Create server socket file descriptor.
    result.socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (result.socket < 0) {
        result.succeeded = false;
        snprintf(result.error_message, NET_ERROR_MESSAGE_LEN, "socket() failed: %s\n", strerror(errno));
        return result;
    }

    // Set socket file descriptor to non blocking, so we can call accept() without blocking.
    int server_socket_flags = fcntl(result.socket, F_GETFL);
    if (server_socket_flags == -1){
        result.succeeded = false;
        snprintf(result.error_message, NET_ERROR_MESSAGE_LEN, "fcntl(F_GETFL) failed: %s\n", strerror(errno));
        return result;
    }
    server_socket_flags |= O_NONBLOCK;
    rc = fcntl(result.socket, F_SETFL, server_socket_flags);
    if (rc != 0){
        result.succeeded = false;
        snprintf(result.error_message, NET_ERROR_MESSAGE_LEN, "fcntl(F_SETFL) failed: %s\n", strerror(errno));
        return result;
    }

    // Bind to a specific port.
    rc = bind(result.socket, server_info->ai_addr, (int)server_info->ai_addrlen);
    if (rc != 0) {
        result.succeeded = false;
        snprintf(result.error_message, NET_ERROR_MESSAGE_LEN, "bind() failed: %s\n", strerror(errno));
        return result;
    }

    // Listen on the socket for incoming connections.
    rc = listen(result.socket, SERVER_ACCEPT_QUEUE_LIMIT);
    if (rc != 0) {
        result.succeeded = false;
        snprintf(result.error_message, NET_ERROR_MESSAGE_LEN, "listen() failed: %s\n", strerror(errno));
        return result;
    }

    freeaddrinfo(server_info);

    result.succeeded = true;
    return result;
}

NetCreateSocketResult net_server_accept(NetSocket socket) {
    NetCreateSocketResult result = { 0 };

    result.socket = accept(socket, NULL, NULL);

    // socket is still -1 on error:
    if (result.socket == NET_INVALID_SOCKET) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            SET_ERR(result, "accept() failed: %s\n", strerror(errno));
            return result;
        }
    }

    result.succeeded = true;
    return result;
}

NetTransferResult net_send(NetSocket socket, void* data, size_t size) {
    NetTransferResult result = { 0 };

    // TODO: handle EWOULDBLOCK and EAGAIN like in net_receive
    ssize_t size_sent = send(socket, data, size, 0);
    if ( size_sent == -1 ) {
        SET_ERR(result, "Failed to send data");
        return result;
    }

    result.succeeded = true;
    result.num_bytes = (size_t)size_sent;
    return result;
}

NetTransferResult net_receive(NetSocket socket, void* data, size_t size) {
    NetTransferResult result;

    ssize_t received = recv(socket, data, size, 0);
    if ( received < 0 ) {
        if ( errno == EWOULDBLOCK || errno == EAGAIN ) {
            // Received nothing, but socket was non-blocking so it's okay.
            result.num_bytes = 0;
        } else {
            SET_ERR(result, "Net error receiving data: %s", strerror(errno));
            return result;
        }
    } else {
        result.num_bytes = (size_t)received;
    }

    result.succeeded = true;
    return result;
}

void net_close(NetSocket socket) {
    close(socket);
}
