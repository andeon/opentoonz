// Qt includes
#include <QCoreApplication>
#include <QThread>
#include <QElapsedTimer>
#include <QSharedMemory>
#include <QProcess>
#include <QMutex>
#include <QSemaphore>
#include <QAtomicInt>
#include <QEventLoop>
#include <QTimer>

// System-specific includes
#if defined(_WIN32)
#include <windows.h>
#elif defined(MACOSX)
#include <sys/sysctl.h>
#include <unistd.h>
#elif defined(LINUX) || defined(FREEBSD) || defined(HAIKU)
#include <sys/time.h>
#include <unistd.h>
#endif

#include "tipc.h"

// Debugging macro
#ifdef TIPC_DEBUG
#define tipc_debug(expr) expr
#else
#define tipc_debug(expr)
#endif

namespace {
int shm_max = -1;
int shm_all = -1;
int shm_seg = -1;
int shm_mni = -1;
}  // namespace

//********************************************************
//    Helper Functions
//********************************************************

// Helper function for reading data from a socket
bool tipc::readSocketData(QLocalSocket *socket, char *data, qint64 dataSize, int msecs, QEventLoop *loop, QEventLoop::ProcessEventsFlag flag) {
    qint64 dataRead = 0;
    char *currData = data;

    while (dataRead < dataSize) {
        if (socket->bytesAvailable() == 0) {
            if (loop) {
                loop->exec(flag);
                if (socket->bytesAvailable() == 0) {
                    return false;
                }
            } else if (!socket->waitForReadyRead(msecs)) {
                return false;
            }
        }

        qint64 bytesRead = socket->read(currData, dataSize - dataRead);
        currData += bytesRead;
        dataRead += bytesRead;
    }

    return true;
}

// Helper function for shared memory operations
bool tipc::handleSharedMemory(QSharedMemory &shmem, int size, bool strictSize) {
    static QSemaphore sem(tipc::shm_maxSegmentCount());
    sem.acquire(1);

    if (tipc::create(shmem, size, strictSize) <= 0) {
        sem.release(1);
        return false;
    }

    return true;
}

// Helper function for reading a message from a stream
QString tipc::readMessageHelper(Stream &stream, Message &msg, int msecs, QEventLoop::ProcessEventsFlag flag) {
    msg.clear();
    if (!stream.readMessage(msg, msecs, flag)) return QString();

    QString res;
    msg >> res;
    return res;
}

//********************************************************
//    tipc::Stream Implementation
//********************************************************

int tipc::Stream::readSize() {
    if (m_socket->bytesAvailable() < sizeof(TINT32)) return -1;

    TINT32 msgSize = -1;
    m_socket->peek((char *)&msgSize, sizeof(TINT32));

    return msgSize;
}

bool tipc::Stream::messageReady() {
    TINT32 msgSize = readSize();
    return msgSize >= 0 && m_socket->bytesAvailable() >= msgSize;
}

bool tipc::Stream::readData(char *data, qint64 dataSize, int msecs) {
    tipc_debug(qDebug("tipc::Stream::readData entry"));
    bool result = readSocketData(m_socket, data, dataSize, msecs, nullptr, QEventLoop::AllEvents);
    tipc_debug(qDebug("tipc::Stream::readData exit"));
    return result;
}

bool tipc::Stream::readDataNB(char *data, qint64 dataSize, int msecs, QEventLoop::ProcessEventsFlag flag) {
    tipc_debug(qDebug("tipc::Stream::readDataNB entry"));
    QEventLoop loop;
    bool result = readSocketData(m_socket, data, dataSize, msecs, &loop, flag);
    tipc_debug(qDebug("tipc::Stream::readDataNB exit"));
    return result;
}

bool tipc::Stream::readMessage(Message &msg, int msecs) {
    TINT32 msgSize = 0;
    if (!readData((char *)&msgSize, sizeof(TINT32), msecs)) return false;

    msg.ba().resize(msgSize);
    return readData(msg.ba().data(), msgSize, msecs);
}

