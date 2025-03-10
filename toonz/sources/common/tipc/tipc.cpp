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
#include <QDebug>
#include <QCommandLineParser> // For better command-line argument handling

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

/*
PLATFORM-SPECIFIC REMINDERS:
- Windows: QLocalSocket::waitForBytesWritten requires data to be read on the other end.
- MACOSX: Shared memory settings are restrictive (4 MB max per segment).
*/

//********************************************************
//    Diagnostics Stuff
//********************************************************

// #define TIPC_DEBUG

#ifdef TIPC_DEBUG
#define tipc_debug(expr) expr
#else
#define tipc_debug(expr)
#endif

#ifdef TIPC_DEBUG
#include <QTime>
#endif

//********************************************************
//    Local namespace Stuff
//********************************************************

namespace {
int shm_max = -1;
int shm_all = -1;
int shm_seg = -1;
int shm_mni = -1;
}  // namespace

//********************************************************
//    tipc Stream Implementation
//********************************************************

int tipc::Stream::readSize() {
  if (m_socket->bytesAvailable() < sizeof(TINT32)) return -1;

  TINT32 msgSize = -1;
  m_socket->peek(reinterpret_cast<char*>(&msgSize), sizeof(TINT32));

  return msgSize;
}

//-------------------------------------------------------------

bool tipc::Stream::messageReady() {
  TINT32 msgSize;
  return (msgSize = readSize()) >= 0 && m_socket->bytesAvailable() >= msgSize;
}

//-------------------------------------------------------------

bool tipc::Stream::readData(char* data, qint64 dataSize, int msecs) {
  tipc_debug(qDebug() << "tipc::Stream::readData entry");
  qint64 dataRead = 0;
  char* currData = data;

  while (dataRead < dataSize) {
    if ((m_socket->bytesAvailable() == 0) &&
        !m_socket->waitForReadyRead(msecs)) {
      tipc_debug(qDebug() << "tipc::Stream::readData exit (unexpected loss of data)");
      return false;
    }

    // Read the supplied data
    qint64 r = m_socket->read(currData, dataSize - dataRead);
    currData += r;
    dataRead += r;
  }

  tipc_debug(qDebug() << "tipc::Stream::readData exit");
  return true;
}

//-------------------------------------------------------------

bool tipc::Stream::readDataNB(char* data, qint64 dataSize, int msecs,
                              QEventLoop::ProcessEventsFlag flag) {
  tipc_debug(qDebug() << "tipc::Stream::readDataNB entry");
  qint64 dataRead = 0;
  char* currData = data;

  QEventLoop loop;
  QObject::connect(m_socket, &QLocalSocket::readyRead, &loop, &QEventLoop::quit);
  QObject::connect(m_socket, &QLocalSocket::errorOccurred, &loop, &QEventLoop::quit);

  if (msecs >= 0) QTimer::singleShot(msecs, &loop, &QEventLoop::quit);

  while (dataRead < dataSize) {
    if (m_socket->bytesAvailable() == 0) {
      loop.exec(flag);
      if (m_socket->bytesAvailable() == 0) {
        tipc_debug(qDebug() << "tipc::Stream::readDataNB exit (unexpected loss of data)");
        return false;
      }
    }

    // Read the supplied data
    qint64 r = m_socket->read(currData, dataSize - dataRead);
    currData += r;
    dataRead += r;
  }

  tipc_debug(qDebug() << "tipc::Stream::readDataNB exit");
  return true;
}

//-------------------------------------------------------------

bool tipc::Stream::readMessage(Message& msg, int msecs) {
  TINT32 msgSize = 0;
  if (!readData(reinterpret_cast<char*>(&msgSize), sizeof(TINT32), msecs)) return false;

  msg.ba().resize(msgSize);
  if (!readData(msg.ba().data(), msgSize, msecs)) return false;

  return true;
}

//-------------------------------------------------------------

bool tipc::Stream::readMessageNB(Message& msg, int msecs,
                                 QEventLoop::ProcessEventsFlag flag) {
  TINT32 msgSize = 0;
  if (!readDataNB(reinterpret_cast<char*>(&msgSize), sizeof(TINT32), msecs, flag)) return false;

  msg.ba().resize(msgSize);
  if (!readDataNB(msg.ba().data(), msgSize, msecs, flag)) return false;

  return true;
}

