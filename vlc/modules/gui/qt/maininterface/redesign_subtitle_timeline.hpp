// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <functional>
#include <memory>
#include <algorithm>
#include <cmath>
#include <QVariant>
#include <QUrl>
#ifdef _WIN32
#include <windows.h>
#endif

inline QVariantList readRedesignSubtitleCues(const QString &path, const QString &encoding = {})
{
    QFile file(path);
    if (file.size() > 8 * 1024 * 1024 || !file.open(QFile::ReadOnly)) return {};
    const QByteArray bytes = file.readAll();
    QStringDecoder decoder(bytes.startsWith("\xff\xfe") || bytes.startsWith("\xfe\xff") ? QStringConverter::Utf16 : QStringConverter::Utf8);
    QString content = decoder(bytes);
    if (decoder.hasError()) {
        QStringDecoder fallback(encoding.toUtf8().constData());
        content = fallback.isValid() ? QString(fallback(bytes)) : QString::fromLatin1(bytes);
    }
    content.replace("\r\n", "\n"); content.replace('\r', '\n');
    if (content.startsWith(QChar(0xfeff))) content.remove(0, 1);
    QVariantList result;
    auto append = [&result](double start, double end, QString text) {
        if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end <= start || result.size() >= 10000) return;
        text.remove(QRegularExpression("<[^>]*>"));
        if (!text.trimmed().isEmpty()) result.append(QVariantMap{{"start", start}, {"end", end}, {"text", text.trimmed()}});
    };
    const QString extension = QFileInfo(path).suffix().toLower();
    if (extension == "ass" || extension == "ssa") {
        bool events = false;
        QStringList format{"layer", "start", "end", "style", "name", "marginl", "marginr", "marginv", "effect", "text"};
        const auto seconds = [](const QString &time) {
            static const QRegularExpression stamp("^(\\d+):(\\d{2}):(\\d{2})\\.(\\d{1,3})$");
            const auto match = stamp.match(time.trimmed());
            if (!match.hasMatch() || match.captured(2).toInt() >= 60 || match.captured(3).toInt() >= 60) return -1.0;
            return match.captured(1).toDouble()*3600 + match.captured(2).toDouble()*60
                + match.captured(3).toDouble() + QString("0." + match.captured(4)).toDouble();
        };
        for (const auto &line : content.split('\n')) {
            const QString trimmed = line.trimmed();
            if (trimmed.startsWith('[')) { events = trimmed.compare("[Events]", Qt::CaseInsensitive) == 0; continue; }
            if (!events) continue;
            if (trimmed.startsWith("Format:", Qt::CaseInsensitive)) {
                format = trimmed.mid(7).toLower().split(','); for (auto &field : format) field = field.trimmed();
            } else if (trimmed.startsWith("Dialogue:", Qt::CaseInsensitive)) {
                const int start = format.indexOf("start"), end = format.indexOf("end"), text = format.indexOf("text");
                if (start < 0 || end < 0 || text != format.size()-1) continue;
                QStringList values; QString remaining = trimmed.mid(9);
                for (int i = 0; i < format.size()-1; ++i) {
                    const int comma = remaining.indexOf(','); if (comma < 0) break;
                    values.append(remaining.left(comma)); remaining.remove(0, comma+1);
                }
                values.append(remaining); if (values.size() != format.size()) continue;
                QString dialogue = values[text];
                // ASS vector drawing is not dialogue text.
                dialogue.remove(QRegularExpression("\\{[^}]*\\\\p[1-9][^}]*\\}.*?(?:\\{[^}]*\\\\p0[^}]*\\}|$)", QRegularExpression::CaseInsensitiveOption));
                dialogue.remove(QRegularExpression("\\{[^}]*\\}"));
                dialogue.replace("\\N", "\n"); dialogue.replace("\\n", "\n"); dialogue.replace("\\h", " ");
                append(seconds(values[start]), seconds(values[end]), dialogue);
            }
            if (result.size() == 10000) break;
        }
    } else if (extension == "srt" || extension == "vtt" || extension == "webvtt") {
        static const QRegularExpression cue("(?:^|\n)(?:(\\d+):)?(\\d{2}):(\\d{2})[,.](\\d{3})[ \\t]+-->[ \\t]+(?:(\\d+):)?(\\d{2}):(\\d{2})[,.](\\d{3})[^\n]*\n([^\n]+(?:\n(?!\n)[^\n]+)*)");
        auto matches = cue.globalMatch(content);
        while (matches.hasNext() && result.size() < 10000) {
            const auto match = matches.next();
            if (match.captured(2).toInt() >= 60 || match.captured(3).toInt() >= 60 || match.captured(6).toInt() >= 60 || match.captured(7).toInt() >= 60) continue;
            append(match.captured(1).toInt()*3600 + match.captured(2).toInt()*60 + match.captured(3).toInt() + match.captured(4).toInt()/1000.0,
                match.captured(5).toInt()*3600 + match.captured(6).toInt()*60 + match.captured(7).toInt() + match.captured(8).toInt()/1000.0, match.captured(9));
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.toMap().value("start").toDouble() < b.toMap().value("start").toDouble(); });
    return result;
}

