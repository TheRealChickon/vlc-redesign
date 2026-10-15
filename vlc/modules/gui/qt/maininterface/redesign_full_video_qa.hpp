// SPDX-License-Identifier: GPL-2.0-or-later
// Explicitly enabled validation, on the task-owned isolated desktop only.
#pragma once
#include <QRegularExpression>
#include <QQmlComponent>
#include "medialibrary/mlbookmarkmodel.hpp"

inline void runRedesignFullVideoQA(MainCtx *ctx,QQuickItem *root,QQmlEngine *engine,
    const QString &destination,const std::shared_ptr<QJsonObject> &results,
    const std::shared_ptr<QJsonArray> &warnings,const std::function<void(QString)> &screenshot)
{
    auto *player=ctx->getIntf()->p_mainPlayerController;
    auto *probe=new QTimer(root);probe->setInterval(600);
    const auto phase=std::make_shared<int>(0),ticks=std::make_shared<int>(0),category=std::make_shared<int>(0);
    const auto reverseStart=std::make_shared<qint64>(0),decodedStart=std::make_shared<qint64>(0);
    const auto cueStart=std::make_shared<double>(0);
    const auto allKeys=std::make_shared<QSet<QString>>();
    const auto bookmarkButton=std::make_shared<QPointer<QObject>>();
    const auto queueBefore=std::make_shared<int>(0);
    const QRect originalGeometry=root->window()->geometry();
    const auto toVariant=[](QVariant v){return v.canConvert<QJSValue>()?v.value<QJSValue>().toVariant():v;};
    const auto call=[toVariant](QObject *o,const char *method){QVariant v;if(o)QMetaObject::invokeMethod(o,method,Q_RETURN_ARG(QVariant,v));return toVariant(v);};
    QObject::connect(probe,&QTimer::timeout,root,[=]{
        auto *settings=root->findChild<QObject*>("redesignSettings");
        QObject *live=nullptr;
        auto *tracks=findRedesignItem(root,"redesignTracksButton");
        if(tracks){const auto panel=tracks->property("livePanel");live=panel.canConvert<QJSValue>()?panel.value<QJSValue>().toQObject():panel.value<QObject*>();}
        auto *right=root->findChild<QObject*>("redesignLibraryPanel");
        auto *folder=root->findChild<QObject*>("redesignFolderPanel");
        auto *media=root->findChild<QObject*>("redesignMediaDialog");
        auto *content=root->window()->contentItem();
        const auto finish=[=]{probe->stop();results->insert("qmlWarnings",*warnings);QFile file(QDir(destination).filePath("checks.json"));if(file.open(QFile::WriteOnly))file.write(QJsonDocument(*results).toJson());libvlc_Quit(vlc_object_instance(ctx->getIntf()));};
        if(++*ticks>110){results->insert("fullVideoCompletedWithinLimit",false);finish();return;}
        switch(*phase){
        case 0:{
            results->insert("nativeConfigRegistryStillComplete",ctx->redesignConfigOptions().size()>1000);
            QQmlComponent component(engine,QUrl("qrc:/qt/qml/VLC/PlayerControls/RedesignActionControl.qml"));
            auto *button=component.createWithInitialProperties({{"controlKind","bookmark"},{"controlLabel","Add bookmark"},{"objectName","fullVideoBookmarkButton"}});
            results->insert("bookmarkControlComponentLoads",button!=nullptr);
            if(button){button->setParent(root);if(auto *item=qobject_cast<QQuickItem*>(button)){item->setParentItem(content);item->setOpacity(0);}*bookmarkButton=button;}
            {vlc_player_locker lock(ctx->getIntf()->p_player);vlc_player_Pause(ctx->getIntf()->p_player);}
            player->setPosition(.3);
            QFile subtitles(QDir(destination).filePath("full-video-cues.srt"));
            if(subtitles.open(QFile::WriteOnly)){subtitles.write("1\n00:00:02,000 --> 00:00:06,000\nFirst actual cue\n\n2\n00:00:10,000 --> 00:00:18,000\nSecond actual cue\n");subtitles.close();ctx->saveRedesignSubtitlePairs({QVariantMap{{"media",player->getUrl().toString(QUrl::FullyEncoded)},{"subtitle",QUrl::fromLocalFile(subtitles.fileName()).toString(QUrl::FullyEncoded)},{"enabled",true}}});}

            ++*phase;return;
        }
        case 1:
            results->insert("bookmarkEnabledIndependentlyOfFavoriteState",*bookmarkButton && (*bookmarkButton)->property("enabled").toBool());
            results->insert("bookmarkFixtureIndexState",ctx->redesignFavoriteAvailable()?"indexed":"not indexed");
            results->insert("bookmarkFixtureInitiallyUnindexed",!ctx->redesignFavoriteAvailable());
            ctx->notifyRedesignShortcut("Ctrl+B","Add bookmark");ctx->addRedesignBookmark();ctx->askShowRedesignQueue(2);++*phase;return;
        case 2:{
            auto *bookmarks=right?right->findChild<MLBookmarkModel*>("redesignSidebarBookmarksModel"):nullptr;
            if((!bookmarks||bookmarks->rowCount()==0)&&*ticks<12)return;
            const QString time=bookmarks&&bookmarks->rowCount()?bookmarks->data(bookmarks->index(0,0),MLBookmarkModel::TimeRole).toString():QString{};
            results->insert("bookmarkSidebarDisplaysMilliseconds",QRegularExpression("^\\d{2,}:\\d{2}:\\d{2}\\.\\d{3}$").match(time).hasMatch());
            results->insert("bookmarkTimeDisplay",time);screenshot("bookmarks-precise");
            if(right)QMetaObject::invokeMethod(right,"close");
            QMetaObject::invokeMethod(settings,"open");settings->setProperty("category",1);++*phase;return;
        }
        case 3:{
            const auto rows=call(settings,"rows").toList();QSet<QString> keys;for(const auto &r:rows)keys.insert(r.toMap().value("key").toString());
            results->insert("currentGeometryAndNotificationSettingsPresent",keys.contains("qt-redesign-remember-geometry")&&keys.contains("qt-notification"));
            settings->setProperty("category",4);const auto playback=call(settings,"rows").toList();bool resume=false;for(const auto &r:playback)resume|=r.toMap().value("key")=="restore-playback-pos";results->insert("resumeUsesRegisteredNativeOption",resume);
            settings->setProperty("category",7);const auto subtitles=call(settings,"rows").toList();bool matching=false,font=false,opacity=false;
            for(const auto &r:subtitles){const auto o=r.toMap();if(o.value("key")=="sub-autodetect-fuzzy")matching=o.value("choices").toList().size()==5;if(o.value("key")=="freetype-fontsize")font=o.value("editor")=="fontSize";if(o.value("key")=="freetype-opacity")opacity=o.value("editor")=="range"&&std::abs(o.value("factor").toDouble()-2.55)<.001;}
            results->insert("matchingRulesHaveFiveReadableChoices",matching);results->insert("globalFontUsesRelativePixelsEditor",font);results->insert("opacityUsesPercentageConversion",opacity);
            screenshot("settings-subtitles");*category=0;settings->setProperty("advanced",true);++*phase;return;
        }
        case 4:{
            settings->setProperty("category",*category);settings->setProperty("advanced",true);
            for(const auto &r:call(settings,"rows").toList()){const auto o=r.toMap();if(o.contains("key"))allKeys->insert(o.value("key").toString());}
            screenshot("settings-category-"+QString::number(*category));
            if(++*category<13)return;
            bool complete=true;QStringList missing;
            for(const auto &raw:ctx->redesignConfigOptions()){const auto o=raw.toMap();if(o.value("hotkey").toBool())continue;if(!allKeys->contains(o.value("key").toString())){complete=false;missing.append(o.value("key").toString());}}
            results->insert("advancedCategoriesRetainEveryNativeOption",complete);results->insert("missingNativeOptions",QJsonArray::fromStringList(missing));
            QMetaObject::invokeMethod(settings,"close");
            results->insert("liveControlsUseActualToolbarPanel",tracks&&live);
            if(tracks)QMetaObject::invokeMethod(tracks,"showPanel",Q_ARG(int,3));
            ++*phase;return;
        }
        case 5:{
            results->insert("subtitlePresetInputValuesAccepted",ctx->applyRedesignLiveOptions({{"freetype-bold",true},{"freetype-shadow-distance",.25},{"freetype-background-opacity",96}}));
            player->setSecondarySubtitleDelayMS(375);player->setSubtitleFPS(23.976f);
            if(auto *name=live?live->findChild<QObject*>("redesignPresetName"):nullptr)name->setProperty("text","Full video QA");
            QMetaObject::invokeMethod(live,"savePreset");
            const auto stored=ctx->settingValue("Redesign/live-presets-3","{}").toString();
            const auto saved=QJsonDocument::fromJson(stored.toUtf8()).object().value("Full video QA").toObject();
            results->insert("subtitlePresetIncludesPreviouslyMissingValues",saved.contains("freetype-bold")&&saved.contains("freetype-shadow-distance")&&saved.contains("freetype-background-opacity")&&saved.contains("_secondarySubtitleDelay")&&saved.contains("_subtitleFPS"));
            ctx->applyRedesignLiveOptions({{"freetype-bold",false},{"freetype-shadow-distance",.05},{"freetype-background-opacity",0}});player->setSecondarySubtitleDelayMS(0);player->setSubtitleFPS(25);
            QMetaObject::invokeMethod(live,"applyPreset",Q_ARG(QVariant,QVariant("Full video QA")));++*phase;return;
        }
        case 6:
            results->insert("subtitlePresetRestoreDetail",QJsonObject{{"bold",ctx->redesignLiveValue("freetype-bold").toBool()},{"backgroundOpacity",ctx->redesignLiveValue("freetype-background-opacity").toInt()},{"secondaryDelayMs",player->getSecondarySubtitleDelayMS()},{"fps",player->getSubtitleFPS()},{"status",live?live->property("status").toString():"missing panel"}});
            results->insert("subtitlePresetRestoresFormattingAndTiming",ctx->redesignLiveValue("freetype-bold").toBool()&&ctx->redesignLiveValue("freetype-background-opacity").toInt()==96&&std::abs(ctx->redesignLiveValue("freetype-shadow-distance").toDouble()-.25)<.001&&player->getSecondarySubtitleDelayMS()==375&&std::abs(player->getSubtitleFPS()-23.976f)<.01);
            if(live){live->setProperty("currentTab",1);const auto snapshot=call(live,"snapshot").toMap();results->insert("videoPresetIncludesFramingAndDeinterlace",snapshot.contains("_aspect")&&snapshot.contains("_crop")&&snapshot.contains("_zoom")&&snapshot.contains("_deinterlace")&&snapshot.contains("_deinterlaceMode"));screenshot("video-presets");live->setProperty("currentTab",2);const auto audio=call(live,"snapshot").toMap();results->insert("audioPresetIncludesStereoAndDevice",audio.contains("_stereoMode")&&audio.contains("_audioDevice"));screenshot("audio-devices");QMetaObject::invokeMethod(live,"close");}
            ctx->askShowRedesignFolder();ctx->askShowRedesignQueue(1);++*phase;return;
        case 7:
            results->insert("folderAndQueueRemainOpenTogether",folder&&right&&folder->property("opened").toBool()&&right->property("opened").toBool());
            results->insert("folderStartsAtCurrentMediaFolder",folder&&folder->property("folder").toString()==ctx->redesignCurrentFolder());
            results->insert("currentFolderHasActualFileEntries",folder&&!toVariant(folder->property("entries")).toList().isEmpty());
            *queueBefore=ctx->getIntf()->p_mainPlaylistController->count();
            ctx->getIntf()->p_mainPlaylistController->append(QVariantList{player->getUrl(),player->getUrl()},false);screenshot("folder-and-queue");++*phase;return;
        case 8:
            results->insert("addingMultipleItemsPreservesPlayingMedia",ctx->getIntf()->p_mainPlaylistController->count()==*queueBefore+2&&player->getPlayingState()==PlayerController::PLAYING_STATE_PAUSED);
            if(folder)QMetaObject::invokeMethod(folder,"close");if(right)QMetaObject::invokeMethod(right,"close");
            QMetaObject::invokeMethod(media,"openMode",Q_ARG(QVariant,QVariant(1)));++*phase;return;
        case 9:{
            auto *urls=media?media->findChild<QObject*>("redesignNetworkUrls"):nullptr;auto *name=media?media->findChild<QObject*>("redesignNetworkPlaylistName"):nullptr;
            results->insert("networkDialogHasUrlsAndPlaylistName",urls&&name&&media->property("opened").toBool());
            if(urls)urls->setProperty("text","https://example.invalid/one\nhttps://example.invalid/two");
            results->insert("networkDialogParsesMultipleUrls",call(media,"items").toList().size()==2);screenshot("network-stream-dialog");QMetaObject::invokeMethod(media,"close");
            ctx->applyRedesignLiveOptions({{"qt-redesign-show-cues",true},{"qt-redesign-show-metadata",true},{"qt-redesign-show-waveform",false}});ctx->requestRedesignSubtitleTimeline();++*phase;return;
        }
        case 10:{
            if(ctx->redesignTimelineBusy()&&*ticks<60)return;
            auto *cue=findVisibleRedesignItem(content,"redesignSubtitleCueBlock");
            results->insert("cueDetail",QJsonObject{{"count",ctx->redesignTimelineCues().size()},{"message",ctx->redesignTimelineMessage()},{"visibleBlock",cue!=nullptr}});
            results->insert("horizontalCueLayerHasRealParsedCues",cue&&ctx->redesignTimelineCues().size()>0);
            results->insert("metadataToggleDisplaysOverlay",findVisibleRedesignItem(content,"redesignMetadataOverlay")!=nullptr);
            results->insert("waveformTogglePersistsNativeSetting",!ctx->redesignPresentation().value("waveform").toBool());
            if(cue){*cueStart=cue->x();QMetaObject::invokeMethod(cue,"moveCue",Q_ARG(QVariant,QVariant(2000)));}
            ++*phase;return;
        }
        case 11:{
            auto *cue=findVisibleRedesignItem(content,"redesignSubtitleCueBlock");
            results->insert("movingCueChangesLiveDelayAndGeometry",cue&&player->getSubtitleDelayMS()==2000&&cue->x()>*cueStart);
            screenshot("subtitle-timeline");
            ctx->notifyRedesignShortcut("C","Crop");++*phase;return;
        }
        case 12:{
            auto *notice=findVisibleRedesignItem(content,"redesignBookmarkNotice");
            bool text=false;if(notice)for(auto *item:notice->findChildren<QObject*>())if(item->property("text").toString().contains("Crop")&&item->property("text").toString().contains("C"))text=true;
            results->insert("feedbackDisplaysActionAndActualKey",text);screenshot("shortcut-feedback");
            ctx->applyRedesignLiveOptions({{"qt-redesign-show-cues",false},{"qt-redesign-show-metadata",false}});
            player->setSubtitleFPS(0);player->setSubtitleDelayMS(0);player->setPosition(.7);++*phase;return;
        }
        case 13:
            results->insert("reverseStartHasVideo",player->hasVideoOutput());
            results->insert("reverseStartSeekable",player->isSeekable());
            results->insert("reverseStartTimeUs",player->getTime().toVLCTick());
            *reverseStart=player->getTime().toVLCTick();*decodedStart=results->value("decodedVideoFrames").toInteger();player->reverse();++*phase;return;
        case 14:
            results->insert("reverseDuringTimeUs",player->getTime().toVLCTick());
            results->insert("reversePreviewStartsForSeekableVideo",player->reversePlaybackActive());++*phase;return;
        case 15:
            results->insert("reverseActuallyDecreasesPlaybackTime",player->getTime().toVLCTick()<*reverseStart-VLC_TICK_FROM_MS(200));
            results->insert("reverseDecodesNewVideoFrames",results->value("decodedVideoFrames").toInteger()>*decodedStart);
            player->stopRedesignReverse();ctx->setRedesignPopout(true);++*phase;return;
        case 16:
            results->insert("floatingVideoUsesNativeWindowAndSurface",ctx->property("redesignPopoutActive").toBool()&&ctx->isMinimalView()&&ctx->isInterfaceAlwaysOnTop()&&root->window()->width()<=410);
            results->insert("floatingVideoHasRestoreControl",findVisibleRedesignItem(content,"redesignPopoutRestore")!=nullptr);screenshot("floating-video");ctx->setRedesignPopout(false);++*phase;return;
        case 17:
            results->insert("floatingVideoRestoresWindowGeometry",root->window()->geometry()==originalGeometry);
            results->insert("floatingVideoRestoresMainPlayer",!ctx->isMinimalView()&&ctx->isPlayerView());
            ctx->setPinVideoControls(false);
            if(auto *fsm=root->findChild<QObject*>("redesignToolbarVisibility")){fsm->setProperty("pointerAtEdge",true);QMetaObject::invokeMethod(fsm,"mouseMove");}
            ++*phase;return;
        case 18:
            if(auto *fsm=root->findChild<QObject*>("redesignToolbarVisibility")){
                results->insert("edgeHoverShowsControls",fsm->property("isVisible").toBool());fsm->setProperty("pointerAtEdge",false);QMetaObject::invokeMethod(fsm,"pointerLeave");
            }
            ++*phase;return;
        case 19:
            if(auto *fsm=root->findChild<QObject*>("redesignToolbarVisibility"))results->insert("leavingEdgeHidesControls",!fsm->property("isVisible").toBool());
            ctx->setPinVideoControls(true);
            ctx->sendHotkey(Qt::Key_C,Qt::NoModifier);
            ++*phase;return;
        case 20:{
            char *message=var_GetString(vlc_object_instance(ctx->getIntf()),"redesign-last-osd-message");
            const QString text=QString::fromUtf8(message?message:"");free(message);
            results->insert("nativeOsdPreservesActionAndAddsActualKey",text.contains("Crop",Qt::CaseInsensitive)&&text.contains("[c]",Qt::CaseInsensitive));
            results->insert("nativeOsdText",text);
            results->insert("fullVideoCompletedWithinLimit",true);screenshot("player-completed");finish();return;
        }
        }
    });probe->start();
}
