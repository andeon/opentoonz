

#include "ttcpip.h"
#include "tconvert.h"
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
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

#include <signal.h>
#include "tthreadmessage.h"
#include "tthread.h"
#include <atomic>     // For thread-safe atomic operations
#include <memory>     // For smart pointers
#include <stdexcept>  // For exception handling

#ifndef _WIN32
#define SOCKET_ERROR -1
#endif

#include <string>
using namespace std;

#define MAXHOSTNAME 1024
#define MAX_ALLOWED_SIZE (10 * 1024 * 1024) // 10MB maximum data size

// Protocol constants - properly defined
constexpr const char* PROTOCOL_HEADER_START = "#$#THS01.00";
constexpr const char* PROTOCOL_HEADER_END = "#$#THE";
constexpr size_t HEADER_START_LEN = 11; // strlen("#$#THS01.00")
constexpr size_t HEADER_END_LEN = 5;    // strlen("#$#THE")

int establish(unsigned short portnum, int &sock);
int get_connection(int s);
void fireman(int);

//------------------ Platform-Independent Shutdown Flag ------------------

// Use atomic for thread-safe operations across all platforms
std::atomic<bool> Sthutdown{false};
#define SET_SHUTDOWN() (Sthutdown.store(true))
#define IS_SHUTDOWN() (Sthutdown.load())

//---------------------------------------------------------------------

// RAII wrapper for socket cleanup
class SocketGuard {
private:
    int m_socket;
public:
    explicit SocketGuard(int socket) : m_socket(socket) {}
    ~SocketGuard() {
        if (m_socket != -1) {
#ifdef _WIN32
            closesocket(m_socket);
#else
            close(m_socket);
#endif
        }
    }
    
    // Prevent copying
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;
    
    int get() const { return m_socket; }
    void release() { m_socket = -1; }
};

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

    // Initial data read
#ifdef _WIN32
    if ((cnt = recv(sock, buff, sizeof(buff) - 1, 0)) < 0)
        return -1;
#else
    if ((cnt = read(sock, buff, sizeof(buff) - 1)) < 0) {
        perror("read");
        close(sock);
        return -1;
    }
#endif

    if (cnt == 0) return 0; // Connection closed

    string header(buff, cnt); // Use actual bytes read
    
    // Find protocol markers with proper error checking
    size_t x1 = header.find(PROTOCOL_HEADER_START);
    if (x1 == string::npos) {
        // Protocol error: start marker not found
        return -1;
    }
    
    x1 += HEADER_START_LEN;
    size_t x2 = header.find(PROTOCOL_HEADER_END, x1); // Search from x1 position
    if (x2 == string::npos) {
        // Protocol error: end marker not found
        return -1;
    }

    // Extract and validate size
    if (x2 <= x1) {
        // Invalid size section
        return -1;
    }
    
    string ssize = header.substr(x1, x2 - x1);
    int dataSize;
    try {
        dataSize = std::stoi(ssize);
    } catch (const std::exception& e) {
        // Invalid size format
        return -1;
    }

    // Validate data size to prevent excessive memory allocation
    if (dataSize < 0 || dataSize > MAX_ALLOWED_SIZE) {
        return -1;
    }

    // Calculate data position safely
    size_t dataStartPos = x2 + HEADER_END_LEN;
    if (dataStartPos > header.size()) {
        dataStartPos = header.size();
    }
    
    data = QString::fromStdString(header.substr(dataStartPos));
    size_t remainingSize = dataSize - data.size();

    // Read remaining data with proper bounds checking
    while (remainingSize > 0) {
        memset(buff, 0, sizeof(buff));
        size_t toRead = (remainingSize < sizeof(buff) - 1) ? remainingSize : sizeof(buff) - 1;
        
#ifdef _WIN32
        if ((cnt = recv(sock, buff, static_cast<int>(toRead), 0)) < 0)
            return -1;
#else
        if ((cnt = read(sock, buff, toRead)) < 0) {
            perror("read");
            close(sock);
            return -1;
        }
#endif
        
        if (cnt == 0) break; // Connection closed
        
        // Use explicit size to handle binary data properly
        data += QString::fromUtf8(buff, cnt);
        if (remainingSize >= static_cast<size_t>(cnt)) {
            remainingSize -= cnt;
        } else {
            break; // Should not happen, but safety first
        }
    }

    return 0;
}

//---------------------------------------------------------------------

void TTcpIpServerImp::onReceive(int sock, const QString &data) {
    QMutexLocker sl(&m_mutex);
    m_server->onReceive(sock, data);
}

//---------------------------------------------------------------------

TTcpIpServer::TTcpIpServer(int port)
    : m_imp(new TTcpIpServerImp(port)) {
    m_imp->m_server = this;

#ifdef _WIN32
    WSADATA wsaData;
    int irc = WSAStartup(MAKEWORD(2, 2), &wsaData); // Use version 2.2
    if (irc != 0) {
        throw std::runtime_error("Windows Socket Startup failed");
    }
#endif
}

//---------------------------------------------------------------------

TTcpIpServer::~TTcpIpServer() {
    if (m_imp->m_s != -1) {
#ifdef _WIN32
        closesocket(m_imp->m_s);
        WSACleanup();
#else
        close(m_imp->m_s);
#endif
    }
    delete m_imp;
}

//---------------------------------------------------------------------

int TTcpIpServer::getPort() const { return m_imp->m_port; }

//---------------------------------------------------------------------

