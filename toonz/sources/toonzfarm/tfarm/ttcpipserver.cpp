

#include "ttcpip.h"
#include "tconvert.h"
#include <csignal> // for sig_atomic_t

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include "tthreadmessage.h"
#include "tthread.h"

#ifndef _WIN32
#define SOCKET_ERROR -1
#endif

#include <string>
#include <iostream>

#define MAXHOSTNAME 1024

int establish(unsigned short portnum, int &sock);
int get_connection(int s);
void fireman(int);
void do_something(int);

// Global shutdown flag set by signal handler or shutdown command
volatile sig_atomic_t shutdownRequested = 0;

//#define TRACE

//---------------------------------------------------------------------

class TTcpIpServerImp {
public:
  TTcpIpServerImp(int port) : m_port(port), m_s(-1), m_server(0) {}

  int readData(int sock, QString &data);
  void onReceive(int sock, const QString &data);

  int m_s;  // socket id
  int m_port;
  TTcpIpServer *m_server;  // back pointer

  TThread::Mutex m_mutex;
};

//---------------------------------------------------------------------

int TTcpIpServerImp::readData(int sock, QString &data) {
  int cnt = 0;
  char buff[1025];
  memset(buff, 0, sizeof(buff));

#ifdef _WIN32
  if ((cnt = recv(sock, buff, sizeof(buff) - 1, 0)) < 0) {
    int err = WSAGetLastError();
    return -1;
  }
#else
  if ((cnt = read(sock, buff, sizeof(buff) - 1)) < 0) {
    printf("socket read failure %d\n", errno);
    perror("network server");
    close(sock);
    return -1;
  }
#endif

  if (cnt == 0) return 0;

#ifdef TRACE
  std::cout << buff << std::endl << std::endl;
#endif

  std::string aa(buff);
  int x1 = aa.find("#$#THS01.00");
  x1 += sizeof("#$#THS01.00") - 1;
  int x2 = aa.find("#$#THE");

  std::string ssize;
  for (int i = x1; i < x2; ++i) ssize.push_back(buff[i]);

  int dataSize = std::stoi(ssize);

  unsigned long size = dataSize;
  data               = QString(buff + x2 + sizeof("#$#THE") - 1);
  size -= data.size();

  while (size > 0) {
    memset(buff, 0, sizeof(buff));

#ifdef _WIN32
    if ((cnt = recv(sock, buff, sizeof(buff) - 1, 0)) < 0) return -1;
#else
    if ((cnt = read(sock, buff, sizeof(buff) - 1)) < 0) {
      printf("socket read failure %d\n", errno);
      perror("network server");
      close(sock);
      return -1;
    }
#endif
    else if (cnt == 0) break;
    else if (cnt < (int)sizeof(buff)) {
      buff[cnt] = '\0';
      data += QString(buff);
    } else {
      data += QString(buff);
    }

    size -= cnt;
  }

  if (data.size() < dataSize) return -1;

  return 0;
}

//---------------------------------------------------------------------

void TTcpIpServerImp::onReceive(int sock, const QString &data) {
  QMutexLocker sl(&m_mutex);
  m_server->onReceive(sock, data);
}

//---------------------------------------------------------------------

TTcpIpServer::TTcpIpServer(int port) : m_imp(new TTcpIpServerImp(port)) {
  m_imp->m_server = this;

#ifdef _WIN32
  WSADATA wsaData;
  WORD wVersionRequested = MAKEWORD(2, 2);
  int irc = WSAStartup(wVersionRequested, &wsaData);
  if (irc != 0) throw("Windows Socket Startup failed");
#endif
}

//---------------------------------------------------------------------

TTcpIpServer::~TTcpIpServer() {
  if (m_imp->m_s != -1)
#ifdef _WIN32
    closesocket(m_imp->m_s);
  WSACleanup();
#else
    close(m_imp->m_s);
#endif
}

//---------------------------------------------------------------------

int TTcpIpServer::getPort() const { return m_imp->m_port; }

//---------------------------------------------------------------------

static void shutdown_cb(int) { shutdownRequested = 1; }

//---------------------------------------------------------------------

class DataReader final : public TThread::Runnable {
public:
  DataReader(int clientSocket, std::shared_ptr<TTcpIpServerImp> serverImp)
      : m_clientSocket(clientSocket), m_serverImp(std::move(serverImp)) {}

  void run() override;

  int m_clientSocket;
  std::shared_ptr<TTcpIpServerImp> m_serverImp;
};

void DataReader::run() {
  QString data;
  int ret = m_serverImp->readData(m_clientSocket, data);
  if (ret != -1) {
    if (data == QString("shutdown"))
      shutdownRequested = 1;
    else
      m_serverImp->onReceive(m_clientSocket, data);
#ifdef _WIN32
    closesocket(m_clientSocket);
#else
    close(m_clientSocket);
#endif
  }
}

