#include "packet.h"

#include <stdlib.h>

const char* packet_type_description(PacketType type) {
    switch (type) {
        case PACKET_TYPE_NONE:
            return "none";
        case PACKET_TYPE_SNAKE_ACTION:
            return "snake action";
        case PACKET_TYPE_LEVEL_STATE:
            return "level state";
        case PACKET_TYPE_SNAKE_STATE:
            return "snake";
        default:
            return "unknown";
    }
}

// TODO: temp or put these in a header.
char* get_timestamp(void);
void net_action_log(const char* timestamp_str,
                    const char* type,
                    size_t bytes_to_send,
                    size_t bytes_sent,
                    int seq,
                    const char* desc);

void _execute_receive_stage(NetSocket* socket,
                            Packet* packet,
                            PacketTransmissionState* packet_transmission_state,
                            U8* bytes,
                            int size,
                            PacketProgressStage next_stage) {
    const char* timestamp_str = get_timestamp();
    int bytes_received = net_receive(socket,
                                     bytes + packet_transmission_state->progress_bytes,
                                     size - packet_transmission_state->progress_bytes);

    if (bytes_received == -1) {
        packet_transmission_state->stage = PACKET_PROGRESS_STAGE_ERROR;
        return;
    }

    if (bytes_received == 0) {
        return;
    }

    packet_transmission_state->progress_bytes += bytes_received;
    if (packet_transmission_state->progress_bytes == size) {
        packet_transmission_state->stage = next_stage;
        packet_transmission_state->progress_bytes = 0;
        net_action_log(timestamp_str,
                       "RECV",
                       size,
                       bytes_received,
                       packet->header.sequence,
                       packet_type_description(packet->header.type));
    } else {
        net_action_log(timestamp_str,
                       "RECV",
                       size,
                       bytes_received,
                       -1,
                       packet_type_description(packet->header.type));
    }
}

void packet_receive(NetSocket* socket,
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
            packet->payload = malloc(packet->header.payload_size);
        }
    }

    if (packet_transmission_state->stage == PACKET_PROGRESS_STAGE_PAYLOAD) {
        _execute_receive_stage(socket,
                               packet,
                               packet_transmission_state,
                               packet->payload,
                               packet->header.payload_size,
                               PACKET_PROGRESS_STAGE_COMPLETE);
    }
}
