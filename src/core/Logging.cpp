#include "core/Logging.h"

#include "core/AppPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>

#include <cstdio>

namespace cutee::logging {
namespace {

constexpr qint64 kMaxFileSize = 5 * 1024 * 1024;
constexpr int kMaxRotatedFiles = 3;

QMutex g_mutex;
QFile *g_file = nullptr;
QString g_path;
Level g_minimum = Level::Info;
QtMessageHandler g_previousHandler = nullptr;

Level levelForMessageType(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return Level::Debug;
    case QtInfoMsg:
        return Level::Info;
    case QtWarningMsg:
        return Level::Warning;
    case QtCriticalMsg:
    case QtFatalMsg:
        return Level::Critical;
    }
    return Level::Info;
}

void rotateIfNeeded()
{
    if (!g_file || !g_file->isOpen() || g_file->size() < kMaxFileSize)
        return;

    g_file->close();
    QFile::remove(QStringLiteral("%1.%2").arg(g_path).arg(kMaxRotatedFiles));
    for (int index = kMaxRotatedFiles - 1; index >= 1; --index) {
        const QString from = QStringLiteral("%1.%2").arg(g_path).arg(index);
        const QString to = QStringLiteral("%1.%2").arg(g_path).arg(index + 1);
        if (QFile::exists(from)) {
            QFile::remove(to);
            QFile::rename(from, to);
        }
    }
    QFile::rename(g_path, g_path + QStringLiteral(".1"));
    g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const Level level = levelForMessageType(type);

    if (level >= g_minimum) {
        QString location;
        if (context.file) {
            location = QStringLiteral(" (%1:%2)").arg(QFileInfo(context.file).fileName()).arg(context.line);
        }
        const QString line = QStringLiteral("%1 [%2] %3%4")
                                 .arg(QDateTime::currentDateTime().toString(
                                          QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                      levelName(level), message, location);

        QMutexLocker locker(&g_mutex);
        if (g_file && g_file->isOpen()) {
            rotateIfNeeded();
            g_file->write(line.toUtf8());
            g_file->write("\n");
            g_file->flush();
        }
    }

    if (g_previousHandler) {
        g_previousHandler(type, context, message);
        return;
    }
    fprintf(stderr, "%s\n", qPrintable(message));
    fflush(stderr);
}

} // namespace

void init()
{
    QMutexLocker locker(&g_mutex);
    if (g_file)
        return;

    const QString directory = AppPaths::logDirectory();
    if (!QDir().mkpath(directory)) {
        fprintf(stderr, "CUTEE: cannot create log directory %s\n", qPrintable(directory));
        return;
    }

    g_path = AppPaths::logFile();
    g_file = new QFile(g_path);
    if (!g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete g_file;
        g_file = nullptr;
        fprintf(stderr, "CUTEE: cannot open log file %s\n", qPrintable(g_path));
        return;
    }

    g_previousHandler = qInstallMessageHandler(messageHandler);

    const QString header = QStringLiteral("===== CUTEE %1 session start =====")
                               .arg(QString::fromLatin1(CUTEE_VERSION));
    g_file->write(header.toUtf8());
    g_file->write("\n");
    g_file->write(QStringLiteral("data dir: %1\n").arg(AppPaths::dataDirectory()).toUtf8());
    g_file->flush();
}

void shutdown()
{
    QMutexLocker locker(&g_mutex);
    if (!g_file)
        return;
    g_previousHandler = qInstallMessageHandler(g_previousHandler);
    g_file->close();
    delete g_file;
    g_file = nullptr;
}

void setMinimumLevel(Level level)
{
    QMutexLocker locker(&g_mutex);
    g_minimum = level;
}

Level minimumLevel()
{
    QMutexLocker locker(&g_mutex);
    return g_minimum;
}

Level levelFromName(const QString &name)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QLatin1String("debug"))
        return Level::Debug;
    if (normalized == QLatin1String("warning"))
        return Level::Warning;
    if (normalized == QLatin1String("critical") || normalized == QLatin1String("error"))
        return Level::Critical;
    return Level::Info;
}

QString levelName(Level level)
{
    switch (level) {
    case Level::Debug:
        return QStringLiteral("DEBUG");
    case Level::Info:
        return QStringLiteral("INFO");
    case Level::Warning:
        return QStringLiteral("WARN");
    case Level::Critical:
        return QStringLiteral("ERROR");
    }
    return QStringLiteral("INFO");
}

QString logFilePath()
{
    QMutexLocker locker(&g_mutex);
    return g_path.isEmpty() ? AppPaths::logFile() : g_path;
}

QStringList readTail(int maxLines)
{
    QFile file(logFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QStringList incoming;
    const qint64 size = file.size();
    const qint64 maxBytes = 2 * 1024 * 1024;
    if (size > maxBytes)
        file.seek(size - maxBytes);

    const QString text = QString::fromUtf8(file.readAll());
    incoming = text.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::SkipEmptyParts);
    while (incoming.size() > maxLines)
        incoming.removeFirst();
    return incoming;
}

} // namespace cutee::logging
