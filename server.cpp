#include <WinSock2.h>
#include <WS2tcpip.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <thread>

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

#define FPS 30

#define FLAG_FRAME_START 0x0001
#define FLAG_FRAME_END   0x0002


enum Command : unsigned char
{
    CMD_START,
    CMD_STOP,
    CMD_CONFIG,
    CMD_STATUS
};


// ============================================================
// Запись чисел в network byte order
// ============================================================

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
    /*
        Записываем uint64 как:

        high 32 bits
        low  32 bits

        оба в network byte order.
    */

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


// ============================================================
// Рисование тестового квадрата
// ============================================================

void draw_rect(
    unsigned char *frame,
    int x,
    int y,
    int width,
    int height
)
{
    for (int py = y; py < y + height; py++)
    {
        for (int px = x; px < x + width; px++)
        {
            if (
                px < 0 || px >= WIDTH ||
                py < 0 || py >= HEIGHT
            )
                continue;

            int offset =
                (py * WIDTH + px) * BPP;

            frame[offset + 0] = 255;
            frame[offset + 1] = 0;
            frame[offset + 2] = 0;
        }
    }
}


void update_rect(
    unsigned char *frame,
    int &x,
    int &y,
    int width,
    int height,
    int &speed_x,
    int &speed_y
)
{
    x += speed_x;
    y += speed_y;

    if (x + width >= WIDTH)
    {
        x = WIDTH - width;
        speed_x = -speed_x;
    }

    if (x <= 0)
    {
        x = 0;
        speed_x = -speed_x;
    }

    if (y + height >= HEIGHT)
    {
        y = HEIGHT - height;
        speed_y = -speed_y;
    }

    if (y <= 0)
    {
        y = 0;
        speed_y = -speed_y;
    }

    draw_rect(
        frame,
        x,
        y,
        width,
        height
    );
}


// ============================================================
// Ответ на CONFIG / STATUS
//
// Формат:
//
// byte 0     command
// byte 1...  UTF-8 / ASCII text
// ============================================================

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


// ============================================================
// Отправка одного VIDEO packet
// ============================================================

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

    /*
        HEADER:

        offset  size

        0       2  magic
        2       1  version
        3       1  type

        4       4  stream_id
        8       4  frame_id
        12      4  packet_seq

        16      8  timestamp_us

        24      2  line_number
        26      2  line_offset

        28      2  payload_size
        30      2  flags
    */

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


// ============================================================
// main
// ============================================================

