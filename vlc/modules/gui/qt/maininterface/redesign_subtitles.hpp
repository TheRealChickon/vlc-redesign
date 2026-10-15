// SPDX-License-Identifier: GPL-3.0-or-later
// REST protocol and public application identifier from OpenSubtitles' GPL VLSub:
// https://github.com/opensubtitles/vlsub-opensubtitles-com
// Credentials stay in this session. System curl validates TLS certificates.
#pragma once
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QPointer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QtEndian>
#include <functional>
#include <memory>

class RedesignSubtitles : public QObject
{
public:
    RedesignSubtitles(QObject *parent, QString directory, std::function<void()> changed)
        : QObject(parent), m_directory(std::move(directory)), m_changed(std::move(changed)) {}
    ~RedesignSubtitles() override { cancel(false); }

    QVariantMap state() const { return {{"busy", m_busy}, {"authenticated", !m_token.isEmpty()},
        {"message", m_message}, {"rows", m_rows}, {"completed", m_cursor}, {"total", m_queue.size()}}; }

    void cancel(bool notify = true)
    {
        ++m_generation;
        if (m_process) { m_process->disconnect(this); m_process->kill(); m_process->deleteLater(); m_process = nullptr; }
        m_busy = false;
        if (notify) { m_message = tr("Cancelled. Completed downloads remain available."); m_changed(); }
    }
    void logout() { cancel(false); m_token.clear(); m_base = apiBase(); m_message = tr("Signed out."); m_changed(); }
    void login(const QString &username, const QString &password, const QString &applicationKey)
    {
        cancel(false);
        if (!applicationKey.trimmed().isEmpty()) {
            if (!QRegularExpression("^[A-Za-z0-9_-]{10,128}$").match(applicationKey.trimmed()).hasMatch()) {
                m_message = tr("The application key is invalid."); m_changed(); return;
            }
            m_key = applicationKey.trimmed().toUtf8();
        }
        if (username.trimmed().isEmpty() || password.isEmpty()) {
            m_message = tr("Enter your OpenSubtitles.com username and password."); m_changed(); return;
        }
        m_busy = true; m_message = tr("Signing in…"); m_changed();
        request(apiBase().resolved(QUrl("login")), QJsonObject{{"username", username.trimmed()}, {"password", password}}, true,
            [this](int status, QByteArray body) {
                const auto json = QJsonDocument::fromJson(body).object();
                const QString token = json.value("token").toString();
                m_busy = false;
                if (status != 200 || token.isEmpty()) { m_message = error(status, json); m_changed(); return; }
                m_token = token.toUtf8(); m_base = apiBase();
                QString host = json.value("base_url").toString();
                host.remove(QRegularExpression("^https?://"));
                if (host == "api.opensubtitles.com" || host == "vip-api.opensubtitles.com")
                    m_base = QUrl("https://" + host + "/api/v1/");
                m_message = tr("Signed in. Search and select a subtitle for each file."); m_changed();
            });
    }
    void search(const QVariantList &media, const QString &language)
    {
        cancel(false); m_rows.clear(); m_queue.clear(); m_cursor = 0; m_language = language.toLower();
        if (m_token.isEmpty()) m_base = apiBase();
        if (!QRegularExpression("^[a-z]{2,3}(?:-[a-z]{2,4})?$").match(m_language).hasMatch()) {
            m_message = tr("Choose a subtitle language."); m_changed(); return;
        }
        // Sequential requests keep playback responsive and respect provider limits.
        for (const auto &value : media) {
            const auto row = value.toMap();
            if (!row.value("enabled", true).toBool()) continue;
            const QUrl url(row.value("media").toString());
            if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isFile()) continue;
            const QFileInfo info(url.toLocalFile());
            m_rows.append(QVariantMap{{"media", row.value("media")}, {"label", row.value("label")},
                {"query", row.value("query", info.completeBaseName())}, {"enabled", true}, {"selected", -1},
                {"candidates", QVariantList{}}, {"status", tr("Waiting")}});
        }
        if (m_rows.isEmpty()) { m_message = tr("Select local media files to search."); m_changed(); return; }
        for (int i = 0; i < m_rows.size(); ++i) m_queue.append(i);
        m_busy = true; m_message = tr("Searching OpenSubtitles…"); m_changed(); searchNext();
    }
    void select(int index, int choice, bool enabled)
    {
        if (m_busy || index < 0 || index >= m_rows.size()) return;
        auto row = m_rows[index].toMap();
        if (choice < -1 || choice >= row.value("candidates").toList().size()) return;
        row["selected"] = choice; row["enabled"] = enabled; m_rows[index] = row; m_changed();
    }
    void download()
    {
        if (m_busy) return;
        if (m_token.isEmpty()) { m_message = tr("Sign in to OpenSubtitles.com to download subtitles."); m_changed(); return; }
        m_queue.clear(); m_cursor = 0;
        for (int i = 0; i < m_rows.size(); ++i) {
            const auto row = m_rows[i].toMap();
            const int selected = row.value("selected", -1).toInt();
            const auto candidates = row.value("candidates").toList();
            if (row.value("enabled").toBool() && selected >= 0 && selected < candidates.size()
                && candidates[selected].toMap().value("url").toString().isEmpty()) m_queue.append(i);
        }
        if (m_queue.isEmpty()) { m_message = tr("Select a search result to download."); m_changed(); return; }
        m_busy = true; m_message = tr("Downloading selected subtitles…"); m_changed(); downloadNext();
    }
    QVariantList downloadedPairs() const
    {
        QVariantList pairs;
        for (const auto &value : m_rows) {
            const auto row = value.toMap();
            const auto choices = row.value("candidates").toList();
            const int selected = row.value("selected", -1).toInt();
            if (!row.value("enabled").toBool() || selected < 0 || selected >= choices.size()) continue;
            const QString url = choices[selected].toMap().value("url").toString();
            if (!url.isEmpty()) pairs.append(QVariantMap{{"media", row.value("media")}, {"subtitle", url}, {"enabled", true}});
        }
        return pairs;
    }

