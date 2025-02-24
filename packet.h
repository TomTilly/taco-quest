#ifndef packet_h
#define packet_h

#include "ints.h"
#include "network.h"

typedef U8 PacketType;
enum {
    PACKET_TYPE_NONE,
    PACKET_TYPE_SNAKE_ACTION,
    PACKET_TYPE_LEVEL_STATE,
    PACKET_TYPE_SNAKE_STATE
};


typedef U8 PacketProgressStage;
enum {
    PACKET_PROGRESS_STAGE_PACKET_HEADER,
    PACKET_PROGRESS_STAGE_PAYLOAD,
    PACKET_PROGRESS_STAGE_COMPLETE,
    PACKET_PROGRESS_STAGE_ERROR,
};

typedef struct {
    U16 payload_size;
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

// TODO: Should we consider combining Packet and PacketTransmissionState.
const char* packet_type_description(PacketType type);

void packet_receive(NetSocket* socket,
                    Packet* packet,
                    PacketTransmissionState* packet_transmission_state);

#endif /* packet_h */
