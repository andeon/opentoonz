

#include "ttcpip.h"
#include "tconvert.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#ifndef _WIN32
#define SOCKET_ERROR -1
#endif

//------------------------------------------------------------------------------
// Initialization and cleanup of WSA on Windows
TTcpIpClient::TTcpIpClient() {
#ifdef _WIN32
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSAStartup(wVersionRequested, &wsaData);
#endif
}

TTcpIpClient::~TTcpIpClient() {
#ifdef _WIN32
    WSACleanup();
#endif
}

//------------------------------------------------------------------------------
// Modernized connect function using getaddrinfo
int TTcpIpClient::connect(const QString &hostName, const QString &addrStr,
                          int port, int &sock) {
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;       // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Convert port to string
    std::string portStr = std::to_string(port);

    struct addrinfo* result = nullptr;
    int res = getaddrinfo(hostName.toUtf8().constData(), portStr.c_str(), &hints, &result);
    if (res != 0 || !result) {
        return HOST_UNKNOWN;
    }

    int socket_id = -1;
    struct addrinfo* ptr = result;

    // Try connecting to each returned address
    for (; ptr != nullptr; ptr = ptr->ai_next) {
        socket_id = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (socket_id == SOCKET_ERROR) continue;

        if (::connect(socket_id, ptr->ai_addr, (int)ptr->ai_addrlen) != SOCKET_ERROR) {
            break; // Connected successfully
        }

#ifdef _WIN32
        closesocket(socket_id);
#else
        close(socket_id);
#endif
        socket_id = -1;
    }

    freeaddrinfo(result);

    if (socket_id == -1) {
        sock = -1;
        return CONNECTION_FAILED;
    }

    sock = socket_id;
    return OK;
}

//------------------------------------------------------------------------------
// Disconnect function
int TTcpIpClient::disconnect(int sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    return OK;
}

//------------------------------------------------------------------------------
// Data send function
int TTcpIpClient::send(int sock, const QString &data) {
    std::string dataUtf8 = data.toStdString();

    QString header("#$#THS01.00");
    header += QString::number((int)dataUtf8.size());
    header += "#$#THE";

    std::string packet = header.toStdString() + dataUtf8;

    int nLeft = (int)packet.size();
    int idx = 0;

    while (nLeft > 0) {
#ifdef _WIN32
        int ret = ::send(sock, packet.c_str() + idx, nLeft, 0);
#else
        int ret = write(sock, packet.c_str() + idx, nLeft);
#endif
        if (ret == SOCKET_ERROR) {
            return SEND_FAILED;
        }
        nLeft -= ret;
        idx += ret;
    }

    shutdown(sock, 1);
    return OK;
}

//------------------------------------------------------------------------------
// Data read function
static int readData(int sock, QString &data) {
    int cnt = 0;
    char buff[1024];

    memset(buff, 0, sizeof(buff));

#ifdef _WIN32
    if ((cnt = recv(sock, buff, sizeof(buff), 0)) < 0) {
        return -1;
    }
#else
    if ((cnt = read(sock, buff, sizeof(buff))) < 0) {
        perror("network server");
        close(sock);
        return -1;
    }
#endif

    if (cnt == 0) return 0;

    std::string aa(buff, cnt);
    size_t x1 = aa.find("#$#THS01.00");
    if (x1 == std::string::npos) return -1;
    x1 += strlen("#$#THS01.00");

    size_t x2 = aa.find("#$#THE", x1);
    if (x2 == std::string::npos) return -1;

    int size = std::stoi(aa.substr(x1, x2 - x1));
    data = QString::fromStdString(aa.substr(x2 + strlen("#$#THE")));

    size -= data.size();
    while (size > 0) {
        memset(buff, 0, sizeof(buff));
#ifdef _WIN32
        cnt = recv(sock, buff, sizeof(buff), 0);
#else
        cnt = read(sock, buff, sizeof(buff));
#endif
        if (cnt <= 0) break;

        data += QString::fromUtf8(buff, cnt);
        size -= cnt;
    }

    return 0;
}

//------------------------------------------------------------------------------
// Send with reading response
int TTcpIpClient::send(int sock, const QString &data, QString &reply) {
    if (data.size() > 0) {
        int ret = send(sock, data);
        if (ret == OK) ret = readData(sock, reply);
        return ret;
    }
    return OK;
}