//---------------------------------------------------------------------

class DataReceiver final : public TThread::Runnable {
public:
  DataReceiver(int clientSocket, const QString &data,
               std::shared_ptr<TTcpIpServerImp> serverImp)
      : m_clientSocket(clientSocket)
      , m_data(data)
      , m_serverImp(std::move(serverImp)) {}

  void run() override;

  int m_clientSocket;
  QString m_data;
  std::shared_ptr<TTcpIpServerImp> m_serverImp;
};

//---------------------------------------------------------------------

void DataReceiver::run() {
  m_serverImp->onReceive(m_clientSocket, m_data);
#ifdef _WIN32
  closesocket(m_clientSocket);
#else
  close(m_clientSocket);
#endif
}

//---------------------------------------------------------------------

void TTcpIpServer::run() {
  try {
    int err = establish(m_imp->m_port, m_imp->m_s);
    if (!err && m_imp->m_s != -1) {
#ifndef _WIN32
      struct sigaction sact;
      sact.sa_handler = shutdown_cb;
      sigemptyset(&sact.sa_mask);
      sact.sa_flags = 0;
      sigaction(SIGUSR1, &sact, 0);
#endif

      while (!shutdownRequested) {
        int t = get_connection(m_imp->m_s);
        if (t < 0) {
#ifndef _WIN32
          if (errno == EINTR) continue;
          perror("accept");
          m_exitCode = errno;
#else
          m_exitCode = WSAGetLastError();
#endif
          return;
        }

        TThread::Executor executor;
#ifdef _WIN32
        QString data;
        int ret = m_imp->readData(t, data);
        if (ret != -1 && !data.isEmpty()) {
          if (data == QString("shutdown")) {
            shutdownRequested = 1;
          } else {
            executor.addTask(new DataReceiver(t, data, m_imp));
          }
        } else {
          ::shutdown(t, 1);
        }
#else
        executor.addTask(new DataReader(t, m_imp));
#endif
      }
    } else {
      m_exitCode = err;
      return;
    }
  } catch (...) {
    m_exitCode = 2000;
    return;
  }
  m_exitCode = 0;
}

//---------------------------------------------------------------------

int TTcpIpServer::getExitCode() const { return m_exitCode; }

//---------------------------------------------------------------------

void TTcpIpServer::sendReply(int socket, const QString &reply) {
  std::string replyUtf8 = reply.toStdString();
  QString header("#$#THS01.00");
  header += QString::number((int)replyUtf8.size());
  header += QString("#$#THE");
  std::string packet = header.toStdString() + replyUtf8;

  int nLeft = packet.size();
  int idx = 0;
  while (nLeft > 0) {
#ifdef _WIN32
    int ret = send(socket, packet.c_str() + idx, nLeft, 0);
#else
    int ret = write(socket, packet.c_str() + idx, nLeft);
#endif
    if (ret == SOCKET_ERROR) break;
    nLeft -= ret;
    idx += ret;
  }
  ::shutdown(socket, 1);
}

//---------------------------------------------------------------------
// Cross-platform establish() using getaddrinfo()
int establish(unsigned short portnum, int &sock) {
    char myname[MAXHOSTNAME + 1];
    memset(myname, 0, sizeof(myname));

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return -1;
    }
#endif

    if (gethostname(myname, MAXHOSTNAME) != 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        return err;
#else
        perror("gethostname");
        return errno;
#endif
    }

    struct addrinfo hints;
    struct addrinfo *result = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_PASSIVE;      // for bind()

    int res = getaddrinfo(myname, nullptr, &hints, &result);
    if (res != 0) {
#ifdef _WIN32
        WSACleanup();
#else
        std::cerr << "getaddrinfo failed: " << gai_strerror(res) << std::endl;
#endif
        return -1;
    }

    sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        freeaddrinfo(result);
        WSACleanup();
        return err;
#else
        perror("socket");
        freeaddrinfo(result);
        return errno;
#endif
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = ((struct sockaddr_in*)result->ai_addr)->sin_addr;
    addr.sin_port = htons(portnum);

    freeaddrinfo(result);

    if (::bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        closesocket(sock);
        WSACleanup();
        return err;
#else
        perror("bind");
        close(sock);
        return errno;
#endif
    }

    if (listen(sock, 3) < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        closesocket(sock);
        WSACleanup();
        return err;
#else
        perror("listen");
        close(sock);
        return errno;
#endif
    }

    return 0; // success
}

//-----------------------------------------------------------------------
int get_connection(int s) {
  int t = accept(s, nullptr, nullptr);
  return t < 0 ? -1 : t;
}

#ifndef _WIN32
void fireman(int) {
  while (waitpid(-1, nullptr, WNOHANG) > 0) ;
}
#endif