int main()
{
    WSADATA wsadata;

    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsadata
        ) != 0
    )
    {
        printf("WSAStartup error\n");
        return 1;
    }


    // ========================================================
    // COMMAND SOCKET
    // ========================================================

    SOCKET command_socket =
        socket(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP
        );

    if (
        command_socket ==
        INVALID_SOCKET
    )
    {
        printf(
            "command socket error: %d\n",
            WSAGetLastError()
        );

        WSACleanup();
        return 1;
    }


    sockaddr_in server_addr{};

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(SERVER_PORT);

    server_addr.sin_addr.s_addr =
        INADDR_ANY;


    if (
        bind(
            command_socket,
            (sockaddr *)&server_addr,
            sizeof(server_addr)
        ) == SOCKET_ERROR
    )
    {
        printf(
            "bind error: %d\n",
            WSAGetLastError()
        );

        closesocket(command_socket);
        WSACleanup();

        return 1;
    }


    /*
        COMMAND socket делаем non-blocking.

        Иначе recvfrom() остановит генерацию
        видео пока команда не придёт.
    */

    u_long non_blocking = 1;

    ioctlsocket(
        command_socket,
        FIONBIO,
        &non_blocking
    );


    // ========================================================
    // VIDEO SOCKET
    // ========================================================

    SOCKET video_socket =
        socket(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP
        );

    if (
        video_socket ==
        INVALID_SOCKET
    )
    {
        printf(
            "video socket error: %d\n",
            WSAGetLastError()
        );

        closesocket(command_socket);
        WSACleanup();

        return 1;
    }


    printf(
        "Command server listening on UDP %d\n",
        SERVER_PORT
    );


    // ========================================================
    // Stream state
    // ========================================================

    bool streaming = false;
    bool client_known = false;

    sockaddr_in video_client{};

    uint32_t stream_id = 0;
    uint32_t frame_id = 0;
    uint32_t packet_seq = 0;


    // ========================================================
    // Test image state
    // ========================================================

    static unsigned char frame[
        FRAME_SIZE
    ];

    int rect_x = 0;
    int rect_y = 0;

    int rect_width = 200;
    int rect_height = 200;

    int speed_x = 3;
    int speed_y = 3;

    int rx, ry, sx=10, sy=10;


    auto server_start =
        std::chrono::steady_clock::now();

    auto next_frame_time =
        std::chrono::steady_clock::now();


    // ========================================================
    // MAIN LOOP
    // ========================================================

    while (1)
    {
        // ====================================================
        // Проверяем COMMAND packets
        // ====================================================

        while (1)
        {
            char command_buffer[512];

            sockaddr_in sender{};

            int sender_size =
                sizeof(sender);

            int received =
                recvfrom(
                    command_socket,
                    command_buffer,
                    sizeof(command_buffer),
                    0,
                    (sockaddr *)&sender,
                    &sender_size
                );


            if (
                received ==
                SOCKET_ERROR
            )
            {
                int error =
                    WSAGetLastError();

                if (
                    error ==
                    WSAEWOULDBLOCK
                )
                {
                    /*
                        Команд сейчас нет.
                    */
                    break;
                }

                printf(
                    "recvfrom error: %d\n",
                    error
                );

                break;
            }


            if (received < 1)
                continue;


            unsigned char command =
                static_cast<
                    unsigned char
                >(
                    command_buffer[0]
                );


            // ================================================
            // START
            // ================================================

            if (
                command ==
                CMD_START
            )
            {
                /*
                    IP клиента берём из команды.

                    Video отправляем на тот же IP,
                    но порт 5001.
                */

                video_client =
                    sender;

                video_client.sin_port =
                    htons(
                        CLIENT_VIDEO_PORT
                    );

                client_known = true;
                streaming = true;

                stream_id++;

                frame_id = 0;
                packet_seq = 0;

                char ip[
                    INET_ADDRSTRLEN
                ];

                inet_ntop(
                    AF_INET,
                    &video_client.sin_addr,
                    ip,
                    sizeof(ip)
                );

                printf(
                    "START from %s\n",
                    ip
                );
            }


            // ================================================
            // STOP
            // ================================================

            else if (
                command ==
                CMD_STOP
            )
            {
                streaming = false;

                printf("STOP\n");
            }


            // ================================================
            // CONFIG
            // ================================================

            else if (
                command ==
                CMD_CONFIG
            )
            {
                char text[256];

                snprintf(
                    text,
                    sizeof(text),

                    "Resolution: %dx%d\n"
                    "Format: RGB888\n"
                    "BPP: %d\n"
                    "FPS: %d\n"
                    "Payload: %d bytes\n"
                    "Video port: %d",

                    WIDTH,
                    HEIGHT,
                    BPP,
                    FPS,
                    CHUNK_SIZE,
                    CLIENT_VIDEO_PORT
                );

                send_command_response(
                    command_socket,
                    sender,
                    CMD_CONFIG,
                    text
                );

                printf(
                    "CONFIG response sent\n"
                );
            }


            // ================================================
            // STATUS
            // ================================================

            else if (
                command ==
                CMD_STATUS
            )
            {
                char text[256];

                snprintf(
                    text,
                    sizeof(text),

                    "Streaming: %s\n"
                    "Stream ID: %u\n"
                    "Frame ID: %u\n"
                    "Packet sequence: %u",

                    streaming
                        ? "ON"
                        : "OFF",

                    stream_id,
                    frame_id,
                    packet_seq
                );

                send_command_response(
                    command_socket,
                    sender,
                    CMD_STATUS,
                    text
                );

                printf(
                    "STATUS response sent\n"
                );
            }
        }


        // ====================================================
        // Если поток выключен
        // ====================================================

        if (
            !streaming ||
            !client_known
        )
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1)
            );

            continue;
        }


        // ====================================================
        // Ограничение FPS
        // ====================================================

        auto now =
            std::chrono::steady_clock::now();

        if (
            now <
            next_frame_time
        )
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1)
            );

            continue;
        }


        next_frame_time =
            now +
            std::chrono::microseconds(
                1000000 / FPS
            );


        // ====================================================
        // Создаём кадр
        // ====================================================

        memset(
            frame,
            0,
            FRAME_SIZE
        );

        update_rect(
            frame,
            rx,
            ry,
            50,
            50,
            sx,
            sy
        );
        
        update_rect(
            frame,
            rect_x,
            rect_y,
            rect_width,
            rect_height,
            speed_x,
            speed_y
        );

        // ====================================================
        // Timestamp для всего кадра
        // ====================================================

        uint64_t timestamp_us =
            static_cast<uint64_t>(
                std::chrono::
                    duration_cast<
                        std::chrono::
                            microseconds
                    >(
                        now -
                        server_start
                    )
                    .count()
            );


        // ====================================================
        // Отправляем кадр ПО СТРОКАМ
        // ====================================================

        for (
            uint16_t line = 0;
            line < HEIGHT;
            line++
        )
        {
            unsigned char *line_data =
                frame +
                line * LINE_SIZE;


            /*
                RGB888:

                WIDTH = 640

                line size:
                640 * 3 = 1920 bytes

                CHUNK_SIZE = 1200

                Значит:

                packet #1:
                    offset = 0
                    size   = 1200

                packet #2:
                    offset = 1200
                    size   = 720
            */

            for (
                uint16_t line_offset = 0;

                line_offset <
                LINE_SIZE;

                line_offset +=
                    CHUNK_SIZE
            )
            {
                int remaining =
                    LINE_SIZE -
                    line_offset;

                uint16_t payload_size =
                    static_cast<uint16_t>(
                        remaining >
                                CHUNK_SIZE
                            ? CHUNK_SIZE
                            : remaining
                    );


                uint16_t flags = 0;


                if (
                    line == 0 &&
                    line_offset == 0
                )
                {
                    flags |=
                        FLAG_FRAME_START;
                }


                if (
                    line ==
                        HEIGHT - 1 &&
                    line_offset +
                            payload_size >=
                        LINE_SIZE
                )
                {
                    flags |=
                        FLAG_FRAME_END;
                }


                send_video_packet(
                    video_socket,
                    video_client,

                    line_data +
                        line_offset,

                    payload_size,

                    stream_id,
                    frame_id,
                    packet_seq,

                    timestamp_us,

                    line,
                    line_offset,

                    flags
                );


                packet_seq++;
            }
        }


        printf(
            "\rstream=%u frame=%u seq=%u x=%d y=%d",
            stream_id,
            frame_id,
            packet_seq,
            rect_x,
            rect_y
        );

        fflush(stdout);


        frame_id++;
    }


    closesocket(
        video_socket
    );

    closesocket(
        command_socket
    );

    WSACleanup();

    return 0;
}