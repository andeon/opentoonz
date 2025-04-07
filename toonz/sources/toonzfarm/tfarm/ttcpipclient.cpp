#include "ttcpip.h"
#include "tconvert.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h> // Added for getaddrinfo
#else
#include <errno.h> /* obligatory includes */
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#ifndef _WIN32
#define SOCKET_ERROR -1
#endif

//------------------------------------------------------------------------------

TTcpIpClient::TTcpIpClient() {
#ifdef _WIN32
  WSADATA wsaData;
  WORD wVersionRequested = MAKEWORD(1, 1);
  int irc                = WSAStartup(wVersionRequested, &wsaData);
#endif
}

//------------------------------------------------------------------------------

TTcpIpClient::~TTcpIpClient() {
#ifdef _WIN32
  WSACleanup();
#endif
}

//------------------------------------------------------------------------------

int TTcpIpClient::connect(const QString &hostName, const QString &addrStr,
                          int port, int &sock) {
  struct addrinfo hints = {0};
  struct addrinfo *result = nullptr;

  // Set up hints for getaddrinfo
  hints.ai_family = AF_INET;    // IPv4 only for now (matches original behavior)
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;        // Any protocol (TCP in this case)

  // Convert port to string for getaddrinfo
  std::string portStr = std::to_string(port);

  // Resolve hostname using getaddrinfo
  int status = getaddrinfo(hostName.toUtf8().constData(), portStr.c_str(), &hints, &result);
  if (status != 0) {
#ifdef _WIN32
    int err = WSAGetLastError();
#else
    // For non-Windows, could use gai_strerror(status) for error details
#endif
    return HOST_UNKNOWN;
  }

  // Create socket
  int socket_id = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_id == SOCKET_ERROR) {
    freeaddrinfo(result);
#ifdef _WIN32
    int err = WSAGetLastError();
#endif
    return CONNECTION_FAILED;
  }

  // Use the first valid address from getaddrinfo
  struct sockaddr_in addr;
  memset((char *)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  struct sockaddr_in *resolvedAddr = (struct sockaddr_in *)result->ai_addr;
  addr.sin_addr = resolvedAddr->sin_addr;

  // Connect to the server
  int rcConnect = ::connect(socket_id, (struct sockaddr *)&addr, sizeof(addr));
  if (rcConnect == SOCKET_ERROR) {
    sock = -1;
#ifdef _WIN32
    int err = WSAGetLastError();
    switch (err) {
    case WSAECONNREFUSED:
      err = CONNECTION_REFUSED;
      break;
    case WSAETIMEDOUT:
      err = CONNECTION_TIMEDOUT;
      break;
    default:
      err = CONNECTION_FAILED;
      break;
    }
    closesocket(socket_id);
#else
    close(socket_id);
    err = CONNECTION_FAILED;
#endif
    freeaddrinfo(result);
    return err;
  }

  sock = socket_id;
  freeaddrinfo(result); // Free the addrinfo structure
  return OK;
}

//------------------------------------------------------------------------------

int TTcpIpClient::disconnect(int sock) {
#ifdef _WIN32
  closesocket(sock);
#else
  close(sock);
#endif
  return OK;
}

//------------------------------------------------------------------------------

int TTcpIpClient::send(int sock, const QString &data) {
  std::string dataUtf8 = data.toStdString();

  QString header("#$#THS01.00");
  header += QString::number((int)dataUtf8.size());
  header += "#$#THE";

  std::string packet = header.toStdString() + dataUtf8;

  int nLeft = packet.size();
  int idx   = 0;
  while (nLeft > 0) {
#ifdef _WIN32
    int ret = ::send(sock, packet.c_str() + idx, nLeft, 0);
#else
    int ret = write(sock, packet.c_str() + idx, nLeft);
#endif

    if (ret == SOCKET_ERROR) {
#ifdef _WIN32
      int err = WSAGetLastError();
#else
#endif
      return SEND_FAILED;
    }
    nLeft -= ret;
    idx += ret;
  }

  shutdown(sock, 1);
  return OK;
}

//------------------------------------------------------------------------------

static int readData(int sock, QString &data) {
  int cnt = 0;
  char buff[1024];
  memset(buff, 0, sizeof(buff));

#ifdef _WIN32
  if ((cnt = recv(sock, buff, sizeof(buff), 0)) < 0) {
    int err = WSAGetLastError();
    return -1;
  }
#else
  if ((cnt = read(sock, buff, sizeof(buff))) < 0) {
    printf("socket read failure %d\n", errno);
    perror("network server");
    close(sock);
    return -1;
  }
#endif

  if (cnt == 0) return 0;

  std::string aa(buff);
  int x1 = aa.find("#$#THS01.00");
  x1 += sizeof("#$#THS01.00") - 1;
  int x2 = aa.find("#$#THE");

  std::string ssize;
  for (int i = x1; i < x2; ++i) ssize.push_back(buff[i]);

  int size = std::stoi(ssize);

  data = QString(buff + x2 + sizeof("#$#THE") - 1);
  size -= data.size();

  while (size > 0) {
    memset(buff, 0, sizeof(buff));

#ifdef _WIN32
    if ((cnt = recv(sock, buff, sizeof(buff), 0)) < 0) {
      int err = WSAGetLastError();
      return -1;
    }
#else
    if ((cnt = read(sock, buff, sizeof(buff))) < 0) {
      printf("socket read failure %d\n", errno);
      perror("network server");
      close(sock);
      return -1;
    }
#endif
    else if (cnt == 0) {
      break;
    } else if (cnt < (int)sizeof(buff)) {
      data += QString(buff);
    } else {
      data += QString(buff);
    }

    size -= cnt;
  }

  if (data.size() < size) return -1;

  return 0;
}

//------------------------------------------------------------------------------

int TTcpIpClient::send(int sock, const QString &data, QString &reply) {
  if (data.size() > 0) {
    int ret = send(sock, data);
    if (ret == 0) ret = readData(sock, reply);
    return ret;
  }

  return OK;
}