private:
    using Completion = std::function<void(int, QByteArray)>;
    QUrl apiBase() const
    {
        // Internal loopback fixture, only during explicitly enabled development QA.
        if (!qEnvironmentVariableIsEmpty("VLC_REDESIGN_QA_DIR")) {
            const QUrl fixture(qEnvironmentVariable("VLC_REDESIGN_QA_SUBTITLE_API"));
            if (fixture.scheme() == "http" && fixture.host() == "127.0.0.1") return fixture;
        }
        return QUrl("https://api.opensubtitles.com/api/v1/");
    }
    static QByteArray configString(QByteArray text)
    {
        text.replace("\\", "\\\\"); text.replace("\"", "\\\"");
        text.replace("\r", "\\r"); text.replace("\n", "\\n");
        return '"' + text + '"';
    }
    void request(const QUrl &url, const QJsonObject &payload, bool api, Completion completed)
    {
#ifdef _WIN32
        const QString executable = QDir(qEnvironmentVariable("SystemRoot", "C:/Windows")).filePath("System32/curl.exe");
#else
        const QString executable = QStringLiteral("/usr/bin/curl");
#endif
        if (!QFileInfo(executable).isFile()) { completed(0, {}); return; }
        auto *process = new QProcess(this); m_process = process;
#ifdef _WIN32
        process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#endif
        QByteArray input = "url = " + configString(url.toEncoded()) + '\n';
        if (api) {
            input += "header = " + configString("Api-Key: " + m_key) + '\n';
            input += "header = \"Content-Type: application/json\"\n";
            input += "header = \"User-Agent: VLSub VLC-Redesign/1.0\"\n";
            if (!m_token.isEmpty()) input += "header = " + configString("Authorization: Bearer " + m_token) + '\n';
        }
        if (!payload.isEmpty()) input += "data = " + configString(QJsonDocument(payload).toJson(QJsonDocument::Compact)) + '\n';
        const bool fixture = url.scheme() == "http" && url.host() == "127.0.0.1"
            && !qEnvironmentVariableIsEmpty("VLC_REDESIGN_QA_DIR");
        const auto generation = m_generation;
        auto output = std::make_shared<QByteArray>();
        auto done = std::make_shared<bool>(false);
        auto finish = [this, process, output, done, completed, generation](int exitCode) {
            if (*done) return;
            *done = true;
            output->append(process->readAllStandardOutput());
            if (m_process == process) m_process = nullptr;
            process->deleteLater();
            if (generation != m_generation) return;
            const int split = output->lastIndexOf('\n');
            const int status = exitCode == 0 && split >= 0 ? output->mid(split + 1).trimmed().toInt() : 0;
            completed(status, status ? output->left(split) : QByteArray{});
        };
        connect(process, &QProcess::readyReadStandardOutput, this, [process, output] {
            output->append(process->readAllStandardOutput());
            if (output->size() > 8 * 1024 * 1024) process->kill();
        });
        connect(process, &QProcess::finished, this, [finish](int code, QProcess::ExitStatus) { finish(code); });
        connect(process, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) finish(-1);
        });
        QStringList arguments{"--disable", "--silent", "--show-error", "--connect-timeout", "8", "--max-time", "25",
            "--max-filesize", "8388608", "--proto", fixture ? "=http" : "=https", "--proto-redir", "=https", "--write-out", "\n%{http_code}", "--config", "-"};
        if (!api) arguments.append({"--location", "--max-redirs", "3"});
        process->start(executable, arguments);
        process->write(input); process->closeWriteChannel();
    }
    QString error(int status, const QJsonObject &json)
    {
        if (status == 401) { m_token.clear(); return tr("Session expired. Sign in again."); }
        if (status == 429) return tr("OpenSubtitles request limit reached. Try again later.");
        if (status == 406) return tr("Your OpenSubtitles download allowance is exhausted.");
        if (status == 0) return tr("Could not connect securely to OpenSubtitles. Check your connection and system curl.");
        const QString message = json.value("message").toString().left(300);
        return message.isEmpty() ? tr("OpenSubtitles returned an error (%1).").arg(status) : message;
    }
    QString hash(const QString &path) const
    {
        QFile file(path);
        if (!file.open(QFile::ReadOnly) || file.size() < 131072) return {};
        quint64 sum = quint64(file.size());
        for (const qint64 offset : {qint64(0), file.size() - 65536}) {
            if (!file.seek(offset)) return {};
            const QByteArray block = file.read(65536);
            if (block.size() != 65536) return {};
            for (int i = 0; i < block.size(); i += 8) sum += qFromLittleEndian<quint64>(block.constData() + i);
        }
        return QString("%1").arg(sum, 16, 16, QChar('0'));
    }
    void searchNext(bool fallback = false)
    {
        if (m_cursor >= m_queue.size()) { m_busy = false; m_message = tr("Search complete. Review the results before downloading."); m_changed(); return; }
        const int index = m_queue[m_cursor]; auto row = m_rows[index].toMap();
        row["status"] = tr("Searching…"); m_rows[index] = row; m_changed();
        QUrl url = m_base.resolved(QUrl("subtitles")); QUrlQuery query;
        query.addQueryItem("languages", m_language); query.addQueryItem("order_by", "download_count"); query.addQueryItem("order_direction", "desc");
        const QString movieHash = fallback ? QString{} : hash(QUrl(row.value("media").toString()).toLocalFile());
        if (!movieHash.isEmpty()) query.addQueryItem("moviehash", movieHash);
        else {
            QString title = row.value("query").toString();
            static const QRegularExpression episode("[ ._-]*(?:S(\\d{1,2})E(\\d{1,3})|(\\d{1,2})x(\\d{1,3}))", QRegularExpression::CaseInsensitiveOption);
            const auto match = episode.match(title);
            if (match.hasMatch()) {
                query.addQueryItem("type", "episode");
                query.addQueryItem("season_number", QString::number(match.captured(1).isEmpty() ? match.captured(3).toInt() : match.captured(1).toInt()));
                query.addQueryItem("episode_number", QString::number(match.captured(2).isEmpty() ? match.captured(4).toInt() : match.captured(2).toInt()));
                title = title.left(match.capturedStart());
            }
            title.replace(QRegularExpression("[._]+"), " "); query.addQueryItem("query", title);
        }
        url.setQuery(query);
        request(url, {}, true, [this, index, fallback, movieHash](int status, QByteArray body) {
            const auto json = QJsonDocument::fromJson(body).object();
            if (status != 200) { m_busy = false; m_message = error(status, json); m_changed(); return; }
            const auto data = json.value("data").toArray();
            if (data.isEmpty() && !fallback && !movieHash.isEmpty()) { searchNext(true); return; }
            QVariantList candidates;
            for (const auto &entry : data) {
                const auto attributes = entry.toObject().value("attributes").toObject();
                for (const auto &value : attributes.value("files").toArray()) {
                    const auto file = value.toObject(); const qint64 id = file.value("file_id").toVariant().toLongLong();
                    if (id <= 0) continue;
                    QString label = file.value("file_name").toString();
                    if (label.isEmpty()) label = attributes.value("release").toString();
                    if (label.isEmpty()) label = tr("Subtitle %1").arg(id);
                    label += " · " + attributes.value("language").toString();
                    if (attributes.value("moviehash_match").toBool()) label += " · " + tr("File hash match");
                    candidates.append(QVariantMap{{"label", label}, {"fileId", id}, {"url", ""}, {"hashMatch", attributes.value("moviehash_match").toBool()}});
                    if (candidates.size() >= 100) break;
                }
                if (candidates.size() >= 100) break;
            }
            auto row = m_rows[index].toMap(); row["candidates"] = candidates;
            row["status"] = candidates.isEmpty() ? tr("No results") : tr("Choose a result");
            row["selected"] = candidates.size() == 1 && candidates[0].toMap().value("hashMatch").toBool() ? 0 : -1;
            m_rows[index] = row; ++m_cursor; m_changed();
            const auto generation = m_generation;
            QTimer::singleShot(1100, this, [this, generation] { if (m_busy && generation == m_generation) searchNext(); });
        });
    }
    void downloadNext()
    {
        if (m_cursor >= m_queue.size()) { m_busy = false; m_message = tr("Downloads complete. Apply the reviewed subtitles to your media files."); m_changed(); return; }
        const int index = m_queue[m_cursor]; auto row = m_rows[index].toMap();
        const int selected = row.value("selected").toInt(); const auto candidate = row.value("candidates").toList()[selected].toMap();
        row["status"] = tr("Downloading…"); m_rows[index] = row; m_changed();
        request(m_base.resolved(QUrl("download")), {{"file_id", QJsonValue::fromVariant(candidate.value("fileId"))}, {"sub_format", "srt"}}, true,
            [this, index, selected, candidate](int status, QByteArray body) {
                const auto json = QJsonDocument::fromJson(body).object();
                if (status != 200) { m_busy = false; m_message = error(status, json); m_changed(); return; }
                const QUrl link(json.value("link").toString()); const QString host = link.host();
                const bool fixture = !qEnvironmentVariableIsEmpty("VLC_REDESIGN_QA_DIR") && link.scheme() == "http" && host == "127.0.0.1";
                const bool trusted = link.scheme() == "https" && (host == "opensubtitles.com" || host.endsWith(".opensubtitles.com") || host.endsWith(".opensubtitles.org"));
                if ((!trusted && !fixture) || !link.userInfo().isEmpty()) { m_busy = false; m_message = tr("The provider returned an invalid download address."); m_changed(); return; }
                request(link, {}, false, [this, index, selected, candidate](int status, QByteArray content) {
                    if (status != 200 || content.isEmpty() || !content.contains("-->")) {
                        m_busy = false; m_message = tr("The downloaded file is not a readable SRT subtitle."); m_changed(); return;
                    }
                    const auto row = m_rows[index].toMap();
                    const QString media = row.value("media").toString();
                    const QString name = QString::fromLatin1(QCryptographicHash::hash(media.toUtf8(), QCryptographicHash::Sha256).toHex())
                        + '-' + candidate.value("fileId").toString() + '.' + m_language + ".srt";
                    QDir().mkpath(m_directory); QSaveFile file(QDir(m_directory).filePath(name));
                    if (!file.open(QFile::WriteOnly) || file.write(content) != content.size() || !file.commit()) {
                        m_busy = false; m_message = tr("Could not save the subtitle file."); m_changed(); return;
                    }
                    auto updated = row; auto candidates = row.value("candidates").toList(); auto choice = candidates[selected].toMap();
                    choice["url"] = QUrl::fromLocalFile(file.fileName()).toString(QUrl::FullyEncoded); candidates[selected] = choice;
                    updated["candidates"] = candidates; updated["status"] = tr("Downloaded"); m_rows[index] = updated;
                    ++m_cursor; m_changed(); const auto generation = m_generation;
                    QTimer::singleShot(1100, this, [this, generation] { if (m_busy && generation == m_generation) downloadNext(); });
                });
            });
    }
    QString m_directory, m_message, m_language;
    QUrl m_base = QUrl("https://api.opensubtitles.com/api/v1/");
    // Published, non-secret identifier supplied with the provider's open source VLSub.
    QByteArray m_key = "d3Sba6j6VYnty3ir5T8GXYoAuiLSBf0S", m_token;
    QVariantList m_rows; QList<int> m_queue; int m_cursor = 0;
    bool m_busy = false; quint64 m_generation = 0;
    QPointer<QProcess> m_process;
    std::function<void()> m_changed;
};