//-------------------------------------------------------------

bool tipc::Stream::flush(int msecs) {
  tipc_debug(qDebug() << "tipc:flush entry");

  while (m_socket->bytesToWrite() > 0) {
    tipc_debug(qDebug() << "bytes to write:" << m_socket->bytesToWrite());
    bool ok = m_socket->flush();
    tipc_debug(qDebug() << "flush success:" << ok << "bytes to write:" << m_socket->bytesToWrite());
    if (m_socket->bytesToWrite() > 0 && !m_socket->waitForBytesWritten(msecs))
      return false;
  }

  tipc_debug(qDebug() << "tipc:flush exit - bytes to write:" << m_socket->bytesToWrite());
  return (m_socket->bytesToWrite() == 0);
}

//********************************************************
//    tipc Stream Operators
//********************************************************

tipc::Stream& operator>>(tipc::Stream& stream, tipc::Message& msg) {
  QLocalSocket* socket = stream.socket();
  msg.clear();

  TINT32 msgSize;
  socket->read(reinterpret_cast<char*>(&msgSize), sizeof(TINT32));
  msg.ba().resize(msgSize);
  socket->read(msg.ba().data(), msgSize);
  return stream;
}

tipc::Stream& operator<<(tipc::Stream& stream, tipc::Message& msg) {
  QLocalSocket* socket = stream.socket();

  TINT32 size = msg.ba().size();
  socket->write(reinterpret_cast<char*>(&size), sizeof(TINT32));
  socket->write(msg.ba().data(), size);

  return stream;
}

//********************************************************
//    tipc Utilities
//********************************************************

QString tipc::applicationSpecificServerName(const QString& srvName) {
  return srvName + QString::number(QCoreApplication::applicationPid());
}

//-------------------------------------------------------------

bool tipc::startBackgroundProcess(const QString& cmdlineProgram,
                                  const QStringList& cmdlineArguments) {
#ifdef _WIN32
  auto proc = std::make_unique<QProcess>();

  proc->start(cmdlineProgram, cmdlineArguments);
  if (proc->state() == QProcess::NotRunning) {
    return false;
  }

  QObject::connect(proc.get(), &QProcess::finished, proc.get(), &QProcess::deleteLater);
  QObject::connect(proc.get(), &QProcess::errorOccurred, proc.get(), &QProcess::deleteLater);
  proc.release(); // Let Qt manage the lifetime
  return true;
#else
  return QProcess::startDetached(cmdlineProgram, cmdlineArguments);
#endif
}

//-------------------------------------------------------------

bool tipc::startSlaveServer(const QString& srvName, const QString& cmdlineProgram,
                            const QStringList& cmdlineArguments) {
  if (!tipc::startBackgroundProcess(cmdlineProgram, cmdlineArguments))
    return false;

  QString mainSrvName(srvName + "_main");

  auto dummySock = std::make_unique<QLocalSocket>();
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

  tipc::Stream stream(dummySock.get());
  tipc::Message msg;

  stream << (msg << QString("$quit_on_error"));
  if (tipc::readMessage(stream, msg, 3000) == QString()) {
    std::cout << "tipc::startSlaveServer - tipc::readMessage TIMEOUT" << std::endl;
    return false;
  }

  if (QCoreApplication::instance() &&
      QThread::currentThread() != QCoreApplication::instance()->thread())
    dummySock->moveToThread(QCoreApplication::instance()->thread());

  QObject::connect(dummySock.get(), &QLocalSocket::errorOccurred,
                   dummySock.get(), &QLocalSocket::deleteLater);

  dummySock.release(); // Let Qt manage the lifetime
  return true;
}

//-------------------------------------------------------------

