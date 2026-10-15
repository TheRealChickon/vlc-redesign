// SPDX-License-Identifier: GPL-2.0-or-later
// Opt-in checks on the isolated QA desktop, never on the user's window.
#pragma once
#include "medialibrary/mlbookmarkmodel.hpp"
#include "menus/menus.hpp"
#include <QMenu>
#include <QSet>

inline void runRedesignParityQA(MainCtx *ctx, QQuickItem *root,
    const QString &destination, const std::shared_ptr<QJsonObject> &results,
    const std::shared_ptr<QJsonArray> &warnings, const std::function<void(QString)> &screenshot)
{
    auto *player = ctx->getIntf()->p_mainPlayerController;
    auto *probe = new QTimer(root); probe->setInterval(800);
    const auto phase = std::make_shared<int>(0);
    const auto ticks = std::make_shared<int>(0);
    const auto bookmarkTime = std::make_shared<qint64>(-1);
    const auto basicKeys = std::make_shared<QSet<QString>>();
    const auto queueCount = std::make_shared<int>(0);
    const auto queueWaits = std::make_shared<int>(0);
    const QString originalShortcut = ctx->redesignConfigValue("key-stop").toString();
    { vlc_player_locker lock(ctx->getIntf()->p_player);
      vlc_player_Pause(ctx->getIntf()->p_player);
      vlc_player_SetPosition(ctx->getIntf()->p_player, .25); }
    QObject::connect(ctx, &MainCtx::redesignBookmarkAdded, root,
        [bookmarkTime, results](qint64 time, bool success) {
            results->insert("instantBookmarkApiSucceeded", success);
            if (success) *bookmarkTime = time;
        });
    const auto qmlRows = [](QObject *settings) {
        QVariant value;
        if (settings) QMetaObject::invokeMethod(settings,"rows",Q_RETURN_ARG(QVariant,value));
        if (value.canConvert<QJSValue>()) value = value.value<QJSValue>().toVariant();
        return value.toList();
    };
    QObject::connect(probe,&QTimer::timeout,root,
        [=] {
        auto *settings = root->findChild<QObject *>("redesignSettings");
        auto *sidebar = root->findChild<QObject *>("redesignLibraryPanel");
        auto *bookmarks = sidebar ? sidebar->findChild<MLBookmarkModel *>("redesignSidebarBookmarksModel") : nullptr;
        auto *content = root->window()->contentItem();
        const auto finish = [=] {
            probe->stop(); results->insert("qmlWarnings",*warnings);
            QFile file(QDir(destination).filePath("checks.json"));
            if (file.open(QFile::WriteOnly)) file.write(QJsonDocument(*results).toJson());
            libvlc_Quit(vlc_object_instance(ctx->getIntf()));
        };
        if (++*ticks > 65) { results->insert("parityCompletedWithinLimit",false); finish(); return; }
        if (*phase == 0) {
            if (std::abs(player->getPosition()-.25)>.03 && *ticks<20) return;
            results->insert("bookmarkActivationTimeMS",MS_FROM_VLC_TICK(player->getTime().toVLCTick()));
            // The private desktop does not become the system's input desktop.
            // Verify Qt's binding and dispatch its real activation signal here.
            auto *shortcut=root->findChild<QObject *>("redesignInstantBookmarkShortcut");
            results->insert("ctrlBShortcutRegisteredAndEnabled",shortcut && shortcut->property("enabled").toBool()
                && shortcut->property("sequence").value<QKeySequence>()==QKeySequence("Ctrl+B"));
            if(shortcut)QMetaObject::invokeMethod(shortcut,"activated");
            ++*phase; return;
        }
        if (*phase == 1) {
            if (*bookmarkTime < 0 && *ticks < 8) return;
            results->insert("ctrlBCreatesBookmarkWithoutDialog",*bookmarkTime >= 0);
            results->insert("bookmarkTimeMS",*bookmarkTime);
            results->insert("bookmarkCapturesCurrentTimestamp",std::abs(*bookmarkTime-results->value("bookmarkActivationTimeMS").toInteger())<500);
            bool dialog = false;
            for (auto *widget: QApplication::topLevelWidgets())
                if (widget->inherits("BookmarksDialog") && widget->isVisible()) dialog = true;
            results->insert("ctrlBDoesNotOpenClassicBookmarkDialog",!dialog);
            results->insert("bookmarkDoesNotResumePausedPlayback",player->getPlayingState()==PlayerController::PLAYING_STATE_PAUSED);
            ctx->askShowRedesignQueue(2); ++*phase; return;
        }
        if (*phase == 2) {
            if (bookmarks && bookmarks->rowCount()==0 && *ticks<15) return;
            results->insert("bookmarkSidebarOpens",sidebar && sidebar->property("opened").toBool() && sidebar->property("section").toInt()==2);
            results->insert("bookmarkSidebarUsesSavedNativeRecord",bookmarks && bookmarks->rowCount()>0);
            screenshot("bookmarks");
            if (bookmarks && bookmarks->rowCount()>0) bookmarks->renameAt(0,"Example moment");
            ++*phase; return;
        }
        if (*phase == 3) {
            results->insert("bookmarkRenameUpdatesNativeModel",bookmarks && bookmarks->rowCount()>0 && bookmarks->data(bookmarks->index(0,0),MLBookmarkModel::NameRole).toString()=="Example moment");
            { vlc_player_locker lock(ctx->getIntf()->p_player); vlc_player_SetPosition(ctx->getIntf()->p_player,.6); }
            ++*phase; return;
        }
        if (*phase == 4) {
            if (auto *seek = findVisibleRedesignItem(content,"redesignSidebarBookmarkSeek-0")) QMetaObject::invokeMethod(seek,"clicked");
            ++*phase; return;
        }
        if (*phase == 5) {
            results->insert("bookmarkSidebarSeeksNativePlayer",std::abs(player->getPosition()-.25)<.03);
            if (auto *remove = findVisibleRedesignItem(content,"redesignSidebarBookmarkRemove-0")) QMetaObject::invokeMethod(remove,"clicked");
            ++*phase; return;
        }
        if (*phase == 6) {
            results->insert("bookmarkRemoveUpdatesNativeModel",bookmarks && bookmarks->rowCount()==0);
            auto *queueButton=findVisibleRedesignItem(root,"playqueueButton");
            results->insert("bottomRightQueueButtonPresent",queueButton!=nullptr);
            if(queueButton)QMetaObject::invokeMethod(queueButton,"clicked");
            ++*phase; return;
        }
        if (*phase == 7) {
            results->insert("queueTabUsesOriginalNativePlaylistPane",findVisibleRedesignItem(content,"redesignQueuePane")!=nullptr);
            results->insert("queueTabRemainsInsidePlayer",ctx->isPlayerView() && ctx->m_redesignSidebarVisible);
            screenshot("queue");
            if (auto *tab=findVisibleRedesignItem(content,"redesignSidebarSection-0")) QMetaObject::invokeMethod(tab,"clicked");
            ++*phase; return;
        }
        if (*phase == 8) {
            results->insert("queueCanSwitchToVideosLibrary",sidebar && sidebar->property("section").toInt()==0 && sidebar->property("currentTab").toInt()==0);
            *queueCount=ctx->getIntf()->p_mainPlaylistController->count();
            auto *add=findVisibleRedesignItem(content,"redesignLibraryQueue-0");
            results->insert("videosSidebarQueueActionPresent",add!=nullptr);
            if (add) QMetaObject::invokeMethod(add,"clicked");
            screenshot("videos-sidebar"); ++*phase; return;
        }
        if (*phase == 9) {
            // MediaLib resolves input items on its worker thread.
            if(ctx->getIntf()->p_mainPlaylistController->count()<=*queueCount && ++*queueWaits<8)return;
            results->insert("videosSidebarAddsToOriginalQueue",ctx->getIntf()->p_mainPlaylistController->count()>*queueCount);
            results->insert("sidebarOperationsKeepPausedPlayback",player->getPlayingState()==PlayerController::PLAYING_STATE_PAUSED);
            if (sidebar) sidebar->setProperty("section",3);
            ++*phase; return;
        }
        if (*phase == 10) {
            auto *chapters=findVisibleRedesignItem(content,"redesignSidebarChapterEntries");
            results->insert("chaptersTabUsesNativeChapterModel",chapters && chapters->property("count").toInt()==player->getChapters()->rowCount());
            const bool required=qEnvironmentVariable("VLC_REDESIGN_QA_MEDIA").contains("-chapters");
            if(required) {
                results->insert("realFileChaptersAvailable",chapters && chapters->property("count").toInt()==2);
                if(auto *chapter=findVisibleRedesignItem(content,"redesignSidebarChapterSeek-1"))QMetaObject::invokeMethod(chapter,"clicked");
            }
            screenshot("chapters");
            if (sidebar) QMetaObject::invokeMethod(sidebar,"close");
            DialogsProvider::getInstance()->prefsDialog(); ++*phase; return;
        }
        if (*phase == 11) {
            if(qEnvironmentVariable("VLC_REDESIGN_QA_MEDIA").contains("-chapters"))results->insert("chapterSelectionSeeksNativePlayer",std::abs(player->getPosition()-.5)<.03);
            results->insert("preferencesRouteUsesRedesignedSettings",settings && settings->property("opened").toBool());
            if (settings) settings->setProperty("category",4);
            ++*phase; return;
        }
        if (*phase == 12) {
            for (const auto &row:qmlRows(settings)) { const auto key=row.toMap().value("key").toString(); if (!key.isEmpty()) basicKeys->insert(key); }
            results->insert("basicPlaybackSettingsExist",basicKeys->size()>=3);
            if (auto *toggle=findRedesignItem(content,"redesignAdvancedOptionsToggle")) QMetaObject::invokeMethod(toggle,"clicked");
            ++*phase; return;
        }
        if (*phase == 13) {
            QSet<QString> expanded; bool unique=true;
            for (const auto &row:qmlRows(settings)) {const auto key=row.toMap().value("key").toString();if (!key.isEmpty()){if(expanded.contains(key))unique=false;expanded.insert(key);}}
            results->insert("advancedOptionsRetainBasicRows",!basicKeys->isEmpty() && expanded.contains(*basicKeys));
            results->insert("advancedOptionsAppendMoreNativeSettings",expanded.size()>basicKeys->size());
            results->insert("advancedOptionsDoNotDuplicateBasicRows",unique);
            screenshot("settings-advanced");
            if (auto *choice=findVisibleRedesignItem(content,"redesignSettingChoice")) {
                auto *popup=choice->property("popup").value<QObject *>();
                if (popup) QMetaObject::invokeMethod(popup,"open");
                results->insert("settingsDropdownOpens",popup && popup->property("visible").toBool());
                screenshot("settings-dropdown");
                if (popup) QMetaObject::invokeMethod(popup,"close");
            }
            if (settings) settings->setProperty("advanced",false);
            ++*phase; return;
        }
        if (*phase == 14) {
            QQuickItem *info=nullptr;
            for(const auto &key:*basicKeys) {info=findVisibleRedesignItem(content,"redesignSettingInfo-"+key);if(info)break;}
            results->insert("settingDescriptionInfoButtonPresent",info!=nullptr);
            if(info)QMetaObject::invokeMethod(info,"clicked");
            ++*phase; return;
        }
        if (*phase == 15) {
            auto *info=settings?settings->findChild<QObject *>("redesignOptionInfo"):nullptr;
            results->insert("settingInfoShowsActualDescription",info && info->property("opened").toBool() && !redesignQmlMap(settings,"infoOption").value("description").toString().isEmpty());
            screenshot("settings-info");
            QVariantMap longest;
            for(const auto &value:ctx->redesignConfigOptions())if(value.toMap().value("description").toString().size()>longest.value("description").toString().size())longest=value.toMap();
            if(settings)settings->setProperty("infoOption",longest);
            root->window()->resize(960,420);
            *phase=18; return;
        }
        if (*phase == 18) {
            auto *info=settings?settings->findChild<QObject *>("redesignOptionInfo"):nullptr;
            results->insert("longNativeDescriptionPopupFitsSettings",info && info->property("y").toDouble()>=0
                && info->property("height").toDouble()<=settings->property("height").toDouble()-16);
            auto *scroll=findVisibleRedesignItem(content,"redesignInfoScroll");
            results->insert("longNativeDescriptionCanScroll",scroll && scroll->property("contentHeight").toDouble()>scroll->height());
            screenshot("settings-long-info");
            if(settings)QMetaObject::invokeMethod(settings,"close");
            root->window()->resize(1440,1024);
            QMenu menu;
            VLCMenuBar::ToolsMenu(ctx->getIntf(),&menu);
            bool found=false;
            for(auto *action:menu.actions())if(action->text().remove('&').contains("Keyboard Shortcuts")){action->trigger();found=true;break;}
            results->insert("toolsMenuHasSeparateShortcutEditor",found);
            *phase=16; return;
        }
        if (*phase == 16) {
            results->insert("toolsMenuOpensDedicatedShortcutView",settings && settings->property("opened").toBool() && settings->property("shortcutsOnly").toBool());
            const auto rows=qmlRows(settings);bool onlyHotkeys=!rows.isEmpty();
            for(const auto &row:rows)if(row.toMap().contains("key")&&!row.toMap().value("hotkey").toBool())onlyHotkeys=false;
            results->insert("shortcutEditorContainsOnlyNativeHotkeys",onlyHotkeys);
            bool labeled=true;
            for(const auto &row:rows)if(row.toMap().value("label").toString().trimmed().isEmpty())labeled=false;
            results->insert("shortcutEditorHasNoUnlabeledRows",labeled);
            screenshot("keyboard-shortcuts");
            auto *capture=findVisibleRedesignItem(content,"redesignShortcut-key-stop");
            results->insert("separateShortcutCaptureFieldPresent",capture!=nullptr);
            if(capture){capture->setProperty("capturing",true);QKeyEvent event(QEvent::KeyPress,Qt::Key_F11,Qt::ControlModifier|Qt::ShiftModifier);QCoreApplication::sendEvent(capture,&event);}
            ++*phase; return;
        }
        if (*phase == 17) {
            results->insert("separateShortcutEditorUpdatesDraft",redesignQmlMap(settings,"draft").value("key-stop").toString()==ctx->redesignHotkey(Qt::Key_F11,Qt::ControlModifier|Qt::ShiftModifier));
            if(settings)QMetaObject::invokeMethod(settings,"close");
            results->insert("shortcutCancelPreservesConfiguration",ctx->redesignConfigValue("key-stop").toString()==originalShortcut);
            results->insert("closedSidebarClearsButtonState",!ctx->m_redesignSidebarVisible);
            results->insert("parityCompletedWithinLimit",true);
            finish(); return;
        }
    });
    probe->start();
}