bool tipc::Stream::readMessageNB(Message &msg, int msecs, QEventLoop::ProcessEventsFlag flag) {
    TINT32 msgSize = 0;
    if (!readDataNB((char *)&msgSize, sizeof(TINT32), msecs, flag)) return false;

    msg.ba().resize(msgSize);
    return readDataNB(msg.ba().data(), msgSize, msecs, flag);
}

bool tipc::Stream::flush(int msecs) {
    tipc_debug(qDebug("tipc:flush entry"));

    while (m_socket->bytesToWrite() > 0) {
        if (!m_socket->waitForBytesWritten(msecs)) return false;
    }

    tipc_debug(qDebug("tipc:flush exit"));
    return true;
}

//********************************************************
//    tipc Stream Operators
//********************************************************

tipc::Stream &operator>>(tipc::Stream &stream, tipc::Message &msg) {
    QLocalSocket *socket = stream.socket();
    msg.clear();

    TINT32 msgSize;
    socket->read((char *)&msgSize, sizeof(TINT32));
    msg.ba().resize(msgSize);
    socket->read(msg.ba().data(), msgSize);
    return stream;
}

tipc::Stream &operator<<(tipc::Stream &stream, tipc::Message &msg) {
    QLocalSocket *socket = stream.socket();

    TINT32 size = msg.ba().size();
    socket->write((char *)&size, sizeof(TINT32));
    socket->write(msg.ba().data(), size);

    return stream;
}

//********************************************************
//    tipc Utilities
//********************************************************

QString tipc::applicationSpecificServerName(QString srvName) {
    return srvName + QString::number(QCoreApplication::applicationPid());
}

bool tipc::startBackgroundProcess(QString cmdlineProgram, QStringList cmdlineArguments) {
#ifdef _WIN32
    QProcess *proc = new QProcess;
    proc->start(cmdlineProgram, cmdlineArguments);
    if (proc->state() == QProcess::NotRunning) {
        delete proc;
        return false;
    }

    QObject::connect(proc, SIGNAL(finished(int, QProcess::ExitStatus)), proc, SLOT(deleteLater()));
    QObject::connect(proc, SIGNAL(error(QProcess::ProcessError)), proc, SLOT(deleteLater()));
    return true;
#else
    return QProcess::startDetached(cmdlineProgram, cmdlineArguments);
#endif
}

bool tipc::startSlaveServer(QString srvName, QString cmdlineProgram, QStringList cmdlineArguments) {
    if (!tipc::startBackgroundProcess(cmdlineProgram, cmdlineArguments)) return false;

    QString mainSrvName(srvName + "_main");
    QLocalSocket *dummySock = new QLocalSocket;
    dummySock->connectToServer(mainSrvName);

    while (dummySock->state() == QLocalSocket::UnconnectedState) {
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10 << 10);  // 10.24 msecs
#endif
        dummySock->connectToServer(mainSrvName);
    }

    dummySock->waitForConnected(-1);

    tipc::Stream stream(dummySock);
    tipc::Message msg;

    stream << (msg << QString("$quit_on_error"));
    if (readMessageHelper(stream, msg, 3000) == QString()) {
        std::cout << "tipc::startSlaveServer - tipc::readMessage TIMEOUT" << std::endl;
        return false;
    }

    if (QCoreApplication::instance() && QThread::currentThread() != QCoreApplication::instance()->thread()) {
        dummySock->moveToThread(QCoreApplication::instance()->thread());
    }

    QObject::connect(dummySock, SIGNAL(error(QLocalSocket::LocalSocketError)), dummySock, SLOT(deleteLater()));
    return true;
}

