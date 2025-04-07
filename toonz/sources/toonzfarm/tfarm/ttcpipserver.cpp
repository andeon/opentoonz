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

#include "tthreadmessage.h"
#include "tthread.h"
#ifndef _WIN32
#define SOCKET_ERROR -1
#endif

#include <string>
using namespace std;

#define MAXHOSTNAME 1024

int establish(unsigned short portnum, int &sock);
int get_connection(int s);
void fireman(int);
void do_something(int);

bool Sthutdown = false;

//#define TRACE

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

int TTcpIpServerImp::readData(int sock, QString &data) {
  int cnt = 0;
  char buff[1025];
  memset(buff, 0, sizeof(buff));

#ifdef _WIN32
  if ((cnt = recv(sock, buff, sizeof(buff) - 1, 0)) < 0) {
    int err = WSAGetLastError();
    // MANAGE THE SPECIFIC ERROR
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
  cout << buff << endl << endl;
#endif

  string aa(buff);
  int x1 = aa.find("#$#THS01.00");
  x1 += sizeof("#$#THS01.00") - 1;
  int x2 = aa.find("#$#THE");

  string ssize;
  for (int i = x1; i < x2; ++i) ssize.push_back(buff[i]);

  int dataSize = std::stoi(ssize);

  unsigned long size = dataSize;
  data               = QString(buff + x2 + sizeof("#$#THE") - 1);
  size -= data.size();

  while (size > 0) {
    memset(buff, 0, sizeof(buff));

#ifdef _WIN32
    if ((cnt = recv(sock, buff, sizeof(buff) - 1, 0)) < 0) {
      int err = WSAGetLastError();
      // MANAGE THE SPECIFIC ERROR
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
    else if (cnt == 0) {
      break;
    } else if (cnt < (int)sizeof(buff)) {
      buff[cnt] = '\0';
      data += QString(buff);
    } else {
      data += QString(buff);
    }

#ifdef TRACE
    cout << buff << endl << endl;
#endif

    size -= cnt;
  }

#ifdef TRACE
  cout << "read " << toString((int)data.length()) << " on " << dataSize << endl
       << endl;
#endif

  if (data.size() < dataSize) return -1;

#ifdef TRACE
  cout << data.toStdString() << endl;
#endif

  return 0;
}

void TTcpIpServerImp::onReceive(int sock, const QString &data) {
  QMutexLocker sl(&m_mutex);
  m_server->onReceive(sock, data);
}

TTcpIpServer::TTcpIpServer(int port) : m_imp(new TTcpIpServerImp(port)) {
  m_imp->m_server = this;

#ifdef _WIN32
  // Windows Socket startup
  WSADATA wsaData;
  WORD wVersionRequested = MAKEWORD(1, 1);
  int irc                = WSAStartup(wVersionRequested, &wsaData);
  if (irc != 0) throw("Windows Socket Startup failed");
#endif
}

TTcpIpServer::~TTcpIpServer() {
  if (m_imp->m_s != -1)
#ifdef _WIN32
    closesocket(m_imp->m_s);
  WSACleanup();
#else
    std::cout << "closing socket" << std::endl;
  close(m_imp->m_s);
#endif
}

int TTcpIpServer::getPort() const { return m_imp->m_port; }

static void shutdown_cb(int) { Sthutdown = true; }

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
      Sthutdown = true;
    else
      m_serverImp->onReceive(m_clientSocket, data);
#ifdef _WIN32
    closesocket(m_clientSocket);
#else
    close(m_clientSocket);
#endif
  }
}

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

void DataReceiver::run() {
  m_serverImp->onReceive(m_clientSocket, m_data);
#ifdef _WIN32
  closesocket(m_clientSocket);
#else
  close(m_clientSocket);
#endif
}

void TTcpIpServer::run() {
  try {
#ifdef _WIN32
    int err = establish(m_imp->m_port, m_imp->m_s);
    if (!err && m_imp->m_s != -1) {
      int t;
      while (!Sthutdown) {
        if ((t = get_connection(m_imp->m_s)) < 0) {
          m_exitCode = WSAGetLastError();
          // MANAGE THE ERROR CONDITION
          return;
        }

        QString data;
        int ret = m_imp->readData(t, data);
        if (ret != -1 && data != "") {
          if (data == QString("shutdown")) {
            Sthutdown = true;
          } else {
            // create a new thread to handle the received data
            TThread::Executor executor;
            executor.addTask(new DataReceiver(t, data, m_imp));
          }
        } else {
          ::shutdown(t, 1);
        }
      }
    } else {
      m_exitCode = err;
      return;
    }
#else
    int err = establish(m_imp->m_port, m_imp->m_s);
    if (!err && m_imp->m_s != -1) {
#ifdef MACOSX
      struct sigaction sact;
      sact.sa_handler = shutdown_cb;
      sigaction(SIGUSR1, &sact, 0);
#else
      sigset(SIGUSR1, shutdown_cb);
#endif
      int t;
      while (!Sthutdown) {
        if ((t = get_connection(m_imp->m_s)) < 0) {
          if (errno == EINTR)
            continue;
          perror("accept");
          m_exitCode = errno;
          return;
        }
        TThread::Executor executor;
        executor.addTask(new DataReader(t, m_imp));
      }
    } else {
      m_exitCode = err;
      return;
    }
#endif
  } catch (...) {
    m_exitCode = 2000;
    return;
  }
  m_exitCode = 0;
}

int TTcpIpServer::getExitCode() const { return m_exitCode; }

void TTcpIpServer::sendReply(int socket, const QString &reply) {
  string replyUtf8 = reply.toStdString();
  QString header("#$#THS01.00");
  header += QString::number((int)replyUtf8.size());
  header += QString("#$#THE");
  string packet = header.toStdString() + replyUtf8;

  int nLeft = packet.size();
  int idx   = 0;
  while (nLeft > 0) {
#ifdef _WIN32
    int ret = send(socket, packet.c_str() + idx, nLeft, 0);
#else
    int ret = write(socket, packet.c_str() + idx, nLeft);
#endif
    if (ret == SOCKET_ERROR) {
      // Error
    }
    nLeft -= ret;
    idx += ret;
  }
  ::shutdown(socket, 1);
}

int establish(unsigned short portnum, int &sock) {
  char myname[MAXHOSTNAME + 1];
  struct sockaddr_in sa;
  struct addrinfo hints = {0};
  struct addrinfo *result = nullptr;

  memset(&sa, 0, sizeof(struct sockaddr_in));
  gethostname(myname, MAXHOSTNAME);

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;

  std::string portStr = std::to_string(portnum);
  int status = getaddrinfo(myname, portStr.c_str(), &hints, &result);
  if (status != 0)
    return (-1);

  sa.sin_family = AF_INET;
  sa.sin_port = htons(portnum);
  struct sockaddr_in *resolvedAddr = (struct sockaddr_in *)result->ai_addr;
  sa.sin_addr = resolvedAddr->sin_addr;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
    int err = WSAGetLastError();
    freeaddrinfo(result);
    return err;
#else
    freeaddrinfo(result);
    return errno;
#endif
  }

  if (::bind(sock, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0) {
#ifdef _WIN32
    int err = WSAGetLastError();
    closesocket(sock);
    freeaddrinfo(result);
    return err;
#else
    freeaddrinfo(result);
    return errno;
    close(sock);
#endif
  }

  freeaddrinfo(result);
  return listen(sock, 3);
}

int get_connection(int s) {
  int t;
  if ((t = accept(s, NULL, NULL)) < 0)
    return (-1);
  return (t);
}

#ifndef _WIN32
void fireman(int) {
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}
#endif
