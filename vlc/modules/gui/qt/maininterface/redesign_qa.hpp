// SPDX-License-Identifier: GPL-2.0-or-later
// Explicitly enabled internal checks for the portable development build.
#pragma once
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QTimer>
#include <QCoreApplication>
#include <vlc_interface.h>
#include <QApplication>
#include <QWidget>
#include <QKeyEvent>
#include "util/navigation_history.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "dialogs/toolbar/controlbar_profile_model.hpp"
#include "dialogs/toolbar/controlbar_profile.hpp"
#include "player/control_list_model.hpp"
#include "player/player_controlbar_model.hpp"
#include "medialibrary/medialib.hpp"
#include "medialibrary/mlhelper.hpp"
#include "medialibrary/mlbasemodel.hpp"
#include "menus/qml_menu_wrapper.hpp"
#include <memory>
#include <functional>
#include "redesign_crash_qa.hpp"
#include "redesign_subtitle_timeline.hpp"

inline QQuickItem *findRedesignItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems())
        if (auto *found = findRedesignItem(child, name)) return found;
    return nullptr;
}

inline QVariantMap redesignQmlMap(QObject *object, const char *property)
{
    const auto value = object->property(property);
    if (value.canConvert<QJSValue>()) return value.value<QJSValue>().toVariant().toMap();
    return value.toMap();
}

inline QQuickItem *findVisibleRedesignItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name && root->isVisible()) return root;
    for (auto *child : root->childItems())
        if (auto *found = findVisibleRedesignItem(child, name)) return found;
    return nullptr;
}

// QML "var" properties arrive wrapped in a QJSValue
inline QObject *redesignQmlObject(const QVariant &value)
{
    return value.canConvert<QJSValue>() ? value.value<QJSValue>().toQObject() : value.value<QObject *>();
}

#include "redesign_parity_qa.hpp"
#include "redesign_full_video_qa.hpp"
#include "redesign_dialogs_qa.hpp"

