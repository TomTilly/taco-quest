#include "../network.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined(PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <unistd.h>
#include <string.h>
#endif

#define GAME_SIMULATE_TIME_INTERVAL_US 150000

typedef enum {
    SESSION_TYPE_SERVER,
    SESSION_TYPE_CLIENT
} SessionType;

// LIBRARY BEGIN
typedef U8 PacketType;
enum {
    PACKET_TYPE_NONE,
    PACKET_TYPE_ONE,
    PACKET_TYPE_TWO,
};

typedef U8 PacketProgressStage;
enum {
    PACKET_PROGRESS_STAGE_PACKET_HEADER,
    PACKET_PROGRESS_STAGE_PAYLOAD,
    PACKET_PROGRESS_STAGE_COMPLETE,
    PACKET_PROGRESS_STAGE_ERROR,
};

typedef struct {
    U16 size;
    U16 sequence;
    PacketType type;
} PacketHeader;

typedef struct {
    PacketProgressStage stage;
    int progress_bytes;
} PacketTransmissionState;

typedef struct {
    PacketHeader header;
    U8* payload;
} Packet;
// LIBRARY END

typedef struct {
    S32 snake_count;
    S32 taco_count;
} PayloadOne;

typedef struct {
    S32 level_width;
    S32 level_height;
    S8* level_tiles;
} PayloadTwo;

uint64_t microseconds_between_timestamps(struct timespec* previous, struct timespec* current) {
    return (current->tv_sec - previous->tv_sec) * 1000000LL +
           ((current->tv_nsec - previous->tv_nsec)) / 1000;
}

// LIBRARY BEGIN
// TODO: Should we consider combining Packet and PacketTransmissionState.
const char* packet_type_description(PacketType type) {
    switch (type) {
        case PACKET_TYPE_NONE:
            return "none";
        case PACKET_TYPE_ONE:
            return "payload one";
        case PACKET_TYPE_TWO:
            return "payload two";
        default:
            return "unknown";
    }
}

void _execute_receive_stage(NetSocket** socket,
                            Packet* packet,
                            PacketTransmissionState* packet_transmission_state,
                            U8* bytes,
                            int size,
                            PacketProgressStage next_stage) {
    net_log("RECV: %6zu bytes | ", sizeof(packet->header));
    int bytes_received = net_receive(*socket,
                                     bytes + packet_transmission_state->progress_bytes,
                                     size - packet_transmission_state->progress_bytes);
    net_log("%5d bytes - seq %5d (%s)\n",
            bytes_received,
            packet->header.sequence,
            packet_type_description(packet->header.type));

    if (bytes_received == -1) {
        fputs(net_get_error(), stderr);
        net_destroy_socket(*socket);
        *socket = NULL;
        packet_transmission_state->stage = PACKET_PROGRESS_STAGE_ERROR;
        return;
    }

    packet_transmission_state->progress_bytes += bytes_received;
    if (packet_transmission_state->progress_bytes == size) {
        packet_transmission_state->stage = next_stage;
        packet_transmission_state->progress_bytes = 0;
    }
}

void packet_receive(NetSocket** socket,
                    Packet* packet,
                    PacketTransmissionState* packet_transmission_state) {
    if (packet_transmission_state->stage == PACKET_PROGRESS_STAGE_PACKET_HEADER) {
        _execute_receive_stage(socket,
                               packet,
                               packet_transmission_state,
                               (U8*)(&packet->header),
                               sizeof(packet->header),
                               PACKET_PROGRESS_STAGE_PAYLOAD);
        if (packet_transmission_state->stage == PACKET_PROGRESS_STAGE_PAYLOAD) {
            packet->payload = malloc(packet->header.size);
        }
    }

    if (packet_transmission_state->stage == PACKET_PROGRESS_STAGE_PAYLOAD) {
        _execute_receive_stage(socket,
                               packet,
                               packet_transmission_state,
                               packet->payload,
                               packet->header.size,
                               PACKET_PROGRESS_STAGE_COMPLETE);
    }
}
// LIBRARY END

