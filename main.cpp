#include <QApplication>
#include <QLabel>
#include <QPixmap>
#include <QImage>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QPlainTextEdit>

#include <QUdpSocket>
#include <QHostAddress>
#include <QtEndian>

#include <stdio.h>
#include <string.h>

#define WIDTH 640
#define HEIGHT 480
#define BPP 3

#define CLIENT_PORT 5001
#define SERVER_PORT 5000

#define FRAME_SIZE (WIDTH * HEIGHT * BPP)

#define CHUNK_SIZE 1200
#define COMMAND_HEADE_SIZE 1
#define VIDEO_HEADER_SIZE 32

#define MAGIC 0x12

enum Command : unsigned char
{
    CMD_START,
    CMD_STOP,
    CMD_CONFIG,
    CMD_STATUS
};

void write_u32(QByteArray &buffer, int offset, quint32 value){
    quint32 network_value = qToBigEndian(value);

    memcpy(buffer.data() + offset, &network_value, sizeof(network_value));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QWidget window;

    QVBoxLayout layout;
    QLabel label;

    QPushButton start_button("Start");
    QPushButton stop_button("Stop");
    QPushButton status_button("Status");
    QPushButton config_button("Config");
    QPlainTextEdit info_widget;
    info_widget.setReadOnly(true);

    layout.addWidget(&start_button);
    layout.addWidget(&stop_button);
    layout.addWidget(&label);
    layout.addWidget(&config_button);
    layout.addWidget(&status_button);
    layout.addWidget(&info_widget);

    window.setLayout(&layout);

    label.setFixedSize(
        WIDTH,
        HEIGHT
    );

    QUdpSocket command_socket, video_socket;

    video_socket.bind(
        QHostAddress::AnyIPv4,
        CLIENT_PORT
    );

    command_socket.bind(
        QHostAddress::AnyIPv4,
        5002
    );

    QHostAddress serverAddress(
        "127.0.0.1"
    );

    QByteArray frameBuffer(
        FRAME_SIZE,
        0
    );

    /*
    
    VIDEOPACKET_HEADER

    Структура пакета:

    TYPE         --  offset  -- size 
    MAGIC        --     0        2
    version      --     2        1
    type         --     3        1
    stream_id    --     4        4
    frame_id     --     8        4
    packet_seq   --     12       4
    timestamp_us --     16       8
    line_number  --     24       2
    line_offset  --     26       2
    payload_size --     28       2
    flags        --     30       2
    
    HEADERSIZE -- 32 bytes
    */

    QObject::connect(
        &start_button,
        &QPushButton::clicked,
        [&](){

            QByteArray command(1, 0);
            command[0] = CMD_START;

            command_socket.writeDatagram(   // команда отправляется на сервер на порт 5000
                command, 
                serverAddress,
                SERVER_PORT
            );

            printf("START sent\n\r");
        }
    );

    QObject::connect(
        &stop_button,
        &QPushButton::clicked,
        [&](){

            QByteArray command(1, 0);

            command[0] = CMD_STOP;

            command_socket.writeDatagram(
                command,
                serverAddress,
                SERVER_PORT
            );

            printf("STOP sent\n\t");
        }
    );

    QObject::connect(
        &stop_button,
        &QPushButton::clicked,
        [&](){

            QByteArray command(1, 0);

            command[0] = CMD_STATUS;

            command_socket.writeDatagram(
                command,
                serverAddress,
                SERVER_PORT
            );

            printf("STOP sent\n\t");
        }
    );

    QObject::connect(
        &stop_button,
        &QPushButton::clicked,
        [&](){

            QByteArray command(1, 0);

            command[0] = CMD_CONFIG;

            command_socket.writeDatagram(
                command,
                serverAddress,
                SERVER_PORT
            );

            printf("STOP sent\n\t");
        }
    );

    QByteArray framebuffer(FRAME_SIZE, 0);

    bool first_frame = true;
    int CurrentFrameId, CurrentStreamId;


    QObject::connect(
    &command_socket,
    &QUdpSocket::readyRead,
    [&]()
    {
        while (command_socket.hasPendingDatagrams())
        {
            QByteArray datagram;

            datagram.resize(
                command_socket.pendingDatagramSize()
            );

            // ВАЖНО: сначала реально читаем UDP-пакет
            command_socket.readDatagram(
                datagram.data(),
                datagram.size()
            );

            if (datagram.size() < 1)
                continue;

            const uchar *data =
                reinterpret_cast<const uchar *>(
                    datagram.constData()
                );

            quint8 command_type =
                qFromBigEndian<quint8>(data);

            // Всё после первого байта считаем текстом
            QString response =
                QString::fromUtf8(
                    datagram.constData() + 1,
                    datagram.size() - 1
                );

            switch (command_type)
            {
                case CMD_CONFIG:
                {
                    info_widget.appendPlainText(
                        "CONFIG:"
                    );

                    info_widget.appendPlainText(
                        response
                    );

                    break;
                }

                case CMD_STATUS:
                {
                    info_widget.appendPlainText(
                        "STATUS:"
                    );

                    info_widget.appendPlainText(
                        response
                    );

                    break;
                }

                default:
                {
                    info_widget.appendPlainText(
                        "Unknown command response"
                    );

                    break;
                }
            }
        }
    }
);

    QObject::connect(
        &video_socket,
        &QUdpSocket::readyRead,
        [&](){
            while(video_socket.hasPendingDatagrams()){
                
                QByteArray datagram;

                datagram.resize(video_socket.pendingDatagramSize());

                video_socket.readDatagram(datagram.data(), datagram.size());

                const uchar *data = (uchar *)datagram.data();
                
                quint16 magic = qFromBigEndian<quint16>(data);

                if(magic != MAGIC) continue;

                // парсинг заголовка
                quint8 version = qFromBigEndian<quint8>(data + 2);
                quint8 type = qFromBigEndian<quint8>(data + 3);

                quint32 stream_id = qFromBigEndian<quint32>(data+4);
                quint32 frame_id = qFromBigEndian<quint32>(data+8);
                quint32 packet_seq = qFromBigEndian<quint32>(data+12);
                quint64 timestamp = qFromBigEndian<quint64>(data+16);

                quint16 line_number = qFromBigEndian<quint16>(data+24);
                quint16 line_offset = qFromBigEndian<quint16>(data+26);

                quint16 payload_size = qFromBigEndian<quint16>(data+28);
                quint16 flags = qFromBigEndian<quint16>(data+30);

                // если первый пакет обнуляем frame_id
                if(first_frame){
                    CurrentFrameId = frame_id;
                    CurrentStreamId = stream_id;

                    first_frame = false;
                }

                if (stream_id != CurrentStreamId){
                        printf(
                            "New stream: %u -> %u\n",
                            CurrentStreamId,
                            stream_id
                        );

                        CurrentStreamId = stream_id;
                        CurrentFrameId = frame_id;

                        framebuffer.fill(0);
                }

                // если приходят пакеты следующего кадра отрисовываем прошлый
                if(frame_id > CurrentFrameId){
                    QImage image((const uchar *)framebuffer.constData(), 
                                WIDTH, HEIGHT, WIDTH*BPP, QImage::Format_RGB888);

                    label.setPixmap(QPixmap::fromImage(image));

                    CurrentFrameId = frame_id;
                }

                int offset = line_number * WIDTH * BPP + line_offset;
                memcpy(framebuffer.data() + offset, datagram.constData() + VIDEO_HEADER_SIZE, payload_size);
            }
        }
    );

    window.show();

    return app.exec();
}