bool tipc::startSlaveConnection(QLocalSocket *socket, QString srvName, int msecs, QString cmdlineProgram, QStringList cmdlineArguments, QString threadName) {
    QElapsedTimer time;
    time.start();

    if (msecs == -1) msecs = (std::numeric_limits<int>::max)();

    QString fullSrvName(srvName + threadName);
    socket->connectToServer(fullSrvName);

    if (socket->state() == QLocalSocket::UnconnectedState && !cmdlineProgram.isEmpty()) {
        static QMutex mutex;
        QMutexLocker locker(&mutex);

        socket->connectToServer(fullSrvName);
        if (socket->state() != QLocalSocket::UnconnectedState) goto connecting;

        if (!tipc::startSlaveServer(srvName, cmdlineProgram, cmdlineArguments)) return false;

        socket->connectToServer(fullSrvName);
        if (socket->state() == QLocalSocket::UnconnectedState) return false;
    }

connecting:
    socket->waitForConnected(msecs - time.elapsed());
    return socket->state() == QLocalSocket::ConnectedState;
}

QString tipc::readMessage(Stream &stream, Message &msg, int msecs) {
    return readMessageHelper(stream, msg, msecs);
}

QString tipc::readMessageNB(Stream &stream, Message &msg, int msecs, QEventLoop::ProcessEventsFlag flag) {
    return readMessageHelper(stream, msg, msecs, flag);
}

QString tipc::uniqueId() {
    static QAtomicInt count;
    count.ref();
    return QString::number(QCoreApplication::applicationPid()) + "_" + QString::number((int)count);
}

int tipc::shm_maxSegmentSize() {
    if (shm_max < 0) {
#ifdef MACOSX
        size_t valSize = sizeof(TINT64);
        TINT64 val;
        sysctlbyname("kern.sysv.shmmax", &val, &valSize, NULL, 0);
        shm_max = std::min(val, (TINT64)(std::numeric_limits<int>::max)());
#else
        shm_max = (std::numeric_limits<int>::max)();
#endif
    }
    return shm_max;
}

int tipc::shm_maxSegmentCount() {
    if (shm_seg < 0) {
#ifdef MACOSX
        size_t valSize = sizeof(TINT64);
        TINT64 val;
        sysctlbyname("kern.sysv.shmseg", &val, &valSize, NULL, 0);
        shm_seg = std::min(val, (TINT64)(std::numeric_limits<int>::max)());
#else
        shm_seg = (std::numeric_limits<int>::max)();
#endif
    }
    return shm_seg;
}

int tipc::shm_maxSharedPages() {
    if (shm_all < 0) {
#ifdef MACOSX
        size_t valSize = sizeof(TINT64);
        TINT64 val;
        sysctlbyname("kern.sysv.shmall", &val, &valSize, NULL, 0);
        shm_all = std::min(val, (TINT64)(std::numeric_limits<int>::max)());
#else
        shm_all = (std::numeric_limits<int>::max)();
#endif
    }
    return shm_all;
}

int tipc::shm_maxSharedCount() {
    if (shm_mni < 0) {
#ifdef MACOSX
        size_t valSize = sizeof(TINT64);
        TINT64 val;
        sysctlbyname("kern.sysv.shmmni", &val, &valSize, NULL, 0);
        shm_mni = std::min(val, (TINT64)(std::numeric_limits<int>::max)());
#else
        shm_mni = (std::numeric_limits<int>::max)();
#endif
    }
    return shm_mni;
}

