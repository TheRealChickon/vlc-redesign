// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <cmath>
#include <functional>
#ifdef _WIN32
#include <windows.h>
#endif

class RedesignSeekPreview : public QObject
{
public:
    RedesignSeekPreview(QObject *parent, std::function<void(QUrl, double)> publish)
        : QObject(parent), m_publish(std::move(publish)) {}
    ~RedesignSeekPreview() override { cancel(); }
    void cancel()
    {
        ++m_generation;
        if (m_process) {
            m_process->disconnect(this); m_process->kill(); m_process->waitForFinished(1000);
            delete m_process; m_process = nullptr;
        }
        m_publish({}, 0);
    }
    void request(const QUrl &media, double seconds, const QString &directory)
    {
        const QFileInfo file(media.toLocalFile());
        if (!media.isLocalFile() || !file.isFile() || !std::isfinite(seconds) || seconds < 0) { cancel(); return; }
        const qint64 bucket = qint64(std::floor(seconds * 2));
        const QByteArray source = QByteArray("seek-preview-v2:") + file.canonicalFilePath().toUtf8() + ':' + QByteArray::number(file.size())
            + ':' + QByteArray::number(file.lastModified().toMSecsSinceEpoch());
        const QString key = "preview-" + QString::fromLatin1(QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex())
            + '-' + QString::number(bucket) + ".png";
        if (m_key == key && m_process) return;
        cancel(); m_key = key;
        QDir cache(directory);
        if (!cache.mkpath(".")) return;
        const QString output = cache.filePath(key);
        const double time = bucket / 2.0;
        if (valid(output)) { m_publish(QUrl::fromLocalFile(output), time); return; }
        const auto entries = cache.entryInfoList({"preview-*.png"}, QDir::Files, QDir::Time);
        static const QRegularExpression ownName("^preview-[a-f0-9]{64}-[0-9]+\\.png$");
        for (int index = 239; index < entries.size(); ++index)
            if (ownName.match(entries[index].fileName()).hasMatch()) QFile::remove(entries[index].absoluteFilePath());
        auto *process = new QProcess(this); m_process = process;
        const quint64 generation = m_generation;
        process->setStandardOutputFile(QProcess::nullDevice()); process->setStandardErrorFile(QProcess::nullDevice());
#ifdef _WIN32
        process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) { arguments->flags |= CREATE_NO_WINDOW; });
#endif
        connect(process, &QProcess::finished, this, [this, process, generation, output, time](int code, QProcess::ExitStatus status) {
            if (generation != m_generation) return;
            m_process = nullptr; process->deleteLater();
            if (code == 0 && status == QProcess::NormalExit && valid(output)) m_publish(QUrl::fromLocalFile(output), time);
            else QFile::remove(output);
        });
        connect(process, &QProcess::errorOccurred, this, [this, process, generation](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart && generation == m_generation) { m_process = nullptr; process->deleteLater(); }
        });
        QTimer::singleShot(11000, process, [process] { if (process->state() != QProcess::NotRunning) process->kill(); });
        const QString executable = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef _WIN32
            "vlc-waveform.exe"
#else
            "vlc-waveform"
#endif
        );
        process->start(executable, {media.toString(QUrl::FullyEncoded), QDir::fromNativeSeparators(output), "preview", QString::number(time, 'f', 3)});
    }
private:
    static bool valid(const QString &path)
    {
        const QFileInfo file(path);
        if (!file.isFile() || file.size() <= 0 || file.size() > 2 * 1024 * 1024) return false;
        QImageReader reader(path);
        const QSize size = reader.size();
        return reader.canRead() && size.width() > 0 && size.width() <= 512 && size.height() > 0 && size.height() <= 1024
            && !reader.read().isNull();
    }
    std::function<void(QUrl, double)> m_publish;
    QProcess *m_process = nullptr;
    QString m_key;
    quint64 m_generation = 0;
};
