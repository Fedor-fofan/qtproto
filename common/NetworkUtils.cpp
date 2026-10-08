#include "NetworkUtils.hpp"

void write_u16(
    char *buffer,
    int offset,
    uint16_t value
)
{
    uint16_t network_value =
        htons(value);

    memcpy(
        buffer + offset,
        &network_value,
        sizeof(network_value)
    );
}


void write_u32(
    char *buffer,
    int offset,
    uint32_t value
)
{
    uint32_t network_value =
        htonl(value);

    memcpy(
        buffer + offset,
        &network_value,
        sizeof(network_value)
    );
}


void write_u64(
    char *buffer,
    int offset,
    uint64_t value
)
{

    uint32_t high =
        static_cast<uint32_t>(
            value >> 32
        );

    uint32_t low =
        static_cast<uint32_t>(
            value & 0xFFFFFFFF
        );

    write_u32(
        buffer,
        offset,
        high
    );

    write_u32(
        buffer,
        offset + 4,
        low
    );
}

void send_command_response(
    SOCKET command_socket,
    const sockaddr_in &client,
    unsigned char command,
    const char *text
)
{
    char buffer[512];

    buffer[0] = command;

    int text_size =
        static_cast<int>(
            strlen(text)
        );

    if (text_size > 510)
        text_size = 510;

    memcpy(
        buffer + 1,
        text,
        text_size
    );

    sendto(
        command_socket,
        buffer,
        1 + text_size,
        0,
        (const sockaddr *)&client,
        sizeof(client)
    );
}

void send_video_packet(
    SOCKET video_socket,

    const sockaddr_in &client,

    const unsigned char *payload,

    uint16_t payload_size,

    uint32_t stream_id,
    uint32_t frame_id,
    uint32_t packet_seq,

    uint64_t timestamp_us,

    uint16_t line_number,
    uint16_t line_offset,

    uint16_t flags
)
{
    char packet[
        VIDEO_HEADER_SIZE +
        CHUNK_SIZE
    ];

    write_u16(
        packet,
        0,
        MAGIC
    );

    packet[2] =
        VERSION;

    packet[3] =
        VIDEO_TYPE;

    write_u32(
        packet,
        4,
        stream_id
    );

    write_u32(
        packet,
        8,
        frame_id
    );

    write_u32(
        packet,
        12,
        packet_seq
    );

    write_u64(
        packet,
        16,
        timestamp_us
    );

    write_u16(
        packet,
        24,
        line_number
    );

    write_u16(
        packet,
        26,
        line_offset
    );

    write_u16(
        packet,
        28,
        payload_size
    );

    write_u16(
        packet,
        30,
        flags
    );

    memcpy(
        packet + VIDEO_HEADER_SIZE,
        payload,
        payload_size
    );

    int result = sendto(
        video_socket,
        packet,
        VIDEO_HEADER_SIZE +
            payload_size,
        0,
        (const sockaddr *)&client,
        sizeof(client)
    );

    if (result == SOCKET_ERROR)
    {
        printf(
            "video sendto error: %d\n",
            WSAGetLastError()
        );
    }
}