void tipc::shm_set(int shmmax, int shmseg, int shmall, int shmmni) {
    tipc_debug(qDebug("shmmax: %i, shmseg: %i, shmall: %i, shmmni: %i", shmmax, shmseg, shmall, shmmni));
#ifdef MACOSX
    TINT64 val;
    int err;
    if (shmmax > 0) {
        val = shmmax;
        err = sysctlbyname("kern.sysv.shmmax", NULL, NULL, &val, sizeof(TINT64));
        if (!err) shm_max = shmmax;
    }
    if (shmseg > 0) {
        val = shmseg;
        err = sysctlbyname("kern.sysv.shmseg", NULL, NULL, &val, sizeof(TINT64));
        if (!err) shm_seg = shmseg;
    }
    if (shmall > 0) {
        val = shmall;
        err = sysctlbyname("kern.sysv.shmall", NULL, NULL, &val, sizeof(TINT64));
        if (!err) shm_all = shmall;
    }
    if (shmmni > 0) {
        val = shmmni;
        err = sysctlbyname("kern.sysv.shmmni", NULL, NULL, &val, sizeof(TINT64));
        if (!err) shm_mni = shmmni;
    }
#endif
}

int tipc::create(QSharedMemory &shmem, int size, bool strictSize) {
    bool retried = false;

    if (!strictSize) size = std::min(size, shm_maxSegmentSize());

    tipc_debug(qDebug() << "shMem create: size =" << size);

retry:
    if (!shmem.create(size)) {
        tipc_debug(qDebug() << "Error: Shared Segment could not be created: #" << shmem.errorString());

        if (shmem.error() == QSharedMemory::AlreadyExists && !retried) {
            retried = true;
            shmem.attach();
            shmem.detach();
            goto retry;
        }

        return -1;
    }

    return size;
}

bool tipc::writeShMemBuffer(Stream &stream, Message &msg, int bufSize, ShMemWriter *dataWriter) {
    tipc_debug(QElapsedTimer time; time.start());
    tipc_debug(qDebug("tipc::writeShMemBuffer entry"));

    QSharedMemory shmem(tipc::uniqueId());
    if (!handleSharedMemory(shmem, bufSize, false)) goto err;

    msg << QString("shm") << shmem.key() << bufSize;

    int remainingData = bufSize;
    while (remainingData > 0) {
        shmem.lock();
        int chunkData = dataWriter->write((char *)shmem.data(), std::min(shmem.size(), remainingData));
        shmem.unlock();
        remainingData -= chunkData;

        stream << (msg << QString("chk") << chunkData);

        if (readMessageHelper(stream, msg, 3000) != "ok") goto err;

        msg.clear();
    }

    tipc_debug(qDebug("tipc::writeShMemBuffer exit"));
    tipc_debug(qDebug() << "tipc::writeShMemBuffer time:" << time.elapsed());
    return true;

err:
    tipc_debug(qDebug("tipc::writeShMemBuffer exit (error)"));
    msg.clear();
    return false;
}

bool tipc::readShMemBuffer(Stream &stream, Message &msg, ShMemReader *dataReader) {
    tipc_debug(QElapsedTimer time; time.start());
    tipc_debug(qDebug("tipc::readShMemBuffer entry"));

    QString res = readMessageHelper(stream, msg, 3000);
    if (res != "shm") {
        tipc_debug(qDebug("tipc::readShMemBuffer exit (res != \"shm\")"));
        return false;
    }

    QString id, chkStr;
    int bufSize;
    msg >> id >> bufSize >> chkStr;

    QSharedMemory shmem(id);
    if (!shmem.attach()) {
        tipc_debug(qDebug("tipc::readShMemBuffer exit (shmem not attached)"));
        return false;
    }

    int remainingData = bufSize;
    while (true) {
        msg >> chunkData;

        shmem.lock();
        remainingData -= dataReader->read((const char *)shmem.data(), chunkData);
        shmem.unlock();

        stream << (msg << clr << QString("ok"));
        stream.flush();

        if (remainingData <= 0) break;

        if (readMessageHelper(stream, msg, 3000) != "chk") {
            tipc_debug(qDebug("tipc::readShMemBuffer exit (unexpected chunk absence)"));
            return false;
        }
    }

    shmem.detach();
    tipc_debug(qDebug("tipc::readShMemBuffer exit"));
    tipc_debug(qDebug() << "tipc::readShMemBuffer time:" << time.elapsed());
    return true;
}