int main(int argc, char** argv) {
    const char* port = NULL;
    const char* ip = NULL;

    NetSocket* server_socket = NULL;
    NetSocket* client_socket = NULL;
    NetSocket* server_client_socket = NULL; // TODO: will be an array to handle multiple connections

    SessionType session_type = SESSION_TYPE_SERVER;

    if (argc > 1) {
        if (strcmp(argv[1], "-s") == 0) {
            session_type = SESSION_TYPE_SERVER;

            if (argc != 3) {
                puts("Expected port argument for server mode");
                return EXIT_FAILURE;
            }

            port = argv[2];
        } else if (strcmp(argv[1], "-c") == 0) {
            session_type = SESSION_TYPE_CLIENT;

            if (argc != 4) {
                puts("Expected ip and port arguments for client mode");
                return EXIT_FAILURE;
            }

            ip = argv[2];
            port = argv[3];
        } else {
            puts("Unexpected argument passed");
            return EXIT_FAILURE;
        }
    } else {
        port = "1234";
        // puts("No arguments passed in.");
        // return EXIT_FAILURE;
    }

    srand((unsigned int)time(0));

    const char* net_log_file_name = session_type == SESSION_TYPE_SERVER ?
        "net_server.log" : "net_client.log";

    if (!net_init(net_log_file_name)) {
        fprintf(stderr, "%s\n", net_get_error());
        return EXIT_FAILURE;
    }

    switch (session_type) {
    case SESSION_TYPE_CLIENT: {
        printf("Starting client session, with address: %s:%s\n", ip, port);

        net_log("CLIENT\n");

        client_socket = net_create_client(ip, port);
        if ( client_socket == NULL ) {
            fprintf(stderr, "%s\n", net_get_error());
            return EXIT_FAILURE;
        }

        printf("Connected to %s:%s\n", ip, port);
        break;
    }
    case SESSION_TYPE_SERVER: {
        printf("Starting server session, with port: %s\n", port);

        net_log("SERVER\n");

        server_socket = net_create_server(port);
        if ( server_socket == NULL ) {
            fputs(net_get_error(), stderr);
            return EXIT_FAILURE;
        }

        break;
    }
    }

    U64 game_tick = 0;
    U16 client_sequence = 0;
    int64_t time_since_update_microseconds = 0;

    struct timespec last_frame_timestamp = {0};
    timespec_get(&last_frame_timestamp, TIME_UTC);

    Packet client_packet = {0};
    PacketTransmissionState client_packet_transmission_state = {
        .stage = PACKET_PROGRESS_STAGE_PACKET_HEADER,
        .progress_bytes = 0,
    };

    while(1) {
        // Calculate how much time has elapsed (in microseconds).
        struct timespec current_frame_timestamp = {0};
        timespec_get(&current_frame_timestamp, TIME_UTC);
        time_since_update_microseconds += microseconds_between_timestamps(&last_frame_timestamp,
                                                                          &current_frame_timestamp);
        // This makes it work on windows, which is <explitive> stupid.
        // time_since_update_microseconds++;
        last_frame_timestamp = current_frame_timestamp;

        // Game tick is less frequent than frames.
        if (session_type == SESSION_TYPE_CLIENT) {
            // Client always attempts to send input and read game state.
            packet_receive(&client_socket,
                           &client_packet,
                           &client_packet_transmission_state);
            if (client_packet_transmission_state.stage == PACKET_PROGRESS_STAGE_COMPLETE) {
                client_packet_transmission_state.stage = PACKET_PROGRESS_STAGE_PACKET_HEADER;
                client_packet_transmission_state.progress_bytes = 0;
            } else if (client_packet_transmission_state.stage == PACKET_PROGRESS_STAGE_ERROR) {
                return EXIT_FAILURE;
            }
        } else if (session_type == SESSION_TYPE_SERVER) {
            // Server sends game state infrequently as part of a tick.
            if (time_since_update_microseconds >= GAME_SIMULATE_TIME_INTERVAL_US) {
                time_since_update_microseconds -= GAME_SIMULATE_TIME_INTERVAL_US;

                game_tick++;
                // Listen for client connections
                if (server_client_socket == NULL) {
                    bool result = net_accept(server_socket, &server_client_socket);
                    if (result) {
                        if (server_client_socket != NULL) {
                            printf("client connected!\n");
                            fflush(stdout);
                        }
                    } else {
                        // fprintf(stderr, "%s\n", net_get_error());
                    }
                } else {
                    // Build a payload for one of 2 message types and send it to the client.
                    int payload_type = rand() % 2;
                    U8* payload = NULL;
                    PacketHeader packet_header;

                    if (payload_type == 0) {
                        PayloadOne* payload_one = malloc(sizeof(PayloadOne));
                        *payload_one = (PayloadOne){
                            .snake_count = rand() % 10,
                            .taco_count = rand() % 20,
                        };

                        packet_header = (PacketHeader){
                            .type = PACKET_TYPE_ONE,
                            .size = sizeof(payload),
                            .sequence = client_sequence++
                        };

                        payload = (U8*)(payload_one);
                    } else {
                        S32 level_width = rand() % 10;
                        S32 level_height = rand() % 10;
                        S32 level_tile_count = level_width * level_height;

                        int level_size = sizeof(level_width) + sizeof(level_height) + (level_width * level_height * sizeof(S8));
                        payload = malloc(level_size);
                        ((PayloadTwo*)(payload))->level_width = level_width;
                        ((PayloadTwo*)(payload))->level_height = level_height;
                        U8* level_tiles = payload + sizeof(level_width) + sizeof(level_height);
                        for (S32 i = 0; i < level_tile_count; i++) {
                            level_tiles[i] = rand() % 256;
                        }

                        packet_header = (PacketHeader){
                            .type = PACKET_TYPE_TWO,
                            .size = (U16)(level_size),
                            .sequence = client_sequence++
                        };
                    }

                    net_log("SEND: %6zu bytes | ", sizeof(packet_header));
                    int bytes_sent = net_send(server_client_socket,
                                              &packet_header,
                                              sizeof(packet_header));
                    net_log("%5d bytes - seq %5d (%s)\n", bytes_sent, packet_header.sequence, "payload one");

                    if (bytes_sent == -1) {
                        fprintf(stderr, "%s\n", net_get_error());
                        // TODO: cleanup server_client_socket
                    } else if (bytes_sent != sizeof(packet_header)) {
                        fprintf(stderr, "sent only %d of %zu bytes of header\n",
                                bytes_sent, sizeof(packet_header));
                    }

                    net_log("SEND: %6zu bytes | ", sizeof(packet_header));
                    bytes_sent = net_send(server_client_socket, &payload, packet_header.size);
                    net_log("%5d bytes - seq %5d (%s)\n", bytes_sent, packet_header.sequence, "payload one");

                    if (bytes_sent == -1) {
                        fprintf(stderr, "%s\n", net_get_error());
                        // TODO: cleanup server_client_socket
                    } else if (bytes_sent != packet_header.size) {
                        fprintf(stderr, "sent only %d of %zu bytes of header\n",
                                bytes_sent, sizeof(packet_header));
                    }

                    free(payload);
                }
            }
        }

#if defined(PLATFORM_WINDOWS)
        Sleep(0);
#else
        // Mac you are on your own.
        usleep(100);
#endif
    }

    switch(session_type) {
    case SESSION_TYPE_CLIENT:
        net_destroy_socket(client_socket);
        break;
    case SESSION_TYPE_SERVER: {
        net_destroy_socket(server_socket);
        if ( server_client_socket ) {
            net_destroy_socket(server_client_socket);
        }
        break;
    }
    }

    net_shutdown();
    return 0;
}
