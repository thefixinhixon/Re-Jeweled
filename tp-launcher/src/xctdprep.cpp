// xctdprep.cpp - see xctdprep.h.
#include "xctdprep.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

constexpr quint32 kXctdMagic = 0x0FF512EDu; // big-endian u32 at offset 0

bool isXctd(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = f.read(4);
    if (head.size() != 4)
        return false;
    const auto *b = reinterpret_cast<const uchar *>(head.constData());
    return (quint32(b[0]) << 24 | quint32(b[1]) << 16 | quint32(b[2]) << 8 |
            quint32(b[3])) == kXctdMagic;
}

// The decoder ships next to the launcher (AppImage usr/bin); a system
// install on PATH is the fallback for source builds.
QString findXctd()
{
    const QString beside =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("xctd"));
    if (QFileInfo(beside).isExecutable())
        return beside;
    return QStandardPaths::findExecutable(QStringLiteral("xctd"));
}

} // namespace

bool copyTree(const QString &src, const QString &dst, const PrepStatus &status,
              QString *error)
{
    const QDir srcDir(src);
    if (!QDir().mkpath(dst)) {
        *error = QStringLiteral("Cannot create %1").arg(dst);
        return false;
    }
    QDirIterator it(src, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString from = it.next();
        const QString rel = srcDir.relativeFilePath(from);
        const QString to = QDir(dst).filePath(rel);
        if (it.fileInfo().isDir()) {
            if (!QDir().mkpath(to)) {
                *error = QStringLiteral("Cannot create %1").arg(to);
                return false;
            }
            continue;
        }
        if (status)
            status(QStringLiteral("Copying %1").arg(rel));
        if (!QFile::copy(from, to)) {
            *error = QStringLiteral("Cannot copy %1 to %2").arg(from, to);
            return false;
        }
    }
    return true;
}

QStringList findXctdFiles(const QString &dataDir)
{
    QStringList encoded;
    QDirIterator it(dataDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (isXctd(path))
            encoded << path;
    }
    return encoded;
}

bool prepareXctdData(const QString &dataDir, const PrepStatus &status,
                     QString *error)
{
    const QDir dir(dataDir);

    // Lowercase default.xex: detection and the runtime look for it by that
    // exact name; XBLA packages and copies often have Default.xex.
    const QString lower = dir.filePath(QStringLiteral("default.xex"));
    const QString upper = dir.filePath(QStringLiteral("Default.xex"));
    if (!QFile::exists(lower) && QFile::exists(upper) && !QFile::copy(upper, lower)) {
        *error = QStringLiteral("Cannot create %1").arg(lower);
        return false;
    }

    const QStringList encoded = findXctdFiles(dataDir);
    if (encoded.isEmpty())
        return true;

    const QString xctd = findXctd();
    if (xctd.isEmpty()) {
        *error = QStringLiteral("The xctd decoder was not found next to the "
                                "launcher, so the game's compressed files "
                                "cannot be prepared.");
        return false;
    }

    for (const QString &path : encoded) {
        const QString rel = dir.relativeFilePath(path);
        if (status)
            status(QStringLiteral("Decompressing %1").arg(rel));
        const QString tmp = path + QStringLiteral(".decoding");
        QFile::remove(tmp);
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(xctd, {path, tmp});
        if (!proc.waitForFinished(-1) || proc.exitStatus() != QProcess::NormalExit ||
            proc.exitCode() != 0) {
            const QString out = QString::fromUtf8(proc.readAll()).trimmed();
            QFile::remove(tmp);
            *error = QStringLiteral("Decompressing %1 failed (exit %2):\n%3")
                         .arg(rel)
                         .arg(proc.exitCode())
                         .arg(out.right(600));
            return false;
        }
        // Replace the encoded file only once its plain copy is complete.
        if (!QFile::remove(path) || !QFile::rename(tmp, path)) {
            *error = QStringLiteral("Cannot replace %1 with its decoded copy").arg(rel);
            return false;
        }
    }
    return true;
}