inline void runRedesignQA(MainCtx *ctx, QQuickItem *root, QQmlEngine *engine)
{
    const QString destination = qEnvironmentVariable("VLC_REDESIGN_QA_DIR");
    if (destination.isEmpty()) return;
    installRedesignCrashEvidence();
    QDir(destination).mkpath(QStringLiteral("."));
    const auto results = std::make_shared<QJsonObject>();
    const auto warnings = std::make_shared<QJsonArray>();
    QObject::connect(ctx->getIntf()->p_mainPlayerController, &PlayerController::statisticsUpdated, root,
        [results](const input_stats_t &stats) {
            results->insert("decodedVideoFrames", qint64(stats.i_decoded_video));
            results->insert("decodedAudioBlocks", qint64(stats.i_decoded_audio));
        });
    QObject::connect(engine, &QQmlEngine::warnings, root, [warnings](const QList<QQmlError> &errors) {
        for (const auto &error : errors) warnings->append(error.toString());
    });
    QTimer::singleShot(500, root, [ctx, root] {
        if (root->window() && !qEnvironmentVariableIsSet("VLC_REDESIGN_QA_SMALL_WINDOW")) root->window()->resize(1440, 1024);
        bool schemeSet = false;
        const int scheme = qEnvironmentVariableIntValue("VLC_REDESIGN_QA_SCHEME", &schemeSet);
        ctx->getColorScheme()->setCurrentIndex(schemeSet && scheme >= 0 && scheme <= 2 ? scheme : 2);
        const QString fixture = qEnvironmentVariable("VLC_REDESIGN_QA_MEDIA");
        if (!fixture.isEmpty()) {
            const QString libraryDirectory = QDir(qEnvironmentVariable("VLC_REDESIGN_QA_DIR")).filePath("library-fixtures");
            QDir().mkpath(libraryDirectory);
            const QString media = QDir(libraryDirectory).filePath(QFileInfo(fixture).fileName());
            if (!QFileInfo::exists(media)) QFile::copy(fixture, media);
            if(!qEnvironmentVariableIsSet("VLC_REDESIGN_QA_FULLVIDEO"))
                ctx->addRedesignMediaFolder(libraryDirectory);
            if (qEnvironmentVariableIsSet("VLC_REDESIGN_QA_PLAYER_FIRST")) ctx->setPlayerView(true);
            ctx->getIntf()->p_mainPlaylistController->append(QVariantList{QUrl::fromLocalFile(media)}, true);
        }
    });
    auto *ready = new QTimer(root);
    ready->setInterval(500);
    const auto attempts = std::make_shared<int>(0);
    QObject::connect(ready, &QTimer::timeout, root, [ctx, root, engine, destination, results, warnings, ready, attempts] {
        auto *player = ctx->getIntf()->p_mainPlayerController;
        const bool hasFixture = !qEnvironmentVariableIsEmpty("VLC_REDESIGN_QA_MEDIA");
        if (hasFixture && (!player->getLength().valid() || player->getBuffering() < 1 || player->getPosition() <= .01) && ++*attempts < 60)
            return;
        ready->stop();
        if (hasFixture) ctx->setPlayerView(true);
        const auto screenshot = [root, destination, results](const QString &name) {
            auto *window = root->window();
            const bool saved = window && window->grabWindow().save(QDir(destination).filePath(name + ".png"));
            results->insert(name, saved);
        };
        screenshot(QStringLiteral("player"));
        results->insert("playbackStarted", player->isStarted());
        results->insert("initialPosition", player->getPosition());
        if (qEnvironmentVariableIsSet("VLC_REDESIGN_QA_FULLVIDEO")) {
            runRedesignFullVideoQA(ctx,root,engine,destination,results,warnings,screenshot);
            return;
        }
        if (qEnvironmentVariableIsSet("VLC_REDESIGN_QA_PARITY")) {
            runRedesignParityQA(ctx,root,destination,results,warnings,screenshot);
            return;
        }
        if (qEnvironmentVariableIsSet("VLC_REDESIGN_QA_DIALOGS")) {
            runRedesignDialogsQA(ctx,root,destination,results,warnings,screenshot);
            return;
        }
        if (qEnvironmentVariableIsSet("VLC_REDESIGN_QA_TOOLBAR")) {
            { vlc_player_locker lock(ctx->getIntf()->p_player); vlc_player_Pause(ctx->getIntf()->p_player); }
            auto *profiles = ctx->controlbarProfileModel();
            const auto videoLayout = profiles->currentModel()->getModelData(PlayerControlbarModel::Videoplayer);
            const auto audioLayout = profiles->currentModel()->getModelData(PlayerControlbarModel::Audioplayer);
            auto *probe = new QTimer(root); probe->setInterval(750);
            const auto phase = std::make_shared<int>(0);
            const auto previousPosition = std::make_shared<double>(0);
            const auto prepare = [ctx, root, profiles, videoLayout, audioLayout](int index) {
                const std::array<int,11> widths{{2541,1920,1440,960,600,1440,1920,2560,1280,2560,2560}};
                ctx->setIntfUserScaleFactor(index >= 6 && index <= 8 ? 1.5 : 1.0);
                root->window()->resize(widths[index], index == 4 ? 540 : 900);
                if (index == 9) {
                    const std::array<QVector<int>,3> centered{{{ControlListModel::PREVIOUS_BUTTON},
                        {ControlListModel::PLAY_BUTTON},{ControlListModel::VOLUME,ControlListModel::PLAYLIST_BUTTON}}};
                    profiles->currentModel()->setModelData(PlayerControlbarModel::Videoplayer, centered);
                    profiles->currentModel()->setModelData(PlayerControlbarModel::Audioplayer, centered);
                } else if (index == 10) {
                    profiles->currentModel()->setModelData(PlayerControlbarModel::Videoplayer, videoLayout);
                    profiles->currentModel()->setModelData(PlayerControlbarModel::Audioplayer, audioLayout);
                }
            };
            const auto pointer = [root](QQuickItem *seek, double fraction, QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons) {
                const QPointF point = seek->mapToScene(QPointF(seek->width() * fraction, seek->height() / 2));
                QMouseEvent event(type, point, root->window()->mapToGlobal(point), button, buttons, Qt::NoModifier);
                QCoreApplication::sendEvent(root->window(), &event);
            };
            prepare(0);
            QObject::connect(probe, &QTimer::timeout, root, [ctx, root, player, profiles, videoLayout, audioLayout, probe, phase, previousPosition, prepare, pointer, results, warnings, screenshot, destination] {
                auto *bar = findVisibleRedesignItem(root, "redesignControlBar");
                auto *play = bar ? findVisibleRedesignItem(bar, "redesignPlayPauseButton") : nullptr;
                auto *seek = bar ? findVisibleRedesignItem(bar, "redesignSeekBar") : nullptr;
                if (!seek && bar) seek = findVisibleRedesignItem(bar, "nativeSeekBar");
                if (*phase < 11) {
                    const auto suffix = QString::number(*phase);
                    results->insert("toolbarControlsPresent-" + suffix, bar && play && seek);
                    if (bar && play && seek) {
                        const bool compact = bar->property("compactLayout").toBool();
                        const bool expectedCompact = *phase != 9 && bar->width() >= 980 * ctx->getIntfUserScaleFactor();
                        results->insert("toolbarResponsiveMode-" + suffix, compact == expectedCompact);
                        const QRectF seekBounds = seek->mapRectToScene(seek->boundingRect());
                        const QRectF playBounds = play->mapRectToScene(play->boundingRect());
                        results->insert("toolbarTimelineInsideBar-" + suffix,
                            bar->mapRectToScene(bar->boundingRect()).adjusted(-1,-1,1,1).contains(seekBounds));
                        results->insert("toolbarTimelineUsableWidth-" + suffix, seek->width() >= 40);
                        if (compact) {
                            const double delta = seekBounds.center().y() - playBounds.center().y();
                            results->insert("toolbarTimelineCenterDelta-" + suffix, delta);
                            results->insert("toolbarTimelineAlignedWithButtons-" + suffix, std::abs(delta) <= 1.5);
                            auto *row = findVisibleRedesignItem(bar, "redesignTimelineRow");
                            auto *elapsed = findVisibleRedesignItem(bar, "redesignElapsedTime");
                            auto *remaining = findVisibleRedesignItem(bar, "redesignRemainingTime");
                            results->insert("toolbarTimelineRowFitsContents-" + suffix, row && row->height() >= seek->height() - 1);
                            results->insert("toolbarElapsedTimeAligned-" + suffix, elapsed &&
                                std::abs(elapsed->mapRectToScene(elapsed->boundingRect()).center().y() - seekBounds.center().y()) <= 1.5);
                            results->insert("toolbarRemainingTimeAligned-" + suffix, remaining &&
                                std::abs(remaining->mapRectToScene(remaining->boundingRect()).center().y() - seekBounds.center().y()) <= 1.5);
                        } else {
                            results->insert("toolbarStackedTimelineAboveButtons-" + suffix, seekBounds.bottom() <= playBounds.top() + 1.5);
                        }
                        if (*phase == 0 || *phase == 2 || *phase == 4 || *phase == 7 || *phase == 9) screenshot("toolbar-fit-" + suffix);
                    }
                    ++*phase;
                    if (*phase < 11) prepare(*phase);
                    return;
                }
                if (*phase == 11 && seek) {
                    pointer(seek,.4,QEvent::MouseButtonPress,Qt::LeftButton,Qt::LeftButton);
                    pointer(seek,.4,QEvent::MouseButtonRelease,Qt::LeftButton,Qt::NoButton);
                    ++*phase; return;
                }
                if (*phase == 12 && seek) {
                    results->insert("toolbarClickSeeksToPointer", std::abs(player->getPosition() - .4) < .06);
                    pointer(seek,.4,QEvent::MouseButtonPress,Qt::LeftButton,Qt::LeftButton);
                    pointer(seek,.7,QEvent::MouseMove,Qt::NoButton,Qt::LeftButton);
                    pointer(seek,.7,QEvent::MouseButtonRelease,Qt::LeftButton,Qt::NoButton);
                    ++*phase; return;
                }
                if (*phase == 13 && seek) {
                    results->insert("toolbarDragSeeksToPointer", std::abs(player->getPosition() - .7) < .06);
                    *previousPosition = player->getPosition();
                    QKeyEvent event(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
                    QCoreApplication::sendEvent(seek, &event);
                    ++*phase; return;
                }
                results->insert("toolbarKeyboardSeekWorks", player->getPosition() < *previousPosition - .01);
                results->insert("toolbarSeekKeepsPausedState", player->getPlayingState() == PlayerController::PLAYING_STATE_PAUSED);
                profiles->currentModel()->setModelData(PlayerControlbarModel::Videoplayer, videoLayout);
                profiles->currentModel()->setModelData(PlayerControlbarModel::Audioplayer, audioLayout);
                ctx->setIntfUserScaleFactor(1);
                probe->stop(); results->insert("qmlWarnings", *warnings);
                QFile file(QDir(destination).filePath("checks.json"));
                if (file.open(QFile::WriteOnly)) file.write(QJsonDocument(*results).toJson());
                libvlc_Quit(vlc_object_instance(ctx->getIntf()));
            });
            probe->start(); return;
        }
        if (qEnvironmentVariableIsSet("VLC_REDESIGN_QA_SEEK_PREVIEW")) {
            { vlc_player_locker lock(ctx->getIntf()->p_player); vlc_player_Pause(ctx->getIntf()->p_player); }
            auto *probe = new QTimer(root); probe->setInterval(500);
            const auto phase = std::make_shared<int>(0), ticks = std::make_shared<int>(0);
            const auto position = std::make_shared<double>(0);
            const auto imageHash = std::make_shared<QByteArray>();
            QObject::connect(probe, &QTimer::timeout, root, [ctx, root, player, probe, phase, ticks, position, imageHash, results, warnings, screenshot, destination] {
                const auto finish = [ctx, probe, results, warnings, destination] {
                    probe->stop(); results->insert("qmlWarnings", *warnings);
                    QFile file(QDir(destination).filePath("checks.json"));
                    if (file.open(QFile::WriteOnly)) file.write(QJsonDocument(*results).toJson());
                    libvlc_Quit(vlc_object_instance(ctx->getIntf()));
                };
                if (++*ticks > 100) { results->insert("seekPreviewCompletedWithinLimit", false); finish(); return; }
                auto *seek = findVisibleRedesignItem(root, "redesignSeekBar");
                if (!seek || !root->window()) return;
                const auto hover = [root, seek](double fraction) {
                    const QPointF point = seek->mapToScene(QPointF(seek->width() * fraction, seek->height() / 2));
                    QMouseEvent event(QEvent::MouseMove, point, root->window()->mapToGlobal(point), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(root->window(), &event);
                };
                if (*phase == 0) { *position = player->getPosition(); hover(.25); ++*phase; return; }
                if (*phase == 1 || *phase == 2) {
                    const double fraction = *phase == 1 ? .25 : .583333333;
                    const double time = std::floor(fraction * player->getLength().toSeconds() * 2) / 2;
                    if (ctx->redesignSeekPreviewUrl().isEmpty() || std::abs(ctx->redesignSeekPreviewTime() - time) > .51) return;
                    auto *popup = seek->findChild<QObject *>("redesignSeekPreview");
                    if (!popup || !popup->property("opened").toBool()) return;
                    QImage image(ctx->redesignSeekPreviewUrl().toLocalFile());
                    const QByteArray pixels = QByteArray::fromRawData(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes());
                    const QByteArray hash = QCryptographicHash::hash(pixels, QCryptographicHash::Sha256);
                    results->insert(QString("seekPreviewRealImage-%1").arg(*phase), !image.isNull() && image.width() == 208);
                    results->insert(QString("seekPreviewHoverPopup-%1").arg(*phase), popup->property("visible").toBool());
                    results->insert(QString("seekPreviewLeavesPlaybackPosition-%1").arg(*phase), std::abs(player->getPosition() - *position) < .01);
                    screenshot(QString("seek-preview-%1").arg(*phase));
                    if (*phase == 1) { *imageHash = hash; hover(.583333333); ++*phase; return; }
                    results->insert("seekPreviewPositionsHaveDifferentFrames", hash != *imageHash);
                    QMouseEvent event(QEvent::MouseMove, QPointF(400, 200), root->window()->mapToGlobal(QPointF(400, 200)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(root->window(), &event);
                    ++*phase; return;
                }
                if (*phase == 3) {
                    auto *popup = seek->findChild<QObject *>("redesignSeekPreview");
                    results->insert("seekPreviewClearsWhenLeavingSeekBar", ctx->redesignSeekPreviewUrl().isEmpty() && popup && !popup->property("visible").toBool());
                    root->window()->setVisibility(QWindow::FullScreen); ++*phase; return;
                }
                auto *top = findVisibleRedesignItem(root, "redesignPlayerTopBar");
                results->insert("fullScreenKeepsMediaTitleWithPinnedControls", top && top->opacity() > .9);
                screenshot("full-screen-title"); root->window()->setVisibility(QWindow::Windowed);
                results->insert("seekPreviewCompletedWithinLimit", true); finish();
            });
            probe->start(); return;
        }
        if (qEnvironmentVariableIsSet("VLC_REDESIGN_QA_FIRST_RUN")) {
            auto *onboarding = root->findChild<QObject *>("redesignOnboarding");
            results->insert("firstRunOpensRedesignedOnboarding", onboarding && onboarding->property("opened").toBool());
            results->insert("firstRunAvoidsClassicWizard", !DialogsProvider::getInstance(ctx->getIntf())->firstRunDialogVisible());
            const bool small = qEnvironmentVariableIsSet("VLC_REDESIGN_QA_SMALL_WINDOW");
            if (small) {
                results->insert("freshLaunchHasRoomForLibrary", root->window()->width() >= 900 && root->window()->height() >= 600);
                auto *probe = new QTimer(root); probe->setInterval(900);
                const auto phase = std::make_shared<int>(0);
                const auto prepare = [ctx, root, onboarding](int index) {
                    const std::array<QSize,4> sizes{{{600,420},{480,540},{960,720},{1280,865}}};
                    ctx->setIntfUserScaleFactor(index/4 == 1 || index/4 == 3 ? 1.5 : 1.0);
                    root->window()->resize(sizes[index/4]);
                    if (onboarding) onboarding->setProperty("step", index%4);
                };
                prepare(0);
                QObject::connect(probe, &QTimer::timeout, root, [ctx, root, onboarding, probe, phase, prepare, results, warnings, screenshot, destination] {
                    if (*phase < 16 && onboarding) {
                        const QRectF windowBounds(QPointF(0,0), QSizeF(root->window()->size()));
                        const QRectF bounds(onboarding->property("x").toDouble(), onboarding->property("y").toDouble(),
                            onboarding->property("width").toDouble(), onboarding->property("height").toDouble());
                        auto *next = findVisibleRedesignItem(root->window()->contentItem(), "redesignOnboardingNext");
                        auto *close = findVisibleRedesignItem(root->window()->contentItem(), "redesignOnboardingClose");
                        const auto suffix = QString::number(*phase);
                        results->insert("welcomeFitsFrame-" + suffix, windowBounds.contains(bounds));
                        results->insert("welcomeLeavesLibraryVisible-" + suffix, bounds.width() <= windowBounds.width() * .85 && bounds.height() <= windowBounds.height() - 36);
                        results->insert("welcomeNextVisible-" + suffix, next && windowBounds.contains(next->mapRectToScene(next->boundingRect())) && bounds.adjusted(-2,-2,2,2).contains(next->mapRectToScene(next->boundingRect())));
                        results->insert("welcomeCloseVisible-" + suffix, close && windowBounds.contains(close->mapRectToScene(close->boundingRect())));
                        if (*phase%4 == 0) {
                            auto *body = findVisibleRedesignItem(root->window()->contentItem(), "redesignOnboardingBody");
                            auto *title = findVisibleRedesignItem(root->window()->contentItem(), "redesignOnboardingTitle");
                            results->insert("welcomeContentFitsWithoutScroll-" + suffix, body && body->property("contentHeight").toDouble() <= body->property("availableHeight").toDouble() + 2);
                            results->insert("welcomeHeadingHasCompactSize-" + suffix, title && title->property("font").value<QFont>().pixelSize() <= 27 * ctx->getIntfUserScaleFactor());
                        }
                        screenshot("welcome-responsive-" + suffix);
                        ++*phase;
                        if (*phase < 16) prepare(*phase);
                        else {
                            const auto before = ctx->redesignConfigValue("qt-privacy-ask");
                            QMetaObject::invokeMethod(onboarding, "close");
                            results->insert("closingWelcomePreservesPrivacyChoice", ctx->redesignConfigValue("qt-privacy-ask") == before);
                        }
                        return;
                    }
                    results->insert("closingWelcomeShowsLibrary", onboarding && !onboarding->property("opened").toBool());
                    results->insert("qmlWarnings", *warnings);
                    QFile file(QDir(destination).filePath("checks.json"));
                    if (file.open(QFile::WriteOnly)) file.write(QJsonDocument(*results).toJson());
                    probe->stop(); libvlc_Quit(vlc_object_instance(ctx->getIntf()));
                });
                probe->start(); return;
            }
            auto *probe = new QTimer(root); probe->setInterval(750);
            const auto phase = std::make_shared<int>(0);
            QObject::connect(probe, &QTimer::timeout, root, [ctx, root, onboarding, probe, phase, results, warnings, screenshot, destination] {
                if (onboarding && *phase < 4) {
                    const int step = onboarding->property("step").toInt();
                    results->insert(QString("firstRunStep-%1").arg(*phase), step == *phase);
                    screenshot(QString("first-run-%1").arg(*phase));
                    if (*phase == 2) {
                        const auto *metadata = onboarding->findChild<QObject *>("redesignOnboardingMetadata");
                        const auto *updates = onboarding->findChild<QObject *>("redesignOnboardingUpdates");
                        results->insert("firstRunNetworkChoicesDefaultOff", metadata && updates
                            && !metadata->property("checked").toBool() && !updates->property("checked").toBool());
                    }
                    auto *next = onboarding->findChild<QObject *>("redesignOnboardingNext");
                    results->insert(QString("firstRunNextButton-%1").arg(*phase), next != nullptr);
                    if (next) QMetaObject::invokeMethod(next, "clicked");
                    ++*phase; return;
                }
                results->insert("firstRunFinishPersistsChoices", onboarding && !onboarding->property("opened").toBool()
                    && !ctx->redesignConfigValue("qt-privacy-ask").toBool() && !ctx->redesignConfigValue("metadata-network-access").toBool());
                probe->stop(); results->insert("qmlWarnings", *warnings);
                QFile file(QDir(destination).filePath("checks.json"));
                if (file.open(QFile::WriteOnly)) file.write(QJsonDocument(*results).toJson());
                libvlc_Quit(vlc_object_instance(ctx->getIntf()));
            });
            probe->start(); return;
        }
        if (qEnvironmentVariableIsSet("VLC_REDESIGN_QA_TIMELINE")) {
            const bool external = qEnvironmentVariableIsSet("VLC_REDESIGN_QA_TIMELINE_EXTERNAL");
            if (external) {
                QFile subtitle(QDir(destination).filePath("timeline-fps.srt"));
                if (subtitle.open(QFile::WriteOnly)) {
                    subtitle.write("1\n00:00:04,000 --> 00:00:07,000\nNative subtitle size\n"); subtitle.close();
                    ctx->saveRedesignSubtitlePairs({QVariantMap{{"media", player->getUrl().toString(QUrl::FullyEncoded)},
                        {"subtitle", QUrl::fromLocalFile(subtitle.fileName()).toString(QUrl::FullyEncoded)}, {"enabled", true}}});
                }
            }
            player->setSubtitleFPS(30);
            { vlc_player_locker lock(ctx->getIntf()->p_player);
              vlc_player_Pause(ctx->getIntf()->p_player);
              if (vlc_player_GetTrackCount(ctx->getIntf()->p_player, SPU_ES))
                  vlc_player_SelectTrack(ctx->getIntf()->p_player,
                      vlc_player_GetTrackAt(ctx->getIntf()->p_player, SPU_ES, 0),
                      VLC_PLAYER_SELECT_EXCLUSIVE); }
            auto *probe = new QTimer(root);
            probe->setInterval(500);
            const auto phase = std::make_shared<int>(-1);
            const auto ticks = std::make_shared<int>(0);
            const auto expected = std::make_shared<double>(0);
            QObject::connect(probe, &QTimer::timeout, root, [ctx, root, player, external, probe, phase, ticks, expected, results, warnings, screenshot, destination] {
                const auto finish = [ctx, probe, results, warnings, destination] {
                    probe->stop(); results->insert("qmlWarnings", *warnings);
                    QFile file(QDir(destination).filePath("checks.json"));
                    if (file.open(QFile::WriteOnly)) file.write(QJsonDocument(*results).toJson());
                    libvlc_Quit(vlc_object_instance(ctx->getIntf()));
                };
                if (++*ticks > 140) { results->insert("timelineCompletedWithinLimit", false); finish(); return; }
                if (*phase == -1) {
                    bool selected;
                    { vlc_player_locker lock(ctx->getIntf()->p_player);
                      selected = vlc_player_GetSelectedTrack(ctx->getIntf()->p_player, SPU_ES) != nullptr; }
                    if (!selected) return;
                    ctx->requestRedesignSubtitleTimeline(); ++*phase; return;
                }
                if (ctx->redesignTimelineBusy()) return;
                if (*phase == 0) {
                    const auto cues = ctx->redesignTimelineCues();
                    const bool valid = !cues.isEmpty() && cues[0].toMap().value("text").toString() == "Native subtitle size"
                        && cues[0].toMap().value("end").toDouble() > cues[0].toMap().value("start").toDouble();
                    results->insert("embeddedTimelineHasRealTimedDialogue", valid);
                    if (!valid) { results->insert("timelineMessage", ctx->redesignTimelineMessage()); finish(); return; }
                    const double start = cues[0].toMap().value("start").toDouble();
                    *expected = (start * (external ? .5 : 1.0) + 1.0) / player->getLength().toSeconds();
                    player->setSubtitleDelayMS(1000);
                    results->insert("timelineCueTimeMatchesNativeFPSPolicy", std::abs(ctx->redesignSubtitleCueTime(start)
                        - *expected * player->getLength().toSeconds()) < .02);
                    auto *button = findRedesignItem(root, "redesignTracksButton");
                    auto *panel = button ? button->property("livePanel").value<QObject *>() : nullptr;
                    results->insert("timelinePanelPresent", panel != nullptr);
                    if (!panel) { finish(); return; }
                    panel->setProperty("currentTab", 0); panel->setProperty("showSubtitleTimeline", true);
                    QMetaObject::invokeMethod(panel, "open");
                    ++*phase; return;
                }
                if (*phase == 1) {
                    screenshot("embedded-timeline");
                    auto *cue = root->window() ? findRedesignItem(root->window()->contentItem(), "redesignTimelineCue") : nullptr;
                    results->insert("timelineCueControlPresent", cue != nullptr);
                    if (cue) QMetaObject::invokeMethod(cue, "clicked");
                    ++*phase; return;
                }
                if (*phase == 2) {
                    results->insert("timelineCueSeekIncludesSubtitleDelay", std::abs(player->getPosition() - *expected) < .01);
                    { vlc_player_locker lock(ctx->getIntf()->p_player);
                      vlc_player_UnselectTrackCategory(ctx->getIntf()->p_player, SPU_ES); }
                    ++*phase; return;
                }
                results->insert("timelineClearsWhenTrackIsDisabled", ctx->redesignTimelineCues().isEmpty() && !ctx->redesignTimelineBusy());
                results->insert("timelineCompletedWithinLimit", true);
                finish();
            });
            probe->start(); return;
        }
        if (qEnvironmentVariableIsSet("VLC_REDESIGN_QA_LAYOUT")) {
            auto *history = engine->singletonInstance<NavigationHistory *>(qmlTypeId("VLC.MainInterface", 1, 0, "History"));
            auto *style = engine->singletonInstance<QObject *>(qmlTypeId("VLC.Style", 1, 0, "VLCStyle"));
            const auto originalPins = ctx->settingValue("Redesign/sidebar-pins", "[]");
            auto *probe = new QTimer(root); probe->setInterval(1200);
            const auto phase = std::make_shared<int>(0);
            const auto previousPosition = std::make_shared<double>(0);
            const auto queueCount = std::make_shared<int>(0);
            QObject::connect(probe, &QTimer::timeout, root, [ctx, root, player, history, style, originalPins, probe, phase, previousPosition, queueCount, results, warnings, screenshot, destination] {
                auto *settings = root->findChild<QObject *>("redesignSettings");
                if (*phase == 0 || *phase == 2 || *phase == 4) {
                    ctx->setPlayerView(false);
                    const int width = *phase == 0 ? 1280 : *phase == 2 ? 960 : 1920;
                    const int height = *phase == 4 ? 1080 : 720;
                    ctx->setIntfUserScaleFactor(*phase == 4 ? 1.5 : 1.0);
                    root->window()->resize(width, height);
                    if (settings) QMetaObject::invokeMethod(settings, "open");
                    ++*phase; return;
                }
                if (*phase == 1 || *phase == 3 || *phase == 5) {
                    const QSizeF available(root->window()->size());
                    const QRectF bounds(settings ? settings->property("x").toDouble() : -1,
                        settings ? settings->property("y").toDouble() : -1,
                        settings ? settings->property("width").toDouble() : 0,
                        settings ? settings->property("height").toDouble() : 0);
                    results->insert(QString("settingsFitViewport-%1").arg(*phase), settings && settings->property("opened").toBool()
                        && QRectF(QPointF(0,0), available).contains(bounds));
                    auto *save = findVisibleRedesignItem(root->window()->contentItem(), "redesignSettingsSave");
                    results->insert(QString("settingsSaveReachable-%1").arg(*phase), save && save->width() > 0 && save->height() > 0
                        && QRectF(QPointF(0,0), available).contains(save->mapRectToScene(save->boundingRect())));
                    screenshot(QString("layout-settings-%1").arg(*phase));
                    if (settings) QMetaObject::invokeMethod(settings, "close");
                    ++*phase; return;
                }
                if (*phase == 6) {
                    ctx->setIntfUserScaleFactor(1); root->window()->resize(1280,720);
                    DialogsProvider::getInstance()->showToolbarEditorDialog(); ++*phase; return;
                }
                if (*phase == 7) {
                    bool fits = false;
                    for (auto *window : QGuiApplication::allWindows()) {
                        if (window->objectName() != "redesignToolbarEditor") continue;
                        fits = window->width() <= root->window()->width() && window->height() <= root->window()->height();
                        if (auto *quick = qobject_cast<QQuickWindow *>(window)) quick->grabWindow().save(QDir(destination).filePath("layout-customize.png"));
                        QMetaObject::invokeMethod(window, "rejected", Q_ARG(bool, true));
                    }
                    results->insert("customizerFitsShortViewport", fits);
                    ctx->setPlayerView(false); history->push({"home"}, Qt::OtherFocusReason);
                    ++*phase; return;
                }
                if (*phase == 8) {
                    auto *pip = findVisibleRedesignItem(root, "pip window");
                    results->insert("miniPlayerNativeVideoAvailable", ctx->canShowVideoPIP() && pip && ctx->hasEmbededVideo());
                    results->insert("miniPlayerTitleMatchesFile", pip && findVisibleRedesignItem(pip, "redesignMiniTitle")
                        && findVisibleRedesignItem(pip, "redesignMiniTitle")->property("text").toString().contains("fixture.mp4"));
                    results->insert("miniPlayerContainsNativeSeek", pip && findVisibleRedesignItem(pip, "redesignMiniSeek"));
                    if (pip) results->insert("miniPlayerFitsViewport", QRectF(QPointF(0,0), QSizeF(root->window()->size())).contains(pip->mapRectToScene(pip->boundingRect())));
                    screenshot("layout-mini-player");
                    if (auto *button = findVisibleRedesignItem(root, "redesignMiniPlay")) QMetaObject::invokeMethod(button, "clicked");
                    ++*phase; return;
                }
                if (*phase == 9) {
                    results->insert("miniPlayerPauseUsesNativePlayer", player->getPlayingState() == PlayerController::PLAYING_STATE_PAUSED);
                    *previousPosition = player->getPosition();
                    if (auto *seek = findVisibleRedesignItem(root, "redesignMiniSeek")) {
                        QKeyEvent event(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
                        QCoreApplication::sendEvent(seek, &event);
                    }
                    ++*phase; return;
                }
                if (*phase == 10) {
                    results->insert("miniPlayerKeyboardSeekUsesNativePlayer", player->getPosition() > *previousPosition + .01);
                    ctx->setPlayerView(true);
                    ++*phase; return;
                }
                if (*phase == 11) {
                    ctx->sendHotkey(Qt::Key_Space, Qt::NoModifier); ++*phase; return;
                }
                if (*phase == 12) {
                    results->insert("nativeSpaceShortcutResumesPlayback", player->getPlayingState() == PlayerController::PLAYING_STATE_PLAYING);
                    ctx->askShowRedesignLibrary(); ++*phase; return;
                }
                if (*phase >= 13 && *phase <= 16) {
                    auto *library = root->findChild<QObject *>("redesignLibraryPanel");
                    results->insert(QString("compactLibraryTab-%1").arg(*phase - 13), library && library->property("opened").toBool()
                        && library->property("currentTab").toInt() == *phase - 13);
                    results->insert("compactLibraryKeepsVideoPlaying", ctx->isPlayerView() && player->getPlayingState() == PlayerController::PLAYING_STATE_PLAYING);
                    screenshot(QString("layout-library-%1").arg(*phase - 13));
                    if (*phase == 13) {
                        *queueCount = ctx->getIntf()->p_mainPlaylistController->count();
                        if (auto *button = findVisibleRedesignItem(root->window()->contentItem(), "redesignLibraryQueue-0")) {
                            results->insert("compactLibraryControlsHavePalette", button->property("backgroundColor").value<QColor>() != QColor(Qt::magenta)
                                && button->property("color").value<QColor>() != QColor(Qt::magenta));
                            QMetaObject::invokeMethod(button, "clicked");
                        }
                    }
                    if (*phase == 14) results->insert("compactLibraryAppendsNativeQueue", ctx->getIntf()->p_mainPlaylistController->count() > *queueCount);
                    if (library) {
                        if (*phase < 16) library->setProperty("currentTab", *phase - 12);
                        else QMetaObject::invokeMethod(library, "close");
                    }
                    ++*phase; return;
                }
                if (*phase == 17) {
                    if (auto *resume = findRedesignItem(root, "redesignResumePrompt")) QMetaObject::invokeMethod(resume, "showResumePanel");
                    ++*phase; return;
                }
                if (*phase == 18) {
                    auto *resume = findVisibleRedesignItem(root, "redesignResumePrompt");
                    const QRectF bounds = resume ? resume->mapRectToScene(resume->boundingRect()) : QRectF{};
                    results->insert("resumePromptNearPlayerControls", resume && bounds.top() > root->height() * .6
                        && std::abs(bounds.center().x() - root->width()/2) < 20);
                    screenshot("layout-resume-prompt");
                    if (resume) QMetaObject::invokeMethod(resume, "hideResumePanel");
                    if (style) QMetaObject::invokeMethod(style, "pinItem", Q_ARG(QVariant, "folder"), Q_ARG(QVariant, destination), Q_ARG(QVariant, "Pinned folder QA"));
                    ctx->setPlayerView(false); history->push({"home"}); ++*phase; return;
                }
                if (*phase == 19) {
                    if (style) { style->setProperty("pinnedItems", QVariantList{}); QMetaObject::invokeMethod(style, "reloadPins"); }
                    const auto saved = QJsonDocument::fromJson(ctx->settingValue("Redesign/sidebar-pins", "[]").toString().toUtf8()).array();
                    results->insert("sidebarPinsPersistInNativeSettings", std::any_of(saved.begin(), saved.end(), [destination](const auto &value) { return value.toObject().value("target").toString() == destination; }));
                    auto *pins = findVisibleRedesignItem(root, "redesignSidebarPins");
                    results->insert("sidebarPinsReloadAndRender", pins && pins->property("count").toInt() > 0);
                    screenshot("layout-sidebar-pins");
                    history->push({"video", "all"}); ++*phase; return;
                }
                if (*phase == 20 || *phase == 22) {
                    auto *sort = root->findChild<SortMenuVideo *>("redesignVideoSortMenu");
                    if (sort) sort->popup(QPoint(300,100), false, {});
                    ++*phase; return;
                }
                if (*phase == 21 || *phase == 23) {
                    const QString actionName = *phase == 21 ? "redesignUnwatchedFirst" : "redesignWatchedFirst";
                    bool found = false;
                    for (auto *widget : QApplication::topLevelWidgets()) if (auto *menu = qobject_cast<QMenu *>(widget)) {
                        for (auto *action : menu->actions()) if (action->objectName() == actionName && menu->isVisible()) { action->trigger(); found = true; }
                        menu->close();
                    }
                    auto *page = findVisibleRedesignItem(root, "redesignVideoAllPage");
                    auto *sort = page ? page->property("sort").value<QObject *>() : nullptr;
                    const auto asObject = [](const QVariant &value) -> QObject * {
                        if (value.canConvert<QObject *>()) return value.value<QObject *>();
                        if (value.canConvert<QJSValue>()) return value.value<QJSValue>().toQObject();
                        return nullptr;
                    };
                    auto *meta = page ? asObject(page->property("_meta")) : nullptr;
                    auto *model = meta ? qobject_cast<MLBaseModel *>(asObject(meta->property("model"))) : nullptr;
                    results->insert(actionName + "Found", found);
                    results->insert(actionName + "Criteria", sort ? sort->property("criteria").toString() : "missing sort context");
                    results->insert(actionName + "Order", sort ? sort->property("order").toInt() : -1);
                    results->insert(actionName + "Grouping", int(ctx->grouping()));
                    results->insert(actionName + "ModelCriteria", model ? model->property("sortCriteria").toString() : "missing native model");
                    results->insert(actionName + "ModelOrder", model ? model->property("sortOrder").toInt() : -1);
                    results->insert(actionName + "UsesNativePlayCount", found && sort && sort->property("criteria").toString() == "playcount"
                        && sort->property("order").toInt() == (*phase == 21 ? Qt::AscendingOrder : Qt::DescendingOrder)
                        && model && model->getMLSortingCriteria() == VLC_ML_SORTING_PLAYCOUNT
                        && model->property("sortOrder").toInt() == (*phase == 21 ? Qt::AscendingOrder : Qt::DescendingOrder)
                        && ctx->grouping() == MainCtx::GROUPING_NONE);
                    if (*phase == 23) { ctx->setSettingValue("Redesign/sidebar-pins", originalPins); if (style) QMetaObject::invokeMethod(style,"reloadPins"); }
                    ++*phase; return;
                }
                screenshot("layout-video-player");
                results->insert("qmlWarnings", *warnings);
                QFile file(QDir(destination).filePath("checks.json"));
                if (file.open(QFile::WriteOnly)) file.write(QJsonDocument(*results).toJson());
                probe->stop(); libvlc_Quit(vlc_object_instance(ctx->getIntf()));
            });
            probe->start(); return;
        }
        if (qEnvironmentVariableIsSet("VLC_REDESIGN_QA_CONTROLS")) {
            { vlc_player_locker lock(ctx->getIntf()->p_player); vlc_player_Pause(ctx->getIntf()->p_player); }
            auto *profiles = ctx->controlbarProfileModel();
            const int originalProfile = profiles->selectedProfile();
            profiles->cloneSelectedProfile("Isolated native controls QA");
            profiles->setSelectedProfile(profiles->rowCount() - 1);
            const QStringList names {"pip", "hdr", "zoom", "sidebar", "settings", "home", "thumbnail", "file",
                "artist", "folder", "playlist", "replay", "bookmarkButton", "favorite", "bookmark", "playlists"};
            const auto phase = std::make_shared<int>(0);
            const auto ticks = std::make_shared<int>(0);
            const auto oldFavorite = std::make_shared<bool>(false);
            const auto bookmarkDone = std::make_shared<bool>(false);
            const auto playlistId = std::make_shared<qint64>(0);
            QObject::connect(ctx, &MainCtx::redesignBookmarkAdded, root, [ctx, root, player, phase, bookmarkDone, results](qint64 time, bool success) {
                if (*phase != 7) return;
                results->insert("toolbarBookmarkApiSucceeded", success);
                results->insert("toolbarBookmarkTimeMS", time);
                if (!success) { results->insert("toolbarBookmarkWritesRealTimedRecord", false); *bookmarkDone = true; return; }
                const QString uri = player->getUrl().toString(QUrl::FullyEncoded);
                struct Bookmarks { bool exists = false; };
                ctx->getMediaLibrary()->runOnMLThread<Bookmarks>(root,
                    [uri, time](vlc_medialibrary_t *ml, Bookmarks &result) {
                        const QByteArray encoded = uri.toUtf8();
                        ml_unique_ptr<vlc_ml_media_t> media{vlc_ml_get_media_by_mrl(ml, encoded.constData())};
                        if (!media) return;
                        ml_unique_ptr<vlc_ml_bookmark_list_t> list{vlc_ml_list_media_bookmarks(ml, nullptr, media->i_id)};
                        if (list) for (size_t i = 0; i < list->i_nb_items; ++i)
                            if (list->p_items[i].i_time == time && list->p_items[i].psz_name && *list->p_items[i].psz_name) result.exists = true;
                        if (result.exists) vlc_ml_media_remove_bookmark(ml, media->i_id, time);
                    },
                    [bookmarkDone, results](quint64, Bookmarks &result) { results->insert("toolbarBookmarkWritesRealTimedRecord", result.exists); *bookmarkDone = true; },
                    "redesign-current-media");
            });
            auto *probe = new QTimer(root); probe->setInterval(750);
            const auto installGroup = [profiles](int group) {
                const int first = ControlListModel::REDESIGN_PIP + group * 4;
                std::array<QVector<int>, 3> layout{{{ControlListModel::PLAY_BUTTON}, {}, {first, first + 1, first + 2, first + 3}}};
                profiles->currentModel()->setModelData(PlayerControlbarModel::Videoplayer, layout);
                profiles->currentModel()->setModelData(PlayerControlbarModel::Audioplayer, layout);
            };
            installGroup(0);
            const auto savedLayout = profiles->currentModel()->getModelData(PlayerControlbarModel::Videoplayer);
            profiles->save();
            const bool reloaded = profiles->reload();
            results->insert("newToolbarControlsPersistThroughReload", reloaded
                && profiles->currentModel()->getModelData(PlayerControlbarModel::Videoplayer) == savedLayout);
            QObject::connect(probe, &QTimer::timeout, root, [ctx, root, player, profiles, originalProfile, names, phase, ticks, oldFavorite, bookmarkDone, playlistId, probe, results, warnings, screenshot, destination, installGroup] {
                const auto finish = [ctx, profiles, originalProfile, probe, results, warnings, destination] {
                    profiles->deleteSelectedProfile(); profiles->setSelectedProfile(originalProfile);
                    probe->stop(); results->insert("qmlWarnings", *warnings);
                    QFile file(QDir(destination).filePath("checks.json"));
                    if (file.open(QFile::WriteOnly)) file.write(QJsonDocument(*results).toJson());
                    libvlc_Quit(vlc_object_instance(ctx->getIntf()));
                };
                if (++*ticks > 85) { results->insert("controlsCompletedWithinLimit", false); finish(); return; }
                if (*phase < 4) {
                    for (int offset = 0; offset < 4; ++offset) {
                        const QString name = names[*phase * 4 + offset];
                        const QString object = name.endsWith("Button") ? name : "redesignControl-" + name;
                        results->insert("toolbarControl-" + name, findRedesignItem(root, object) != nullptr);
                    }
                    screenshot(QString("toolbar-group-%1").arg(*phase));
                    ++*phase;
                    if (*phase < 4) installGroup(*phase);
                    else ctx->refreshRedesignFavorite();
                    return;
                }
                if (ctx->redesignFavoriteBusy()) return;
                if (*phase == 4) {
                    if (!ctx->redesignFavoriteAvailable()) { ctx->refreshRedesignFavorite(); return; }
                    *oldFavorite = ctx->redesignIsFavorite();
                    if (auto *button = findRedesignItem(root, "redesignControl-favorite")) QMetaObject::invokeMethod(button, "clicked");
                    ++*phase; return;
                }
                if (*phase == 5) {
                    results->insert("toolbarFavoriteChangesNativeLibrary", ctx->redesignIsFavorite() != *oldFavorite);
                    ctx->refreshRedesignFavorite(); ++*phase; return;
                }
                if (*phase == 6) {
                    results->insert("toolbarFavoriteSurvivesNativeReload", ctx->redesignIsFavorite() != *oldFavorite);
                    ++*phase;
                    if (auto *button = findRedesignItem(root, "redesignControl-bookmark")) QMetaObject::invokeMethod(button, "clicked");
                    return;
                }
                if (*phase == 7) {
                    if (!*bookmarkDone) return;
                    ctx->toggleRedesignFavorite(); ++*phase; return;
                }
                if (*phase == 8) {
                    results->insert("toolbarFavoriteRestored", ctx->redesignIsFavorite() == *oldFavorite);
                    const QString path = QDir(destination).filePath("native-queue.m3u");
                    int exported;
                    { vlc::playlist::vlc_playlist_locker lock(ctx->getIntf()->p_playlist);
                      exported = vlc_playlist_Export(ctx->getIntf()->p_playlist, qtu(path), "export-m3u8"); }
                    QFile file(path);
                    results->insert("nativePlaylistExportPreserved", exported == VLC_SUCCESS && file.open(QFile::ReadOnly)
                        && file.readAll().contains(player->getUrl().toString(QUrl::FullyEncoded).toUtf8()));
                    const QString uri = player->getUrl().toString(QUrl::FullyEncoded);
                    struct Playlist { qint64 id = 0; };
                    ctx->getMediaLibrary()->runOnMLThread<Playlist>(root,
                        [uri](vlc_medialibrary_t *ml, Playlist &result) {
                            const QByteArray encoded = uri.toUtf8();
                            ml_unique_ptr<vlc_ml_media_t> media{vlc_ml_get_media_by_mrl(ml, encoded.constData())};
                            if (!media) return;
                            ml_unique_ptr<vlc_ml_playlist_t> playlist{vlc_ml_playlist_create(ml, "Native playlist name QA")};
                            if (!playlist) return;
                            const int64_t mediaId = media->i_id;
                            if (vlc_ml_playlist_append(ml, playlist->i_id, &mediaId, 1) == VLC_SUCCESS) result.id = playlist->i_id;
                        },
                        [ctx, playlistId, results](quint64, Playlist &result) {
                            *playlistId = result.id; results->insert("nativeSavedPlaylistCreated", result.id != 0);
                            if (result.id) ctx->getMediaLibrary()->addAndPlay(MLItemId(result.id, VLC_ML_PARENT_PLAYLIST));
                        });
                    ++*phase; return;
                }
                if (!*playlistId || ctx->redesignMediaLocation().value("playlist").toString() != "Native playlist name QA") return;
                results->insert("playlistNameUsesActualSavedPlaylist", true);
                const qint64 id = *playlistId;
                ctx->getMediaLibrary()->runOnMLThread(root, [id](vlc_medialibrary_t *ml) { vlc_ml_playlist_delete(ml, id); });
                results->insert("controlsCompletedWithinLimit", true); finish();
            });
            probe->start(); return;
        }
        player->setRate(1.25f);
        player->jumpToPos(.5);
        const auto allOptions = ctx->redesignConfigOptions();
        results->insert("registeredOptions", allOptions.size());
        const QVariant original = ctx->redesignConfigValue("volume-step");
        const bool unknownRejected = !ctx->saveRedesignConfig({{"volume-step", 5}, {"redesign-nonexistent", true}});
        results->insert("unknownDraftRejectedAtomically", unknownRejected && ctx->redesignConfigValue("volume-step") == original);
        results->insert("fractionalIntegerRejected", !ctx->saveRedesignConfig({{"file-caching", 1.5}}));
        results->insert("malformedBooleanRejected", !ctx->saveRedesignConfig({{"spu", QStringLiteral("true")}}));
        const bool saved = ctx->saveRedesignConfig({{"volume-step", 5}});
        results->insert("validSettingSaved", saved && ctx->redesignConfigValue("volume-step").toInt() == 5);
        results->insert("testSettingRestored", ctx->saveRedesignConfig({{"volume-step", original}}));

        const QString batchDirectory = QDir(destination).filePath("batch-fixtures");
        QDir().mkpath(batchDirectory);
        const auto createFixture = [batchDirectory](const QString &name, const QByteArray &data = {}) {
            QFile file(QDir(batchDirectory).filePath(name));
            if (file.open(QFile::WriteOnly)) file.write(data);
            return QUrl::fromLocalFile(file.fileName()).toString(QUrl::FullyEncoded);
        };
        const QString exactMedia = createFixture("Exact movie.mp4");
        const QString exactSubtitle = createFixture("Exact movie.en.srt", "1\n00:00:01,000 --> 00:00:02,500\n<b>First cue</b>\n\n2\n00:00:03,000 --> 00:00:02,000\nInvalid backwards cue\n\n3\n00:00:04,000 --> 00:00:05,000\nSecond cue\n");
        const QString assSubtitle = createFixture("Timeline.ass",
            "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
            "Dialogue: 0,0:00:04.20,0:00:05.50,Default,,0,0,0,,{\\i1}Second, with comma{\\i0}\\NNext line\n"
            "Comment: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,Not dialogue\n"
            "Dialogue: 0,0:00:01.10,0:00:02.20,Default,,0,0,0,,{\\p1}m 0 0 l 10 10{\\p0}First cue\n"
            "Dialogue: 0,0:00:03.00,0:00:02.00,Default,,0,0,0,,Backwards\n");
        const auto assCues = readRedesignSubtitleCues(QUrl(assSubtitle).toLocalFile());
        results->insert("subtitleAssTimelinePreservesDialogue", assCues.size() == 2
            && assCues[0].toMap().value("text") == "First cue"
            && assCues[0].toMap().value("start").toDouble() == 1.1
            && assCues[1].toMap().value("text") == "Second, with comma\nNext line");
        const QString vttSubtitle = createFixture("Timeline.vtt",
            "WEBVTT\n\n1\n00:04.200 --> 00:05.500 align:start\nSecond cue\n\n"
            "2\n00:01.100 --> 00:02.200\n<v Speaker>First cue</v>\n\n"
            "3\n00:61.000 --> 00:62.000\nInvalid timestamp\n");
        const auto vttCues = readRedesignSubtitleCues(QUrl(vttSubtitle).toLocalFile());
        results->insert("subtitleVttTimelineSortsValidCues", vttCues.size() == 2
            && vttCues[0].toMap().value("text") == "First cue"
            && vttCues[1].toMap().value("start").toDouble() == 4.2);
        createFixture("Show.Name.S01E02.1080p.mp4");
        createFixture("Show Name 1x02.en.srt");
        createFixture("Show.Name.S01E02.de.srt");
        createFixture("Other.Show.S01E02.en.srt");
        createFixture("Unmatched movie.mp4");
        const auto rows = ctx->previewRedesignSubtitles("folder", batchDirectory);
        bool single = false, ambiguous = false, unmatched = false, noCrossShow = true;
        for (const auto &value : rows) {
            const auto row = value.toMap();
            const auto candidates = row.value("candidates").toList();
            if (row.value("label").toString() == "Exact movie.mp4")
                single = candidates.size() == 1 && row.value("selected").toInt() == 0;
            if (row.value("label").toString().startsWith("Show.Name")) {
                ambiguous = candidates.size() == 2 && row.value("selected").toInt() == -1 && !row.value("enabled").toBool();
                for (const auto &candidate : candidates)
                    if (candidate.toMap().value("label").toString().startsWith("Other.Show")) noCrossShow = false;
            }
            if (row.value("label").toString() == "Unmatched movie.mp4")
                unmatched = candidates.isEmpty() && !row.value("enabled").toBool();
        }
        results->insert("subtitleSingleMatchSelected", single);
        results->insert("subtitleAmbiguousMatchRequiresChoice", ambiguous);
        results->insert("subtitleOtherShowExcluded", noCrossShow && ambiguous);
        results->insert("subtitleMissingMatchNotInvented", unmatched);
        const QVariant oldPairs = ctx->settingValue("Redesign/subtitle-files", QVariantMap{});
        const QVariantMap validPair{{"media", exactMedia}, {"subtitle", exactSubtitle}, {"enabled", true}};
        const QVariantMap invalidPair{{"media", exactMedia}, {"subtitle", "file:///missing-subtitle.srt"}, {"enabled", true}};
        results->insert("subtitleBatchRejectedAtomically", !ctx->saveRedesignSubtitlePairs({validPair, invalidPair})
            && ctx->settingValue("Redesign/subtitle-files", QVariantMap{}) == oldPairs);
        results->insert("subtitleBatchSaved", ctx->saveRedesignSubtitlePairs({validPair}));
        results->insert("subtitleSavedMatchRetained", ctx->previewRedesignSubtitles("folder", batchDirectory).value(0).toMap().value("selected").toInt() == 0);
        if (hasFixture) {
            const QVariantMap currentPair{{"media", player->getUrl().toString(QUrl::FullyEncoded)}, {"subtitle", exactSubtitle}, {"enabled", true}};
            const bool associated = ctx->saveRedesignSubtitlePairs({currentPair});
            const auto cues = ctx->redesignSubtitleCues();
            results->insert("subtitleTimelineReadsRealCues", associated && cues.size() == 2
                && cues[0].toMap().value("text") == "First cue" && cues[1].toMap().value("start").toDouble() == 4.0);
        }
        ctx->setSettingValue("Redesign/subtitle-files", oldPairs);

        QObject *settings = root->findChild<QObject *>("redesignSettings");
        results->insert("settingsPresent", settings != nullptr);
        if (settings) QMetaObject::invokeMethod(settings, "open");
        auto *timer = new QTimer(root);
        timer->setInterval(500);
        const auto stage = std::make_shared<int>(0);
        auto *history = engine->singletonInstance<NavigationHistory *>(qmlTypeId("VLC.MainInterface", 1, 0, "History"));
        const bool gridView = ctx->hasGridView();
        QObject::connect(timer, &QTimer::timeout, root, [ctx, root, history, gridView, settings, timer, stage, screenshot, destination, results, warnings] {
            if (settings && *stage < 13) {
                if (*stage == 0) {
                    results->insert("rateAfterSeek", ctx->getIntf()->p_mainPlayerController->getRate());
                    results->insert("positionAfterSeek", ctx->getIntf()->p_mainPlayerController->getPosition());
                }
                if (*stage == 3) {
                    auto *capture = qobject_cast<QQuickItem *>(settings->findChild<QObject *>("redesignShortcut-key-stop"));
                    const auto oldDraft = settings->property("draft");
                    if (capture) {
                        capture->forceActiveFocus(Qt::TabFocusReason);
                        capture->setProperty("capturing", true);
                        QKeyEvent key(QEvent::KeyPress, Qt::Key_F11, Qt::ControlModifier | Qt::ShiftModifier);
                        QCoreApplication::sendEvent(capture, &key);
                        results->insert("shortcutCaptureUsesNativeKeyFormat", settings->property("draft").toMap().value("key-stop")
                            == ctx->redesignHotkey(Qt::Key_F11, Qt::ControlModifier | Qt::ShiftModifier));
                        capture->setProperty("capturing", true);
                        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                        QCoreApplication::sendEvent(capture, &escape);
                        results->insert("shortcutEscapeCancelsCapture", !capture->property("capturing").toBool());
                        settings->setProperty("draft", oldDraft);
                    }
                }
                screenshot(QString("settings-%1").arg(*stage));
                ++*stage;
                if (*stage < 13) settings->setProperty("category", *stage);
                return;
            }
            if (settings && *stage == 13) QMetaObject::invokeMethod(settings, "close");
            if (*stage >= 13 && *stage < 17) {
                QObject *button = findRedesignItem(root, QStringLiteral("redesignTracksButton"));
                results->insert("livePanelButtonPresent", button != nullptr);
                if (button) {
                    QMetaObject::invokeMethod(button, "showPanel", Q_ARG(int, *stage - 13));
                    screenshot(QString("live-%1").arg(*stage - 13));
                }
                ++*stage;
                return;
            }
            if (*stage == 17) {
                auto *button = findRedesignItem(root, QStringLiteral("redesignTracksButton"));
                if (button) QMetaObject::invokeMethod(button, "showPanel", Q_ARG(int, 0));
                const auto state = ctx->redesignInterfaceState();
                ctx->saveRedesignConfig({{"volume-step", 7}}, false);
                results->insert("profileSettingsRestored", ctx->applyRedesignInterfaceState(state, false));
                results->insert("profileValueRestored", ctx->redesignConfigValue("volume-step") == state.value("options").toMap().value("volume-step"));
                auto *onboarding = root->findChild<QObject *>("redesignOnboarding");
                results->insert("onboardingPresent", onboarding != nullptr);
                if (onboarding) QMetaObject::invokeMethod(onboarding, "open");
                ++*stage;
                return;
            }
            if (*stage >= 18 && *stage < 22) {
                auto *onboarding = root->findChild<QObject *>("redesignOnboarding");
                screenshot(QString("onboarding-%1").arg(*stage-18));
                if (onboarding) {
                    if (*stage < 21) onboarding->setProperty("step", *stage-17);
                    else QMetaObject::invokeMethod(onboarding, "close");
                }
                ++*stage;
                return;
            }
            if (*stage == 22) {
                // closing the welcome shows the library, and the short fixture may have
                // ended by now; the importer belongs to the player, so play again first
                static int importWait = 0;
                if (!ctx->getIntf()->p_mainPlayerController->isStarted() || !ctx->isPlayerView()) {
                    if (importWait++ == 0) ctx->getIntf()->p_mainPlaylistController->play();
                    ctx->setPlayerView(true);
                    if (importWait < 10) return;
                }
                // users reach the importer from the open live panel ("Add subtitles")
                auto *button = findVisibleRedesignItem(root, QStringLiteral("redesignTracksButton"));
                if (button) QMetaObject::invokeMethod(button, "showPanel", Q_ARG(int, 3));
                const bool imported = button && QMetaObject::invokeMethod(button, "showSubtitleImport");
                results->insert("subtitleImporterPresent", imported);
                ++*stage;
                return;
            }
            if (*stage == 23) {
                auto *tracks = findVisibleRedesignItem(root, QStringLiteral("redesignTracksButton"));
                auto *panel = tracks ? redesignQmlObject(tracks->property("livePanel")) : nullptr;
                auto *importer = panel ? panel->findChild<QObject *>(QStringLiteral("redesignSubtitleImport")) : nullptr;
                results->insert("subtitleImporterShownOverPlayer", importer && importer->property("opened").toBool() && ctx->isPlayerView());
                screenshot("subtitle-import");
                auto *button = findVisibleRedesignItem(root, QStringLiteral("redesignTracksButton"));
                if (button) {
                    QMetaObject::invokeMethod(button, "closeSubtitleImport");
                    QMetaObject::invokeMethod(button, "closePanel");
                }
                ++*stage;
                return;
            }
            if (*stage == 24) {
                screenshot("player-final");
                ctx->setPlayerView(false);
                ++*stage;
                return;
            }
            if (*stage == 25) {
                screenshot("home");
                const auto folder = ctx->redesignFrequentFolders().value(0).toMap().value("path").toString();
                results->insert("frequentFolderTreeValid", !folder.isEmpty() && ctx->redesignFolderTree(folder).isValid());
                ++*stage;
                return;
            }
            if (*stage >= 26 && *stage < 33) {
                const QStringList pages {"playlists", "recent", "favorites", "video"};
                if (*stage >= 27 && *stage <= 30) {
                    const int page = *stage - 27;
                    screenshot("library-" + pages[page]);
                    if (page < 3) results->insert("library-" + pages[page] + "-loaded",
                        findRedesignItem(root, "redesign" + QStringList{"Playlists", "Recent", "Favorites"}[page] + "View") != nullptr);
                }
                if (history && *stage < 30) {
                    const int page = *stage - 26;
                    history->push(page == 3 ? QStringList{"video", "all"} : QStringList{pages[page]});
                }
                if (*stage == 30) ctx->setGridView(false);
                if (*stage == 31) { screenshot("library-video-list"); ctx->setGridView(gridView); }
                if (*stage == 32) DialogsProvider::getInstance()->extendedDialog();
                ++*stage;
                return;
            }
            if (*stage == 33) {
                bool captured = false;
                for (auto *widget : QApplication::topLevelWidgets()) {
                    if (widget->inherits("ExtendedDialog") && widget->isVisible()) {
                        captured = widget->grab().save(QDir(destination).filePath("native-effects.png"));
                        widget->close();
                    }
                }
                results->insert("nativeEffectsDialogRendered", captured);
                DialogsProvider::getInstance()->showToolbarEditorDialog();
                ++*stage;
                return;
            }
            if (*stage == 34) {
                bool captured = false;
                for (auto *window : QGuiApplication::allWindows()) {
                    if (window->objectName() != "redesignToolbarEditor") continue;
                    if (auto *quick = qobject_cast<QQuickWindow *>(window))
                        captured = quick->grabWindow().save(QDir(destination).filePath("customize.png"));
                    const auto originalState = ctx->redesignInterfaceState();
                    auto *profiles = ctx->controlbarProfileModel();
                    const int originalIndex = profiles->selectedProfile();
                    const int count = profiles->rowCount();
                    const QString originalId = profiles->currentModel()->property("redesignId").toString();
                    profiles->cloneSelectedProfile("Redesign QA profile");
                    profiles->setSelectedProfile(count);
                    auto *clone = profiles->currentModel();
                    const QString id = clone->property("redesignId").toString();
                    results->insert("clonedProfileHasUniqueIdentity", !id.isEmpty() && id != originalId);
                    clone->setName("Renamed QA profile");
                    ctx->saveRedesignConfig({{"volume-step", 7}}, false);
                    ctx->storeRedesignInterfaceProfile(id);
                    profiles->save(false);
                    profiles->reload();
                    results->insert("profileIdentitySurvivesReloadAndRename", profiles->getProfile(count)->property("redesignId").toString() == id
                        && profiles->getProfile(count)->name() == "Renamed QA profile");
                    ctx->saveRedesignConfig({{"volume-step", 8}}, false);
                    results->insert("profileSettingsSurviveReload", ctx->loadRedesignInterfaceProfile(id)
                        && ctx->redesignConfigValue("volume-step").toInt() == 7);
                    profiles->setSelectedProfile(count);
                    profiles->deleteSelectedProfile();
                    profiles->setSelectedProfile(originalIndex);
                    profiles->save(false);
                    ctx->applyRedesignInterfaceState(originalState, true);
                    QMetaObject::invokeMethod(window, "rejected", Q_ARG(bool, true));
                    results->insert("profileTestRestoredOriginal", profiles->rowCount() == count
                        && profiles->currentModel()->property("redesignId").toString() == originalId);
                    break;
                }
                results->insert("customizeDialogRendered", captured);
                ++*stage;
                return;
            }
            if (*stage == 35) {
                if (settings) {
                    QMetaObject::invokeMethod(settings, "openAtOption", Q_ARG(QVariant, QStringLiteral("key-stop")));
                }
                ++*stage; return;
            }
            if (*stage == 36) {
                auto *capture = root->window() ? findRedesignItem(root->window()->contentItem(), "redesignShortcut-key-stop") : nullptr;
                results->insert("shortcutCaptureFieldPresent", capture != nullptr);
                if (capture) {
                    capture->forceActiveFocus(Qt::TabFocusReason); capture->setProperty("capturing", true);
                    QKeyEvent key(QEvent::KeyPress, Qt::Key_F11, Qt::ControlModifier | Qt::ShiftModifier);
                    QCoreApplication::sendEvent(capture, &key);
                    const QString expected = ctx->redesignHotkey(Qt::Key_F11, Qt::ControlModifier | Qt::ShiftModifier);
                    results->insert("shortcutCaptureUsesNativeKeyFormat", redesignQmlMap(settings,"draft").value("key-stop").toString() == expected);
                    capture->setProperty("capturing", true);
                    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier); QCoreApplication::sendEvent(capture, &escape);
                    results->insert("shortcutEscapeCancelsCapture", !capture->property("capturing").toBool());
                    capture->setProperty("capturing", true);
                    QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier); QCoreApplication::sendEvent(capture, &space);
                    results->insert("shortcutConflictPromptsBeforeReplace", !redesignQmlMap(settings,"pendingShortcut").value("conflicts").toList().isEmpty()
                        && redesignQmlMap(settings,"draft").value("key-stop").toString() == expected);
                }
                ++*stage; return;
            }
            if (*stage == 37) {
                if (settings) {
                    screenshot("shortcut-conflict");
                    if (auto *button = settings->findChild<QObject *>("redesignShortcutReplace")) QMetaObject::invokeMethod(button,"clicked");
                    const auto draft = redesignQmlMap(settings,"draft");
                    const QString space = ctx->redesignHotkey(Qt::Key_Space,Qt::NoModifier);
                    results->insert("shortcutReplaceClearsOnlyConflictingAssignments", draft.value("key-stop").toString() == space
                        && !draft.value("key-play-pause").toString().split('\t').contains(space));
                    const QString original = ctx->redesignConfigValue("key-stop").toString();
                    QMetaObject::invokeMethod(settings,"close");
                    results->insert("shortcutCancelPreservesSavedAssignment", ctx->redesignConfigValue("key-stop").toString() == original
                        && redesignQmlMap(settings,"draft").isEmpty());
                }
                ++*stage; return;
            }
            if (!qEnvironmentVariableIsEmpty("VLC_REDESIGN_QA_SUBTITLE_API")) {
                if (*stage == 38) {
                    ctx->loginRedesignSubtitles("local-fixture-user", "local-fixture-password");
                    ++*stage; return;
                }
                if (*stage == 39) {
                    const auto state = ctx->redesignSubtitleDownloadState();
                    if (state.value("busy").toBool()) return;
                    results->insert("providerFixtureLogin", state.value("authenticated").toBool());
                    auto rows = ctx->previewRedesignSubtitles("folder", QDir(destination).filePath("batch-fixtures"));
                    while (rows.size() > 2) rows.removeLast();
                    for (auto &value : rows) { auto row = value.toMap(); row["enabled"] = true; value = row; }
                    ctx->searchRedesignSubtitles(rows, "en"); ++*stage; return;
                }
                if (*stage == 40) {
                    const auto state = ctx->redesignSubtitleDownloadState();
                    if (state.value("busy").toBool()) return;
                    const auto rows = state.value("rows").toList();
                    bool review = rows.size() == 2;
                    for (int index = 0; index < rows.size(); ++index) {
                        const auto row = rows[index].toMap();
                        review &= row.value("candidates").toList().size() == 1 && row.value("selected").toInt() == -1;
                        ctx->selectRedesignSubtitleDownload(index, 0, true);
                    }
                    results->insert("providerFixtureRequiresResultReview", review);
                    ctx->downloadRedesignSubtitles(); ++*stage; return;
                }
                if (*stage == 41) {
                    const auto state = ctx->redesignSubtitleDownloadState();
                    if (state.value("busy").toBool()) return;
                    bool downloaded = true; QSet<QString> paths;
                    for (const auto &value : state.value("rows").toList()) {
                        const auto choices = value.toMap().value("candidates").toList();
                        const QUrl url(choices.value(0).toMap().value("url").toString());
                        QFile file(url.toLocalFile());
                        downloaded &= url.isLocalFile() && file.open(QFile::ReadOnly) && file.readAll().contains("Local provider fixture");
                        paths.insert(url.toLocalFile());
                    }
                    results->insert("providerFixtureBatchDownloadsRealFiles", downloaded && paths.size() == 2);
                    const auto originalPairs = ctx->settingValue("Redesign/subtitle-files", QVariantMap{});
                    results->insert("providerFixtureBatchApplied", ctx->applyRedesignSubtitleDownloads());
                    ctx->setSettingValue("Redesign/subtitle-files", originalPairs);
                    ctx->logoutRedesignSubtitles();
                    results->insert("providerFixtureLogoutClearsSession", !ctx->redesignSubtitleDownloadState().value("authenticated").toBool());
                    ctx->searchRedesignSubtitles({}, "en");
                    results->insert("providerEmptySelectionRejected", !ctx->redesignSubtitleDownloadState().value("busy").toBool());
                    ++*stage; return;
                }
            }
            results->insert("seekPosition", ctx->getIntf()->p_mainPlayerController->getPosition());
            results->insert("playbackRate", ctx->getIntf()->p_mainPlayerController->getRate());
            results->insert("waveformBins", ctx->redesignWaveformPeaks().size());
            const auto peaks = ctx->redesignWaveformPeaks();
            double low = 1.0, high = 0.0;
            for (const auto &peak : peaks) { low = std::min(low, peak.toDouble()); high = std::max(high, peak.toDouble()); }
            results->insert("waveformHasVariation", peaks.size() == 1024 && high - low > .05);
            results->insert("frequentFolderCount", ctx->redesignFrequentFolders().size());
            ctx->setRedesignSleepTimer(1);
            results->insert("sleepTimerStarted", ctx->redesignSleepTimerActive());
            ctx->setRedesignSleepTimer(0);
            results->insert("sleepTimerCancelled", !ctx->redesignSleepTimerActive());
            results->insert("qmlWarnings", *warnings);
            QFile file(QDir(destination).filePath("checks.json"));
            if (file.open(QFile::WriteOnly)) file.write(QJsonDocument(*results).toJson());
            timer->stop();
            libvlc_Quit(vlc_object_instance(ctx->getIntf()));
        });
        timer->start();
    });
    ready->start();
}
