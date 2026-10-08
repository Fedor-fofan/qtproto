#ifndef NETUTILS
#define NETUTILS

#include <WinSock2.h>
#include <stdint.h>
#include <stdio.h>

#define FLAG_FRAME_START 0x0001
#define FLAG_FRAME_END   0x0002

#define WIDTH 640
#define HEIGHT 480
#define BPP 3

#define FRAME_SIZE (WIDTH * HEIGHT * BPP)
#define LINE_SIZE  (WIDTH * BPP)

#define SERVER_PORT       5000
#define CLIENT_VIDEO_PORT 5001

#define VIDEO_HEADER_SIZE 32
#define CHUNK_SIZE        1200

#define MAGIC   0x12

#define VERSION 1

#define VIDEO_TYPE 1

enum Command : unsigned char
{
    CMD_START,
    CMD_STOP,
    CMD_CONFIG,
    CMD_STATUS
};

void write_u16(char unsigned *buffer, int offset, int value);
void write_u32(char unsigned *buffer, int offset, int value);
void write_u64(char unsigned *buffer, int offset, int value);

void send_command_response(SOCKET sock, const sockaddr_in &client, unsigned char command, const char *text);
void send_video_packet(SOCKET sock, const sockaddr_in &client, const unsigned char *payload, uint16_t payload_size,
                        uint32_t stream_id, uint32_t frame_id, uint32_t packet_seq, uint64_t timestamp_us,
                        uint16_t line_number, uint16_t line_offset, uint16_t flags
                    );


#endif