class RedesignSubtitleTimeline : public QObject
{
public:
    using Publish = std::function<void(QVariantList, bool, QString)>;
    RedesignSubtitleTimeline(QObject *parent, Publish publish) : QObject(parent), m_publish(std::move(publish)) {}
    ~RedesignSubtitleTimeline() override { cancel(); }
    void cancel()
    {
        ++m_generation;
        if (m_process) { m_process->disconnect(this); m_process->kill(); m_process->waitForFinished(1000); delete m_process; m_process = nullptr; }
        m_temporary.reset();
    }
    void request(const QUrl &media, const QString &track, bool ass, const QString &directory)
    {
        cancel();
        const QFileInfo file(media.toLocalFile());
        if (!media.isLocalFile() || !file.isFile()) { m_publish({}, false, tr("Add a local subtitle file to view a timeline for network media.")); return; }
        QDir cache(directory); if (!cache.mkpath(".")) { m_publish({}, false, tr("Could not create the subtitle timeline cache.")); return; }
        const QByteArray identity = file.canonicalFilePath().toUtf8() + ':' + QByteArray::number(file.size()) + ':'
            + QByteArray::number(file.lastModified().toMSecsSinceEpoch()) + ':' + track.toUtf8();
        const QString suffix = ass ? ".ass" : ".vtt";
        m_cache = cache.filePath(QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex()) + suffix);
        if (QFileInfo::exists(m_cache)) {
            const auto cues = readRedesignSubtitleCues(m_cache);
            if (!cues.isEmpty()) { m_publish(cues, false, {}); return; }
        }
        m_temporary = std::make_unique<QTemporaryDir>(cache.filePath("extract-XXXXXX"));
        if (!m_temporary->isValid()) { m_publish({}, false, tr("Could not prepare subtitle extraction.")); return; }
        m_output = QDir(m_temporary->path()).filePath("track" + suffix);
        m_process = new QProcess(this);
#ifdef _WIN32
        m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
#endif
        m_process->setStandardOutputFile(QProcess::nullDevice());
        m_process->setStandardErrorFile(QProcess::nullDevice());
        const auto generation = m_generation;
        auto *process = m_process;
        connect(process, &QProcess::finished, this, [this, process, generation](int code, QProcess::ExitStatus status) {
            if (generation != m_generation) return;
            const auto cues = code == 0 && status == QProcess::NormalExit ? readRedesignSubtitleCues(m_output) : QVariantList{};
            if (!cues.isEmpty()) { QFile::remove(m_cache); QFile::copy(m_output, m_cache); }
            m_process = nullptr;
            process->deleteLater();
            m_temporary.reset();
            m_publish(cues, false, cues.isEmpty() ? tr("No text cues could be extracted from this track.") : QString{});
        });
        connect(process, &QProcess::errorOccurred, this, [this, process, generation](QProcess::ProcessError error) {
            if (generation != m_generation || error != QProcess::FailedToStart) return;
            m_process = nullptr;
            process->deleteLater();
            m_temporary.reset();
            m_publish({}, false, tr("The subtitle extraction helper is unavailable."));
        });
        m_publish({}, true, tr("Reading subtitle timeline…"));
        const QString program = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef _WIN32
            "vlc-waveform.exe"
#else
            "vlc-waveform"
#endif
        );
        QTimer::singleShot(60000, process, [process] { if (process->state() != QProcess::NotRunning) process->kill(); });
        process->start(program, {media.toString(QUrl::FullyEncoded), QDir::fromNativeSeparators(m_output), ass ? "subtitles-ass" : "subtitles-text", track});
    }
private:
    Publish m_publish;
    QProcess *m_process = nullptr;
    std::unique_ptr<QTemporaryDir> m_temporary;
    QString m_cache, m_output;
    quint64 m_generation = 0;
};