bool tipc::startSlaveConnection(QLocalSocket* socket, const QString& srvName,
                                int msecs, const QString& cmdlineProgram,
                                const QStringList& cmdlineArguments,
                                const QString& threadName) {
  QElapsedTimer time;
  time.start();

  if (msecs == -1) msecs = std::numeric_limits<int>::max();

  QString fullSrvName(srvName + threadName);
  socket->connectToServer(fullSrvName);

  if (socket->state() == QLocalSocket::UnconnectedState &&
      !cmdlineProgram.isEmpty()) {
    static QMutex mutex;
    QMutexLocker locker(&mutex);

    socket->connectToServer(fullSrvName);
    if (socket->state() != QLocalSocket::UnconnectedState) goto connecting;

    if (!tipc::startSlaveServer(srvName, cmdlineProgram, cmdlineArguments))
      return false;

    socket->connectToServer(fullSrvName);
    if (socket->state() == QLocalSocket::UnconnectedState) return false;
  }

connecting:
  socket->waitForConnected(msecs - time.elapsed());
  return (socket->state() == QLocalSocket::ConnectedState);
}

//-------------------------------------------------------------

QString tipc::readMessage(Stream& stream, Message& msg, int msecs) {
  msg.clear();
  stream.flush();
  if (!stream.readMessage(msg, msecs)) return QString();

  QString res;
  msg >> res;
  return res;
}

//-------------------------------------------------------------

QString tipc::readMessageNB(Stream& stream, Message& msg, int msecs,
                            QEventLoop::ProcessEventsFlag flag) {
  msg.clear();
  if (!stream.readMessageNB(msg, msecs, flag)) return QString();

  QString res;
  msg >> res;
  return res;
}

//-------------------------------------------------------------

QString tipc::uniqueId() {
  static QAtomicInt count;
  count.ref();
  return QString::number(QCoreApplication::applicationPid()) + "_" +
         QString::number(static_cast<int>(count));
}

//-------------------------------------------------------------

int tipc::shm_maxSegmentSize() {
  if (shm_max < 0) {
#ifdef MACOSX
    size_t valSize = sizeof(TINT64);
    TINT64 val;
    sysctlbyname("kern.sysv.shmmax", &val, &valSize, nullptr, 0);
    shm_max = std::min(val, static_cast<TINT64>(std::numeric_limits<int>::max()));
#else
    shm_max = std::numeric_limits<int>::max();
#endif
  }

  return shm_max;
}

//-------------------------------------------------------------

int tipc::shm_maxSegmentCount() {
  if (shm_seg < 0) {
#ifdef MACOSX
    size_t valSize = sizeof(TINT64);
    TINT64 val;
    sysctlbyname("kern.sysv.shmseg", &val, &valSize, nullptr, 0);
    shm_seg = std::min(val, static_cast<TINT64>(std::numeric_limits<int>::max()));
#else
    shm_seg = std::numeric_limits<int>::max();
#endif
  }

  return shm_seg;
}

//-------------------------------------------------------------

int tipc::shm_maxSharedPages() {
  if (shm_all < 0) {
#ifdef MACOSX
    size_t valSize = sizeof(TINT64);
    TINT64 val;
    sysctlbyname("kern.sysv.shmall", &val, &valSize, nullptr, 0);
    shm_all = std::min(val, static_cast<TINT64>(std::numeric_limits<int>::max()));
#else
    shm_all = std::numeric_limits<int>::max();
#endif
  }

  return shm_all;
}

//-------------------------------------------------------------

int tipc::shm_maxSharedCount() {
  if (shm_mni < 0) {
#ifdef MACOSX
    size_t valSize = sizeof(TINT64);
    TINT64 val;
    sysctlbyname("kern.sysv.shmmni", &val, &valSize, nullptr, 0);
    shm_mni = std::min(val, static_cast<TINT64>(std::numeric_limits<int>::max()));
#else
    shm_mni = std::numeric_limits<int>::max();
#endif
  }

  return shm_mni;
}

//-------------------------------------------------------------

void tipc::shm_set(int shmmax, int shmseg, int shmall, int shmmni) {
  tipc_debug(qDebug() << "shmmax:" << shmmax << "shmseg:" << shmseg << "shmall:" << shmall << "shmmni:" << shmmni);
#ifdef MACOSX
  TINT64 val;
  int err;
  if (shmmax > 0) {
    val = shmmax;
    err = sysctlbyname("kern.sysv.shmmax", nullptr, nullptr, &val, sizeof(TINT64));
    if (!err) shm_max = shmmax;
  }
  if (shmseg > 0) {
    val = shmseg;
    err = sysctlbyname("kern.sysv.shmseg", nullptr, nullptr, &val, sizeof(TINT64));
    if (!err) shm_seg = shmseg;
  }
  if (shmall > 0) {
    val = shmall;
    err = sysctlbyname("kern.sysv.shmall", nullptr, nullptr, &val, sizeof(TINT64));
    if (!err) shm_all = shmall;
  }
  if (shmmni > 0) {
    val = shmmni;
    err = sysctlbyname("kern.sysv.shmmni", nullptr, nullptr, &val, sizeof(TINT64));
    if (!err) shm_mni = shmmni;
  }
#endif
}