static void shutdown_cb(int) { SET_SHUTDOWN(); }

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
    // Use RAII for automatic socket cleanup
    SocketGuard socketGuard(m_clientSocket);
    
    QString data;
    bool success = (m_serverImp->readData(socketGuard.get(), data) != -1);
    if (success) {
        if (data == "shutdown") {
            SET_SHUTDOWN();
        } else {
            m_serverImp->onReceive(socketGuard.get(), data);
        }
    }
    // Socket automatically closed by SocketGuard destructor
}

//---------------------------------------------------------------------

class DataReceiver final : public TThread::Runnable {
public:
    DataReceiver(int clientSocket, const QString &data, std::shared_ptr<TTcpIpServerImp> serverImp)
        : m_clientSocket(clientSocket), m_data(data), m_serverImp(std::move(serverImp)) {}

    void run() override;

    int m_clientSocket;
    QString m_data;
    std::shared_ptr<TTcpIpServerImp> m_serverImp;
};

void DataReceiver::run() {
    SocketGuard socketGuard(m_clientSocket);
    m_serverImp->onReceive(socketGuard.get(), m_data);
    // Socket automatically closed by SocketGuard destructor
}

//---------------------------------------------------------------------

void TTcpIpServer::run() {
    try {
        int socket = -1;
        int err = establish(m_imp->m_port, socket);
        if (err != 0 || socket == -1) { 
            m_exitCode = err;
            return;
        }

        // Use RAII for main server socket
        SocketGuard serverSocketGuard(socket);
        m_imp->m_s = socket; // Track the socket

#ifndef _WIN32
#ifdef MACOSX
        struct sigaction sact;
        sact.sa_handler = shutdown_cb;
        sigaction(SIGUSR1, &sact, 0);
#else
        signal(SIGUSR1, shutdown_cb); // Fixed: use signal() instead of deprecated sigset()
#endif
#endif

        int clientSocket;
        while (!IS_SHUTDOWN()) {
            if ((clientSocket = get_connection(serverSocketGuard.get())) < 0) {
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
            // Use smart pointer to prevent memory leaks
            auto task = std::make_unique<DataReader>(clientSocket, m_imp);
            executor.addTask(task.release()); // Transfer ownership to executor
        }

        // Releases the guard after the loop to avoid double-close in the destructor
        serverSocketGuard.release();

    } catch (const std::exception& e) {
        std::cerr << "Exception in TTcpIpServer::run(): " << e.what() << std::endl;
        m_exitCode = 2000;
        return;
    } catch (...) {
        std::cerr << "Unknown exception in TTcpIpServer::run()" << std::endl;
        m_exitCode = 2001;
        return;
    }

    m_exitCode = 0;
}

//---------------------------------------------------------------------

int TTcpIpServer::getExitCode() const { return m_exitCode; }

//---------------------------------------------------------------------

void TTcpIpServer::sendReply(int socket, const QString &reply) {
    SocketGuard socketGuard(socket); // Ensure socket cleanup on exception
    
    string replyUtf8 = reply.toStdString();

    // Validate data size before sending
    if (replyUtf8.size() > MAX_ALLOWED_SIZE) {
        throw std::runtime_error("Reply data too large");
    }

    QString header = QString(PROTOCOL_HEADER_START) + 
                     QString::number(static_cast<int>(replyUtf8.size())) + 
                     QString(PROTOCOL_HEADER_END);
    
    string packet = header.toStdString() + replyUtf8;

    int nLeft = static_cast<int>(packet.size());
    int idx = 0;
    while (nLeft > 0) {
#ifdef _WIN32
        int ret = send(socketGuard.get(), packet.c_str() + idx, nLeft, 0);
        if (ret == SOCKET_ERROR) break;
#else
        int ret = write(socketGuard.get(), packet.c_str() + idx, nLeft);
        if (ret < 0) break;
#endif
        nLeft -= ret;
        idx += ret;
    }

    // Shutdown is still called but socket cleanup is handled by SocketGuard
    ::shutdown(socketGuard.get(), 1);
}

//---------------------------------------------------------------------

int establish(unsigned short portnum, int &sock) {
    char myname[MAXHOSTNAME + 1];
    struct sockaddr_in sa;
    struct hostent *hp;

    memset(&sa, 0, sizeof(sa));                        /* Clear the 'sa' structure */
    if (gethostname(myname, MAXHOSTNAME) != 0) {
        return 1;                                      /* Failed to retrieve the host name */
    }
    hp = gethostbyname(myname);                        /* Get address information for this host*/
    if (!hp) return 2;                                 /* Host does not exist! */

    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons(portnum);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
    }

    // Set SO_REUSEADDR to avoid "address already in use" errors
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
#ifdef _WIN32
                   (const char*)&reuse, sizeof(reuse)
#else
                   &reuse, sizeof(reuse)
#endif
                   ) < 0) {
        // Continue anyway - this is not critical
    }

    if (::bind(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        closesocket(sock);
        return err;
#else
        int err = errno;
        close(sock);
        return err;
#endif
    }

    int listen_ret = listen(sock, 10); // Increased backlog for better performance
    if (listen_ret < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        closesocket(sock);
        return err;
#else
        int err = errno;
        close(sock);
        return err;
#endif
    }

    return 0;  // Success
}

//---------------------------------------------------------------------

int get_connection(int s) {
    int t = accept(s, NULL, NULL);
    if (t < 0) return -1;
    return t;
}

#ifndef _WIN32
void fireman(int) {
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}
#endif
