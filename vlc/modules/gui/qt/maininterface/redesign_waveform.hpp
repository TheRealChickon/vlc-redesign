// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QtEndian>
#include <functional>
#include <memory>
#include <atomic>
#include "medialibrary/mlthreadpool.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

// Decode local audio with this build's own codecs. Never substitute invented
// waveform samples, and never download a network source for analysis.
class RedesignWaveform : public QObject
{
public:
    explicit RedesignWaveform(std::function<void(QVariantList)> publish, QObject *parent, ThreadRunner *runner)
        : QObject(parent), m_publish(std::move(publish)), m_runner(runner) {}
    ~RedesignWaveform() override { cancel(); }

    void cancel()
    {
        if (m_readCancellation) m_readCancellation->store(true);
        if (m_readJob) { m_runner->cancelTask(this, m_readJob); m_readJob = 0; }
        if (m_process) {
            m_process->disconnect(this);
            m_process->kill();
            m_process->waitForFinished(1000);
            delete m_process;
            m_process = nullptr;
        }
        m_temporary.reset();
    }

    void request(const QUrl &url, const QString &cacheDirectory)
    {
        const QString qa = qEnvironmentVariable("VLC_REDESIGN_QA_DIR");
        if (!qa.isEmpty()) qWarning("Redesign waveform request: local=%d, file=%d", url.isLocalFile(), QFileInfo(url.toLocalFile()).isFile());
        if (!url.isLocalFile()) { cancel(); m_publish({}); return; }
        const QFileInfo source(url.toLocalFile());
        if (!source.isFile() || source.size() > 2LL * 1024 * 1024 * 1024) { cancel(); m_publish({}); return; }
        QDir cache(cacheDirectory);
        if (!cache.mkpath(QStringLiteral("."))) return;
        const QByteArray identity = source.canonicalFilePath().toUtf8() + ':'
            + QByteArray::number(source.size()) + ':'
            + QByteArray::number(source.lastModified().toMSecsSinceEpoch());
        const auto hash = QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex();
        if (m_identity == hash && ((m_process && m_process->state() != QProcess::NotRunning) || m_readJob)) return;
        cancel();
        m_identity = hash;
        m_publish({});
        m_cachePath = cache.filePath(QString::fromLatin1(hash) + QStringLiteral(".json"));
        QFile stored(m_cachePath);
        if (stored.open(QFile::ReadOnly) && stored.size() <= 65536) {
            const auto array = QJsonDocument::fromJson(stored.readAll()).array();
            if (array.size() == 1024) {
                m_publish(array.toVariantList());
                return;
            }
        }
        m_temporary = std::make_shared<QTemporaryDir>(cache.filePath(QStringLiteral("decode-XXXXXX")));
        if (!m_temporary->isValid()) return;
        m_pcmPath = QDir(m_temporary->path()).filePath(QStringLiteral("audio.pcm"));
        m_process = new QProcess(this);
        if (!qa.isEmpty()) qWarning("Redesign waveform decoder executable: %s", qPrintable(QCoreApplication::applicationFilePath()));
        connect(m_process, &QProcess::started, this, [this] {
            if (!qEnvironmentVariableIsEmpty("VLC_REDESIGN_QA_DIR")) qWarning("Redesign waveform process started: %lld", m_process->processId());
        });
        m_process->setStandardOutputFile(QProcess::nullDevice());
        m_process->setStandardErrorFile(qa.isEmpty() ? QProcess::nullDevice() : QDir(qa).filePath(QStringLiteral("waveform-decode.log")));
#ifdef _WIN32
        m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
            if (!qEnvironmentVariableIsEmpty("VLC_REDESIGN_QA_DIR")) qWarning("Redesign waveform command: %s", qPrintable(QString::fromWCharArray(args->arguments)));
        });
#endif
        connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
            if (!qEnvironmentVariableIsEmpty("VLC_REDESIGN_QA_DIR")) qWarning("Redesign waveform complete: code=%d, status=%d, bytes=%lld", code, int(status), QFileInfo(m_pcmPath).size());
            if (status == QProcess::NormalExit && code == 0) readPeaks();
            m_temporary.reset();
        });
        connect(m_process, &QProcess::errorOccurred, this, [](QProcess::ProcessError error) {
            if (!qEnvironmentVariableIsEmpty("VLC_REDESIGN_QA_DIR")) qWarning("Redesign waveform process error: %d", int(error));
        });
        const QString program = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef _WIN32
            QStringLiteral("vlc-waveform.exe")
#else
            QStringLiteral("vlc-waveform")
#endif
        );
        m_process->start(program, {url.toString(QUrl::FullyEncoded), QDir::fromNativeSeparators(m_pcmPath)});
        // Bound CPU time and temporary disk use; leave the normal seek bar on
        // failure, unsupported files, or a cancelled analysis.
        QTimer::singleShot(45000, m_process, [process = m_process] {
            if (process->state() != QProcess::NotRunning) process->kill();
        });
    }

private:
    void readPeaks()
    {
        struct Result { QVariantList peaks; };
        m_readCancellation = std::make_shared<std::atomic_bool>(false);
        const auto cancellation = m_readCancellation;
        const auto temporary = m_temporary;
        const QString path = m_pcmPath, cache = m_cachePath;
        const QByteArray identity = m_identity;
        m_readJob = m_runner->runOnThread<Result>(this,
            [path, cache, cancellation, temporary](Result &result) {
                QFile pcm(path);
                if (cancellation->load() || !pcm.open(QFile::ReadOnly) || pcm.size() < 2 || pcm.size() > 120LL * 1024 * 1024) return;
                const qint64 samples = pcm.size() / 2;
                QVector<double> peaks(1024, 0.0);
                qint64 offset = 0;
                while (!pcm.atEnd() && !cancellation->load()) {
                    const auto chunk = pcm.read(32768);
                    for (qsizetype i = 0; i + 1 < chunk.size(); i += 2, ++offset) {
                        const auto value = qFromLittleEndian<qint16>(chunk.constData() + i);
                        const int bin = std::min<qint64>(1023, offset * 1024 / samples);
                        peaks[bin] = std::max(peaks[bin], std::abs(double(value)) / 32768.0);
                    }
                }
                if (cancellation->load()) return;
                const double reference = std::max(0.0001, *std::max_element(peaks.cbegin(), peaks.cend()));
                for (double peak : peaks) result.peaks.append(peak / reference);
                QFile stored(cache);
                if (stored.open(QFile::WriteOnly)) stored.write(QJsonDocument(QJsonArray::fromVariantList(result.peaks)).toJson(QJsonDocument::Compact));
            },
            [this, identity, cancellation](quint64 id, Result &result) {
                if (id != m_readJob || identity != m_identity || cancellation->load()) return;
                m_readJob = 0;
                if (!result.peaks.isEmpty()) m_publish(result.peaks);
            }, "redesign-waveform");
    }
    std::function<void(QVariantList)> m_publish;
    QProcess *m_process = nullptr;
    std::shared_ptr<QTemporaryDir> m_temporary;
    std::shared_ptr<std::atomic_bool> m_readCancellation;
    ThreadRunner *m_runner;
    quint64 m_readJob = 0;
    QString m_pcmPath, m_cachePath;
    QByteArray m_identity;
};