//-------------------------------------------------------------

int tipc::create(QSharedMemory& shmem, int size, bool strictSize) {
  bool ok, retried = false;

  if (!strictSize) size = std::min(size, shm_maxSegmentSize());

  tipc_debug(qDebug() << "shMem create: size =" << size);

retry:
  ok = shmem.create(size);
  if (!ok) {
    tipc_debug(qDebug() << "Error: Shared Segment could not be created:" << shmem.errorString());

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

//-------------------------------------------------------------

bool tipc::writeShMemBuffer(Stream& stream, Message& msg, int bufSize,
                            ShMemWriter* dataWriter) {
  tipc_debug(QElapsedTimer time; time.start());
  tipc_debug(qDebug() << "tipc::writeShMemBuffer entry");

  static QSemaphore sem(tipc::shm_maxSegmentCount());
  sem.acquire(1);

  {
    QSharedMemory shmem(tipc::uniqueId());
    bool ok = (tipc::create(shmem, bufSize) > 0);
    if (!ok) goto err;

    msg << QString("shm") << shmem.key() << bufSize;

    int remainingData = bufSize;
    while (remainingData > 0) {
      tipc_debug(QElapsedTimer xchTime; xchTime.start());
      shmem.lock();
      int chunkData = dataWriter->write(static_cast<char*>(shmem.data()), std::min(shmem.size(), remainingData));
      shmem.unlock();
      tipc_debug(qDebug() << "exchange time:" << xchTime.elapsed());

      remainingData -= chunkData;
      stream << (msg << QString("chk") << chunkData);

      if (tipc::readMessage(stream, msg) != "ok") goto err;

      msg.clear();
    }
  }

  sem.release(1);
  tipc_debug(qDebug() << "tipc::writeShMemBuffer exit");
  tipc_debug(qDebug() << "tipc::writeShMemBuffer time:" << time.elapsed());
  return true;

err:
  tipc_debug(qDebug() << "tipc::writeShMemBuffer exit (error)");
  msg.clear();
  sem.release(1);
  return false;
}

//-------------------------------------------------------------

bool tipc::readShMemBuffer(Stream& stream, Message& msg,
                           ShMemReader* dataReader) {
  tipc_debug(QElapsedTimer time; time.start());
  tipc_debug(qDebug() << "tipc::readShMemBuffer entry");

  QString res(tipc::readMessage(stream, msg));
  if (res != "shm") {
    tipc_debug(qDebug() << "tipc::readShMemBuffer exit (res != \"shm\")");
    return false;
  }

  QString id, chkStr;
  int bufSize;
  msg >> id >> bufSize >> chkStr;

  QSharedMemory shmem(id);
  shmem.attach();
  if (!shmem.isAttached()) {
    tipc_debug(qDebug() << "tipc::readShMemBuffer exit (shmem not attached)");
    return false;
  }

  int remainingData = bufSize;
  while (true) {
    msg >> chkStr >> remainingData;

    tipc_debug(QElapsedTimer xchTime; xchTime.start());
    shmem.lock();
    remainingData -= dataReader->read(static_cast<const char*>(shmem.data()), remainingData);
    shmem.unlock();
    tipc_debug(qDebug() << "exchange time:" << xchTime.elapsed());

    stream << (msg << QString("ok"));
    stream.flush();

    if (remainingData <= 0) break;

    if (tipc::readMessage(stream, msg) != "chk") {
      tipc_debug(qDebug() << "tipc::readShMemBuffer exit (unexpected chunk absence)");
      return false;
    }
  }

  shmem.detach();
  tipc_debug(qDebug() << "tipc::readShMemBuffer exit");
  tipc_debug(qDebug() << "tipc::readShMemBuffer time:" << time.elapsed());
  return true;
}
