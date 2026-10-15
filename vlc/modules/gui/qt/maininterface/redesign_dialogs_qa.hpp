// SPDX-License-Identifier: GPL-2.0-or-later
// Opt-in dialog walk-through on the isolated QA desktop, never on the user's window.
// Opens every redesigned media dialog mode and the native dialogs and menus, saves
// a picture of each and records whether its background follows the active theme.
#pragma once
#include "menus/menus.hpp"
#include <QImage>
#include <QMenu>
#include <QPointer>
#include <QSet>
#include <vector>

inline double redesignMeanLightness(const QImage &source)
{
    if (source.isNull()) return -1;
    const QImage image = source.convertToFormat(QImage::Format_RGB32);
    double total = 0; int samples = 0;
    const int stepX = qMax(1, image.width() / 64), stepY = qMax(1, image.height() / 64);
    for (int y = 0; y < image.height(); y += stepY)
        for (int x = 0; x < image.width(); x += stepX) { total += QColor(image.pixel(x, y)).lightnessF(); ++samples; }
    return samples ? total / samples : -1;
}

inline void runRedesignDialogsQA(MainCtx *ctx, QQuickItem *root,
    const QString &destination, const std::shared_ptr<QJsonObject> &results,
    const std::shared_ptr<QJsonArray> &warnings, const std::function<void(QString)> &screenshot)
{
    struct Step { QString name; std::function<void()> open; bool widget; std::function<void()> close; };
    auto *dialogs = DialogsProvider::getInstance();
    const bool dark = ctx->getColorScheme()->currentScheme() != ColorSchemeModel::Day;
    auto *probe = new QTimer(root); probe->setInterval(700);
    const auto index = std::make_shared<int>(0);
    const auto opened = std::make_shared<bool>(false);
    const auto before = std::make_shared<QSet<QWidget *>>();
    const auto menu = std::make_shared<QPointer<QMenu>>();
    const auto steps = std::make_shared<std::vector<Step>>();
    { vlc_player_locker lock(ctx->getIntf()->p_player); vlc_player_Pause(ctx->getIntf()->p_player); }

    const auto closeMedia = [root] { if (auto *media = root->findChild<QObject *>("redesignMediaDialog")) QMetaObject::invokeMethod(media, "close"); };
    for (int mode = 0; mode < 6; ++mode)
        steps->push_back({QString("media-dialog-%1").arg(mode), [dialogs, mode] { emit dialogs->redesignOpenMediaRequested(mode); }, false, closeMedia});
    const auto hideNew = [before] {
        for (auto *widget : QApplication::topLevelWidgets())
            if (widget->isVisible() && !before->contains(widget)) widget->hide();
    };
    steps->push_back({"native-about", [dialogs] { dialogs->aboutDialog(); }, true, hideNew});
    steps->push_back({"native-media-info", [dialogs] { dialogs->mediaInfoDialog(); }, true, hideNew});
    steps->push_back({"native-extensions", [dialogs] { dialogs->pluginDialog(); }, true, hideNew});
    steps->push_back({"native-help", [dialogs] { dialogs->helpDialog(); }, true, hideNew});
    steps->push_back({"native-messages", [dialogs] { dialogs->messagesDialog(); }, true, hideNew});
    steps->push_back({"native-goto-time", [dialogs] { dialogs->gotoTimeDialog(); }, true, hideNew});
    steps->push_back({"native-effects", [dialogs] { dialogs->extendedDialog(); }, true, hideNew});
    steps->push_back({"menu-audio", [ctx, menu] {
        *menu = VLCMenuBar::AudioPopupMenu(ctx->getIntf(), false);
        if (*menu) (*menu)->popup(QPoint(400, 300));
    }, true, [menu] { if (*menu) { (*menu)->hide(); (*menu)->deleteLater(); } }});
    const auto tracksPanel = [root](bool show) {
        if (auto *tracks = findVisibleRedesignItem(root, QStringLiteral("redesignTracksButton"))) {
            if (show) QMetaObject::invokeMethod(tracks, "showPanel", Q_ARG(int, 2));
            else QMetaObject::invokeMethod(tracks, "closePanel");
        }
    };
    steps->push_back({"live-audio", [tracksPanel] { tracksPanel(true); }, false, [] {}});
    steps->push_back({"eq-preset-open", [root] {
        auto *tracks = findVisibleRedesignItem(root, QStringLiteral("redesignTracksButton"));
        if (auto *live = tracks ? redesignQmlObject(tracks->property("livePanel")) : nullptr)
            if (auto *select = live->findChild<QObject *>("redesignPresetSelect"))
                if (auto *popup = redesignQmlObject(select->property("popup"))) QMetaObject::invokeMethod(popup, "open");
    }, false, [tracksPanel] { tracksPanel(false); }});

    QObject::connect(probe, &QTimer::timeout, root, [=] {
        if (*index >= int(steps->size())) {
            probe->stop();
            results->insert("dialogStepsCompleted", int(steps->size()));
            results->insert("qmlWarnings", *warnings);
            QFile file(QDir(destination).filePath("checks.json"));
            if (file.open(QFile::WriteOnly)) file.write(QJsonDocument(*results).toJson());
            libvlc_Quit(vlc_object_instance(ctx->getIntf()));
            return;
        }
        const Step &step = steps->at(*index);
        if (!*opened) {
            before->clear();
            for (auto *widget : QApplication::topLevelWidgets()) if (widget->isVisible()) before->insert(widget);
            step.open();
            *opened = true;
            return;
        }
        QImage image;
        if (step.widget) {
            QWidget *shown = nullptr;
            for (auto *widget : QApplication::topLevelWidgets())
                if (widget->isVisible() && !before->contains(widget)) { shown = widget; break; }
            results->insert(step.name + "-shown", shown != nullptr);
            if (shown) {
                image = shown->grab().toImage();
                results->insert(step.name, image.save(QDir(destination).filePath(step.name + ".png")));
            }
        } else {
            screenshot(step.name);
            if (auto *window = root->window()) image = window->grabWindow();
            if (step.name.startsWith("media-dialog") || step.name == "live-audio" || step.name == "eq-preset-open") {
                auto *tracks = findVisibleRedesignItem(root, QStringLiteral("redesignTracksButton"));
                auto *media = step.name.startsWith("media-dialog") ? root->findChild<QObject *>("redesignMediaDialog")
                    : tracks ? redesignQmlObject(tracks->property("livePanel")) : nullptr;
                results->insert(step.name + "-shown", media && media->property("opened").toBool());
                if (step.name == "eq-preset-open") {
                    auto *select = media ? media->findChild<QObject *>("redesignPresetSelect") : nullptr;
                    auto *popup = select ? redesignQmlObject(select->property("popup")) : nullptr;
                    results->insert("eq-preset-list-opened", popup && popup->property("opened").toBool());
                }
                if (media && !image.isNull()) {
                    const QRect area(media->property("x").toInt(), media->property("y").toInt(),
                                     media->property("width").toInt(), media->property("height").toInt());
                    image = image.copy(area);
                }
            }
        }
        const double lightness = redesignMeanLightness(image);
        results->insert(step.name + "-lightness", lightness);
        if (lightness >= 0) results->insert(step.name + "-matchesTheme", dark ? lightness < .45 : lightness > .55);
        step.close();
        *opened = false;
        ++*index;
    });
    probe->start();
}
