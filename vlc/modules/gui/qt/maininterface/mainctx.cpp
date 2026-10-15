/*****************************************************************************
 * main_interface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2011 VideoLAN and AUTHORS
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include <QQmlEngine>
#include <QJSEngine>

#include "mainctx.hpp"
#include "redesign_waveform.hpp"
#include "redesign_subtitles.hpp"
#include "redesign_subtitle_timeline.hpp"
#include "redesign_seek_preview.hpp"
#include "mainctx_submodels.hpp"
#include "medialibrary/mlhelper.hpp"
#include "medialibrary/medialib.hpp"
#include "network/networkmediamodel.hpp"

#include "compositor.hpp"
#include "util/renderer_manager.hpp"
#include "util/csdbuttonmodel.hpp"

#include "util/vlchotkeyconverter.hpp"
#include "util/qt_dirs.hpp"                     // toNativeSeparators

#include "util/color_scheme_model.hpp"
#include "style/redesign_widgets.hpp"

#include "widgets/native/interface_widgets.hpp"     // bgWidget, videoWidget

#include "playlist/playlist_controller.hpp"
#include "player/player_controller.hpp"

#include "dialogs/dialogs_provider.hpp"
#include "dialogs/systray/systray.hpp"

#include "videosurface.hpp"

#include "menus/menus.hpp"                            // Menu creation

#include "dialogs/toolbar/controlbar_profile_model.hpp"
#include "dialogs/help/help.hpp"

#include <QKeyEvent>

#include <QUrl>
#include <QDate>
#include <QMimeData>
#include <QClipboard>
#include <QInputDialog>
#include <QDir>
#include <QSaveFile>
#include <QLocale>
#include <QRegularExpression>
#include <QFileDialog>
#include <QColorDialog>
#include <QFileInfo>
#include <QSet>
#include <QKeySequence>
#include <QProcess>
#include <QCryptographicHash>

#include <QQmlProperty>

#include <QWindow>
#include <QScreen>

#include <QOperatingSystemVersion>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QStyleHints>
#endif

#ifdef _WIN32
#include <QFileInfo>
#endif

#include <vlc_interface.h>
#include <vlc_preparser.h>
#include <vlc_config_cat.h>
#include <vlc_modules.h>
#include <cmath>

#define VLC_REFERENCE_SCALE_FACTOR 96.

using  namespace vlc::playlist;

// #define DEBUG_INTF

/* Callback prototypes */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfBossCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfRaiseMainCB( vlc_object_t *p_this, const char *psz_variable,
                           vlc_value_t old_val, vlc_value_t new_val,
                           void *param );

const QEvent::Type MainCtx::ToolbarsNeedRebuild =
        (QEvent::Type)QEvent::registerEventType();

namespace
{

template <typename T>
T loadVLCOption(vlc_object_t *obj, const char *name);

template <>
int loadVLCOption<int>(vlc_object_t *obj, const char *name)
{
    return var_InheritInteger(obj, name);
}

template <>
float loadVLCOption<float>(vlc_object_t *obj, const char *name)
{
    return var_InheritFloat(obj, name);
}

template <>
bool loadVLCOption<bool>(vlc_object_t *obj, const char *name)
{
    return var_InheritBool(obj, name);
}

}

MainCtx::MainCtx(qt_intf_t *_p_intf)
    : p_intf(_p_intf)
    , m_csdButtonModel {std::make_unique<CSDButtonModel>(this, this)}
{
    // If `playlist-autostart` is set, and there are initial files
    // we don't bother loading main display just to unload it
    // immediately after. Instead, we directly load the player
    // page:
    if (var_InheritBool(p_intf, "playlist-autostart"))
    {
        // We can not use `vlc_playlist_GetCurrentIndex()`, because
        // playlist starts playing after the interface is added.
        // We also can not use the playlist controller because it
        // is not updated yet.
        vlc_playlist_locker lock(p_intf->p_playlist);

        if (vlc_playlist_Count(p_intf->p_playlist) > 0)
        {
            m_mainInterfaceModes |= MAININTERFACE_MODE_PLAYER;
        }
    }

    /**
     *  Configuration and settings
     *  Pre-building of interface
     **/

    settings = getSettings();
    connect(p_intf->p_mainPlayerController, &PlayerController::inputChanged, this, [this] {
        cancelRedesignSeekPreview();
        cancelRedesignSubtitleTimeline();
        m_redesignTimelineCues.clear(); m_redesignTimelineMessage.clear(); emit redesignTimelineChanged();
        ++m_redesignFavoriteRevision;
        m_redesignIsFavorite = false; m_redesignFavoriteAvailable = false; m_redesignFavoriteBusy = false;
        emit redesignFavoriteChanged();
        QTimer::singleShot(250, this, [this] { refreshRedesignFavorite(); });
    });
    m_redesignSleepTimer.setSingleShot(true);
    connect(&m_redesignSleepTimer, &QTimer::timeout, this, [this] {
        p_intf->p_mainPlaylistController->stop();
        emit redesignSleepTimerChanged();
    });
    connect(p_intf->p_mainPlayerController, &PlayerController::playingStateChanged, this,
        [this](PlayerController::PlayingState state) {
            if (state != PlayerController::PLAYING_STATE_PLAYING) return;
            const QString media = p_intf->p_mainPlayerController->getUrl().toString(QUrl::FullyEncoded);
            if (media == m_redesignAssociatedMedia) return;
            m_redesignAssociatedMedia = media;
            const QUrl mediaUrl(media);
            if (mediaUrl.isLocalFile() && QFileInfo(mediaUrl.toLocalFile()).isFile()
                && var_InheritBool(p_intf, "save-recentplay")
                && [this,&mediaUrl]{const auto words=redesignConfigValue("qt-redesign-history-filter").toString().split('|',Qt::SkipEmptyParts);for(const auto &word:words)if(!word.trimmed().isEmpty()&&mediaUrl.toString().contains(word.trimmed(),Qt::CaseInsensitive))return false;return true;}()) {
                const QString directory = QFileInfo(mediaUrl.toLocalFile()).absolutePath();
                auto folders = settings->value(QStringLiteral("Redesign/frequent-folders")).toMap();
                auto entry = folders.value(directory).toMap();
                entry["count"] = std::min(1000000, entry.value("count").toInt() + 1);
                entry["lastPlayed"] = QDateTime::currentMSecsSinceEpoch();
                folders[directory] = entry;
                if (folders.size() > 100) {
                    auto oldest = folders.begin();
                    for (auto i = folders.begin(); i != folders.end(); ++i)
                        if (i.value().toMap().value("lastPlayed").toLongLong() < oldest.value().toMap().value("lastPlayed").toLongLong()) oldest = i;
                    folders.erase(oldest);
                }
                settings->setValue(QStringLiteral("Redesign/frequent-folders"), folders);
                QTimer::singleShot(0, this, [this] { emit redesignFrequentFoldersChanged(); });
            }
            const auto pairs = settings->value(QStringLiteral("Redesign/subtitle-files")).toMap();
            const QString subtitle = pairs.value(media).toString();
            if (subtitle.isEmpty() || !QFileInfo(QUrl(subtitle).toLocalFile()).isFile()) return;
            QTimer::singleShot(250, this, [this, media, subtitle] {
                if (p_intf->p_mainPlayerController->getUrl().toString(QUrl::FullyEncoded) == media)
                    p_intf->p_mainPlayerController->associateSubtitleFile(subtitle);
            });
        }, Qt::QueuedConnection);
    m_colorScheme = new ColorSchemeModel(this);

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(m_colorScheme, &ColorSchemeModel::currentChanged, qGuiApp, [colorScheme = m_colorScheme]() {
        QStyleHints *const styleHints = qGuiApp->styleHints();
        if (unlikely(!styleHints))
            return;

        Qt::ColorScheme scheme;
        switch (colorScheme->currentScheme())
        {
        case ColorSchemeModel::ColorScheme::Day:
            scheme = Qt::ColorScheme::Light;
            break;
        case ColorSchemeModel::ColorScheme::Night:
            scheme = Qt::ColorScheme::Dark;
            break;
        case ColorSchemeModel::ColorScheme::System:
        default:
            styleHints->unsetColorScheme();
            return;
        }

        styleHints->setColorScheme(scheme);
    });
#endif

    m_sort = new SortCtx(this);
    m_search = new SearchCtx(this);
    m_playqueuePanel = new SidePanelCtx(this);
    m_navigationPanel = new SidePanelCtx(this);

    // getOSInfo();
    QOperatingSystemVersion currentOS = QOperatingSystemVersion::current();
    switch (currentOS.type()) {
    case QOperatingSystemVersion::OSType::Windows:
        m_osName = Windows;
        break;

    default:
        m_osName = Unknown;
        break;
    }
    m_osVersion = (currentOS.majorVersion());

    loadPrefs(false);
    loadFromSettingsImpl(false);
    applyRedesignWidgetStyle(this);
    connect(m_colorScheme, &ColorSchemeModel::currentChanged, this,
        [this] { applyRedesignWidgetStyle(this); }, Qt::QueuedConnection);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qGuiApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_colorScheme->currentScheme() == ColorSchemeModel::System) applyRedesignWidgetStyle(this);
    });
#endif

    /* Get the available interfaces */
    m_extraInterfaces = new VLCVarChoiceModel(VLC_OBJECT(p_intf->intf), "intf-add", this);

    vlc_medialibrary_t* ml = vlc_ml_instance_get( p_intf );
    b_hasMedialibrary = (ml != NULL);
    if (b_hasMedialibrary) {
        m_medialib = new MediaLib(p_intf, p_intf->p_mainPlaylistController);
    }

    /* Controlbar Profile Model Creation */
    m_controlbarProfileModel = new ControlbarProfileModel(p_intf->mainSettings, this);

    const QUrl homeUrl = QUrl::fromLocalFile(QVLCUserDir( VLC_HOME_DIR ));
    m_dialogFilepath = getSettings()->value( "filedialog-path", homeUrl ).toUrl();

    QString platformName = QGuiApplication::platformName();

    b_hasWayland = platformName.startsWith(QLatin1String("wayland"), Qt::CaseInsensitive);

    /*********************************
     * Create the Systray Management *
     *********************************/
    //postpone systray initialisation to speedup starting time
    QMetaObject::invokeMethod(this, &MainCtx::initSystray, Qt::QueuedConnection);

    /*************************************************************
     * Connect the input manager to the GUI elements it manages  *
     * Beware initSystray did some connects on input manager too *
     *************************************************************/
    /**
     * Connects on nameChanged()
     * Those connects are different because options can impeach them to trigger.
     **/
    /* Main Interface statusbar */
    /* and title of the Main Interface*/
    connect( THEMIM, &PlayerController::inputChanged, this, &MainCtx::onInputChanged );
    connect( THEMIM, &PlayerController::hasVideoOutputChanged, this, &MainCtx::onVOutChanged );

    /* END CONNECTS ON IM */

    /* VideoWidget connects for asynchronous calls */
    connect( this, &MainCtx::askToQuit, THEDP, &DialogsProvider::quit, Qt::QueuedConnection  );

    /** END of CONNECTS**/


    /************
     * Callbacks
     ************/
    libvlc_int_t* libvlc = vlc_object_instance(p_intf);
    var_AddCallback( libvlc, "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_AddCallback( libvlc, "intf-boss", IntfBossCB, p_intf );
    var_AddCallback( libvlc, "intf-show", IntfRaiseMainCB, p_intf );

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    if( var_InheritBool( p_intf, "qt-privacy-ask") )
    {
        //postpone dialog call, as composition might not be ready yet
        QMetaObject::invokeMethod(this, [](){
            THEDP->firstRunDialog();
        }, Qt::QueuedConnection);
    }
    else if (m_medialib)
    {
        QMetaObject::invokeMethod(m_medialib, &MediaLib::reload, Qt::QueuedConnection);
    }

    const struct vlc_preparser_cfg cfg = []{
        struct vlc_preparser_cfg cfg{};
        cfg.types = VLC_PREPARSER_TYPE_PARSE;
        cfg.max_parser_threads = 1;
        cfg.timeout = 0;
        cfg.external_process = false;
        return cfg;
    }();
    m_network_preparser = vlc_preparser_New(VLC_OBJECT(libvlc), &cfg);

#ifdef UPDATE_CHECK
    /* Checking for VLC updates */
    if( var_InheritBool( p_intf, "qt-updates-notif" ) &&
        !var_InheritBool( p_intf, "qt-privacy-ask" ) )
    {
        int interval = var_InheritInteger( p_intf, "qt-updates-days" );
        if( QDate::currentDate() >
            getSettings()->value( "updatedate" ).toDate().addDays( interval ) )
        {
            /* check for update at startup */
            getUpdateModel()->checkUpdate();
            getSettings()->setValue( "updatedate", QDate::currentDate() );
        }
    }
#endif

    m_threadRunner = new ThreadRunner();

    m_initialEffectiveMainInterfaceMode = getEffectiveMainInterfaceMode();
}

MainCtx::~MainCtx()
{
    m_redesignSubtitles.reset();
    m_redesignTimeline.reset();
    m_redesignSeekPreview.reset();
    m_redesignWaveform.reset();
    /* Save states */

    m_threadRunner->destroy();

    if ( !p_intf->preferencesResetPending)
    {
        settings->beginGroup("MainWindow");
        settings->setValue( "ShowRemainingTime", m_showRemainingTime );
        settings->setValue( "interface-scale", QString::number( m_intfUserScaleFactor ) );

        /* Save playlist state */
        settings->setValue( "playqueue-panel-docked", m_playqueuePanel->m_docked );
        settings->setValue( "playqueue-panel-visible", m_playqueuePanel->m_visible );
        settings->setValue( "playqueue-panel-width", QString::number( m_playqueuePanel->m_width ) );

        settings->setValue( "navigation-panel-visible", m_navigationPanel->m_visible );
        settings->setValue( "navigation-panel-width", QString::number( m_navigationPanel->m_width ) );

        settings->setValue( "player-playlist-width", QString::number( m_playerPlaylistWidth ) );
        settings->setValue( "artist-albums-width", QString::number( m_artistAlbumsWidth ) );

        settings->setValue( "grid-view", m_gridView );
        settings->setValue( "grouping", m_grouping );

        settings->setValue( "color-scheme-index", m_colorScheme->currentIndex() );

        settings->setValue( "album-sections", m_albumSections );

        settings->setValue( "lyrics-mode", m_lyricsMode );

        /* Save the stackCentralW sizes */
        settings->endGroup();
    }
    else
    {
        settings->remove("MainWindow");
    }

    if( !p_intf->preferencesResetPending && var_InheritBool( p_intf, "save-recentplay" ) )
        getSettings()->setValue( "filedialog-path", m_dialogFilepath );
    else
        getSettings()->remove( "filedialog-path" );

    /* Unregister callbacks */
    libvlc_int_t* libvlc = vlc_object_instance(p_intf);
    var_DelCallback( libvlc, "intf-boss", IntfBossCB, p_intf );
    var_DelCallback( libvlc, "intf-show", IntfRaiseMainCB, p_intf );
    var_DelCallback( libvlc, "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_DelCallback( libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    if (m_medialib)
        delete m_medialib;

    if (m_network_preparser)
        vlc_preparser_Delete(m_network_preparser);

    p_intf->p_mi = NULL;
}

bool MainCtx::hasVLM() const {
#ifdef ENABLE_VLM
    return true;
#else
    return false;
#endif
}

bool MainCtx::useClientSideDecoration() const
{
    //don't show CSD when interface is fullscreen
    return !m_windowTitlebar;
}

bool MainCtx::hasFirstrun() const {
    return var_InheritBool(p_intf,  "qt-privacy-ask" );
}

void MainCtx::setUseGlobalShortcuts( bool useShortcuts )
{
    if (m_useGlobalShortcuts == useShortcuts)
        return;
    m_useGlobalShortcuts = useShortcuts;
    emit useGlobalShortcutsChanged(m_useGlobalShortcuts);
}

void MainCtx::setWindowSuportExtendedFrame(bool support) {
    if (m_windowSuportExtendedFrame == support)
        return;
    m_windowSuportExtendedFrame = support;
    emit windowSuportExtendedFrameChanged();
}

void MainCtx::setWindowExtendedMargin(unsigned int margin) {
    if (margin == m_windowExtendedMargin)
        return;
    m_windowExtendedMargin = margin;
    emit windowExtendedMarginChanged(margin);
}

/*****************************
 *   Main UI handling        *
 *****************************/

void MainCtx::loadPrefs(const bool callSignals)
{
    const auto loadFromVLCOption = [this, callSignals](auto &variable, const char *name
            , const std::function<void(MainCtx *)> signal)
    {
        using variableType = std::remove_reference_t<decltype(variable)>;

        const auto value =  loadVLCOption<variableType>(VLC_OBJECT(p_intf), name);
        if (value == variable)
            return;

        variable = value;
        if (callSignals && signal)
            signal(this);
    };

    bool minimalMode = false;
    /* Are we in the enhanced always-video mode or not ? */
    loadFromVLCOption(minimalMode, "qt-minimal-view", nullptr);
    setMinimalView(minimalMode);

    loadFromVLCOption(m_bgCone, "qt-bgcone", &MainCtx::bgConeToggled);

    /* Should the UI stays on top of other windows */
    loadFromVLCOption(b_interfaceOnTop, "video-on-top", [this](MainCtx *)
    {
        emit interfaceAlwaysOnTopChanged(b_interfaceOnTop);
    });

    loadFromVLCOption(m_hasToolbarMenu, "qt-menubar", &MainCtx::hasToolbarMenuChanged);

    loadFromVLCOption(m_windowTitlebar, "qt-titlebar" , &MainCtx::useClientSideDecorationChanged);

    loadFromVLCOption(m_smoothScroll, "qt-smooth-scrolling", &MainCtx::smoothScrollChanged);

    loadFromVLCOption(m_maxVolume, "qt-max-volume", &MainCtx::maxVolumeChanged);

    loadFromVLCOption(m_pinVideoControls, "qt-pin-controls", &MainCtx::pinVideoControlsChanged);

    loadFromVLCOption(m_pinOpacity, "qt-fs-opacity", &MainCtx::pinOpacityChanged);

    loadFromVLCOption(m_safeArea, "qt-safe-area", &MainCtx::safeAreaChanged);

    loadFromVLCOption(m_mouseHideTimeout, "mouse-hide-timeout", &MainCtx::mouseHideTimeoutChanged);
}

void MainCtx::loadFromSettingsImpl(const bool callSignals)
{
    const auto loadFromSettings = [this, callSignals](auto &variable, const char *name
            , const auto defaultValue, auto obj,  auto signal)
    {
        using variableType = std::remove_reference_t<decltype(variable)>;

        const auto value = getSettings()->value(name, defaultValue).template value<variableType>();
        if (value == variable)
            return;

        variable = value;
        if (obj && callSignals && signal)
            (obj->*signal)(variable);
    };

    loadFromSettings(m_playqueuePanel->m_docked, "MainWindow/playqueue-panel-docked", true, m_playqueuePanel, &SidePanelCtx::dockedChanged);
    loadFromSettings(m_playqueuePanel->m_visible, "MainWindow/playqueue-panel-visible", false, m_playqueuePanel, &SidePanelCtx::visibleChanged);
    loadFromSettings(m_playqueuePanel->m_width, "MainWindow/playqueue-panel-width", 120, m_playqueuePanel, &SidePanelCtx::widthChanged);

    loadFromSettings(m_navigationPanel->m_visible, "MainWindow/navigation-panel-visible", true, m_navigationPanel, &SidePanelCtx::visibleChanged);
    loadFromSettings(m_navigationPanel->m_width, "MainWindow/navigation-panel-width", 120, m_navigationPanel, &SidePanelCtx::widthChanged);

    loadFromSettings(m_playerPlaylistWidth, "MainWindow/player-playlist-width", 120, this, &MainCtx::playerPlaylistWidthChanged);
    loadFromSettings(m_artistAlbumsWidth, "MainWindow/artist-albums-width", 120, this,  &MainCtx::artistAlbumsWidthChanged);

    loadFromSettings(m_gridView, "MainWindow/grid-view", true, this, &MainCtx::gridViewChanged);

    loadFromSettings(m_grouping, "MainWindow/grouping", GROUPING_NONE, this, &MainCtx::groupingChanged);

    loadFromSettings(m_showRemainingTime, "MainWindow/ShowRemainingTime", false, this, &MainCtx::showRemainingTimeChanged);

    loadFromSettings(m_albumSections, "MainWindow/album-sections", true, this, &MainCtx::albumSectionsChanged);

    loadFromSettings(m_lyricsMode, "MainWindow/lyrics-mode", true, this, &MainCtx::lyricsModeChanged);

    const auto colorSchemeIndex = getSettings()->value( "MainWindow/color-scheme-index", 2 ).toInt();
    m_colorScheme->setCurrentIndex(colorSchemeIndex);

    /* user interface scale factor */
    auto userIntfScaleFactor = var_InheritFloat(p_intf, "qt-interface-scale");
    if (userIntfScaleFactor == -1)
        userIntfScaleFactor = getSettings()->value( "MainWindow/interface-scale", 1.0).toDouble();
    if (m_intfUserScaleFactor != userIntfScaleFactor)
    {
        m_intfUserScaleFactor = userIntfScaleFactor;
        updateIntfScaleFactor();
    }
}

void MainCtx::reloadPrefs()
{
    loadPrefs(true);
}

void MainCtx::onInputChanged( bool hasInput )
{
    m_redesignAssociatedMedia.clear();
    if( hasInput == false )
        return;
    int autoRaise = var_InheritInteger( p_intf, "qt-auto-raise" );
    if ( autoRaise == MainCtx::RAISE_NEVER )
        return;
    if( THEMIM->hasVideoOutput() == true )
    {
        if( ( autoRaise & MainCtx::RAISE_VIDEO ) == 0 )
            return;
    }
    else if ( ( autoRaise & MainCtx::RAISE_AUDIO ) == 0 )
        return;
    emit askRaise();
}

void MainCtx::onVOutChanged( bool video )
{
    int autoRaise = var_InheritInteger( p_intf, "qt-auto-raise" );
    if ( video && ( autoRaise & MainCtx::RAISE_VIDEO ) )
        emit askRaise();
}

#ifdef KeyPress
#undef KeyPress
#endif
void MainCtx::sendHotkey(Qt::Key key , Qt::KeyboardModifiers modifiers)
{
    p_intf->p_mainPlayerController->stopRedesignReverse();
    QKeyEvent event(QEvent::KeyPress, key, modifiers );
    int vlckey = qtEventToVLCKey(&event);
    var_SetInteger(vlc_object_instance(p_intf), "key-pressed", vlckey);
    // Native OSD appends the triggering key; custom GUI actions use notifyRedesignShortcut().
}

QVariantMap MainCtx::redesignPresentation() const
{
    return {{"waveform", redesignConfigValue("qt-redesign-show-waveform")},
            {"cues", redesignConfigValue("qt-redesign-show-cues")},
            {"metadata", redesignConfigValue("qt-redesign-show-metadata")},
            {"preview", redesignConfigValue("qt-redesign-preview")},
            {"autoHide", redesignConfigValue("qt-redesign-auto-hide")},
            {"feedbackDuration", redesignConfigValue("qt-redesign-feedback-duration")}};
}

void MainCtx::notifyRedesignShortcut(const QString &key, const QString &action)
{
    if (!redesignConfigValue("qt-redesign-key-feedback").toBool() || !redesignConfigValue("osd").toBool()) return;
    QString title = action;
    if (title.isEmpty()) {
        const QKeySequence pressed(key);
        for (const auto &entry : redesignConfigOptions()) {
            const auto option = entry.toMap();
            if (!option.value("hotkey").toBool() || option.value("key").toString().startsWith("global-")) continue;
            for (const auto &binding : option.value("value").toString().split('\t'))
                if (QKeySequence(binding) == pressed) { title = option.value("label").toString(); break; }
            if (!title.isEmpty()) break;
        }
    }
    if (!key.isEmpty()) emit redesignShortcutPressed(key, title);
}

QString MainCtx::redesignCurrentFolder() const
{
    const QUrl url = p_intf->p_mainPlayerController->getUrl();
    return url.isLocalFile() ? QFileInfo(url.toLocalFile()).absolutePath() : QString{};
}

QVariantList MainCtx::redesignBrowseFolder(const QString &path) const
{
    QDir directory(path);
    if (path.isEmpty() || !directory.exists()) return {};
    QVariantList result;
    const QSet<QString> extensions{"mp4","mkv","avi","mov","webm","m4v","mpeg","mpg","ts","m2ts","ogv","wmv","flv","vob","mp3","flac","ogg","opus","m4a","aac","wav","aiff","wma","m3u","m3u8","xspf","pls"};
    for (const auto &entry : directory.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase)) {
        if (!entry.isDir() && !extensions.contains(entry.suffix().toLower())) continue;
        result.append(QVariantMap{{"name",entry.fileName()},{"path",entry.absoluteFilePath()},
            {"url",QUrl::fromLocalFile(entry.absoluteFilePath()).toString(QUrl::FullyEncoded)},
            {"folder",entry.isDir()},{"bytes",entry.size()}});
    }
    return result;
}

void MainCtx::setRedesignPopout(bool active)
{
    auto *window=intfMainWindow();
    if(!window||active==m_redesignPopoutActive)return;
    if(active){
        if(!p_intf->p_mainPlayerController->hasVideoOutput())return;
        m_redesignPopoutGeometry=window->geometry();
        m_redesignPopoutVisibility=window->visibility();
        m_redesignPopoutWasMinimal=isMinimalView();m_redesignPopoutWasPlayer=isPlayerView();m_redesignPopoutWasOnTop=isInterfaceAlwaysOnTop();
        m_redesignPopoutActive=true;
        window->showNormal();setPlayerView(true);setMinimalView(true);setInterfaceAlwaysOnTop(true);
        QTimer::singleShot(0,this,[this,window]{if(m_redesignPopoutActive)window->resize(400,250);});
    }else{
        m_redesignPopoutActive=false;setMinimalView(m_redesignPopoutWasMinimal);setPlayerView(m_redesignPopoutWasPlayer);setInterfaceAlwaysOnTop(m_redesignPopoutWasOnTop);
        const QRect saved=m_redesignPopoutGeometry;const auto visibility=m_redesignPopoutVisibility;window->setGeometry(saved);QTimer::singleShot(0,this,[window,saved,visibility]{window->setGeometry(saved);window->setVisibility(visibility);});
    }
    emit redesignPopoutChanged();
}

void MainCtx::rescanRedesignPlugins()
{
    if(findChild<QProcess*>("redesignPluginScan"))return;
    auto *scan=new QProcess(this);scan->setObjectName("redesignPluginScan");
#ifdef _WIN32
    scan->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a){a->flags|=0x08000000;});
#endif
    connect(scan,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this,scan](int code,QProcess::ExitStatus status){
        emit redesignShortcutPressed({},code==0&&status==QProcess::NormalExit?tr("Plugins rescanned. New plugins load next time VLC starts."):tr("Plugin rescan failed."));scan->deleteLater();
    });
    connect(scan,&QProcess::errorOccurred,this,[this,scan](QProcess::ProcessError){emit redesignShortcutPressed({},tr("Plugin rescan could not start."));scan->deleteLater();});
    const QString directory=QCoreApplication::applicationDirPath();
    scan->start(QDir(directory).filePath("vlc-cache-gen.exe"),QStringList{QDir(directory).filePath("plugins")});
}

void MainCtx::clearRedesignHistory()
{
    settings->remove("Redesign/frequent-folders");
    emit redesignFrequentFoldersChanged();
    if (m_medialib) m_medialib->runOnMLThread(this, [](vlc_medialibrary_t *ml) {
        vlc_ml_clear_history(ml, VLC_ML_HISTORY_TYPE_GLOBAL);
    });
}

void MainCtx::sendVLCHotkey(int vlcHotkey)
{
    var_SetInteger(vlc_object_instance(p_intf), "key-pressed", vlcHotkey);
}

QString MainCtx::redesignHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers) const
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    const int code = qtEventToVLCKey(&event);
    return code ? VLCKeyToString(code, false) : QString{};
}

void MainCtx::updateIntfScaleFactor()
{
    auto newValue = m_intfUserScaleFactor;
    if (QWindow* window = p_intf->p_compositor ? p_intf->p_compositor->interfaceMainWindow() : nullptr)
    {
        QScreen* screen = window->screen();
        if (screen)
        {
            qreal dpi = screen->logicalDotsPerInch();
            newValue = m_intfUserScaleFactor * dpi / VLC_REFERENCE_SCALE_FACTOR;
        }
    }

    const int FACTOR_RESOLUTION = 100;
    if ( static_cast<int>(m_intfScaleFactor * FACTOR_RESOLUTION)
         == static_cast<int>(newValue * FACTOR_RESOLUTION) ) return;

    m_intfScaleFactor = newValue;
    emit intfScaleFactorChanged();
}

void MainCtx::onWindowVisibilityChanged(QWindow::Visibility visibility)
{
    m_windowVisibility = visibility;
}

void MainCtx::setHasAcrylicSurface(const bool v)
{
    if (m_hasAcrylicSurface == v)
        return;

    m_hasAcrylicSurface = v;
    emit hasAcrylicSurfaceChanged(v);
}

void MainCtx::incrementIntfUserScaleFactor(bool increment)
{
    if (increment)
        setIntfUserScaleFactor(m_intfUserScaleFactor + 0.1);
    else
        setIntfUserScaleFactor(m_intfUserScaleFactor - 0.1);
}

void MainCtx::setIntfUserScaleFactor(double newValue)
{
    const auto value = qBound(getMinIntfUserScaleFactor(), newValue, getMaxIntfUserScaleFactor());
    if (qFuzzyCompare(value, m_intfUserScaleFactor))
        return;
    m_intfUserScaleFactor = value;
    updateIntfScaleFactor();
}

void MainCtx::setHasToolbarMenu( bool hasToolbarMenu )
{
    if (m_hasToolbarMenu == hasToolbarMenu)
        return;

    m_hasToolbarMenu = hasToolbarMenu;

    config_PutInt("qt-menubar", (int) hasToolbarMenu);

    emit hasToolbarMenuChanged();
}

void MainCtx::setPinVideoControls(bool pinVideoControls)
{
    if (m_pinVideoControls == pinVideoControls)
        return;

    m_pinVideoControls = pinVideoControls;
    emit pinVideoControlsChanged();
}

void MainCtx::setPinOpacity(float pinOpacity)
{
    if (m_pinOpacity == pinOpacity)
        return;

    m_pinOpacity = pinOpacity;

    emit pinOpacityChanged();
}

inline void MainCtx::initSystray()
{
    bool b_systrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    bool b_systrayWanted = var_InheritBool( p_intf, "qt-system-tray" );

    if( var_InheritBool( p_intf, "qt-start-minimized") )
    {
        if( b_systrayAvailable )
        {
            b_systrayWanted = true;
            b_hideAfterCreation = true;
        }
        else
            msg_Err( p_intf, "cannot start minimized without system tray bar" );
    }

    if( b_systrayAvailable && b_systrayWanted )
        m_systray = std::make_unique<VLCSystray>(this);
}

ThreadRunner* MainCtx::threadRunner() const
{
    return m_threadRunner;
}

QUrl MainCtx::folderMRL(const QString &fileMRL) const
{
    return folderMRL(QUrl::fromUserInput(fileMRL));
}

QUrl MainCtx::folderMRL(const QUrl &fileMRL) const
{
    if (fileMRL.isLocalFile())
    {
        const QString f = fileMRL.toLocalFile();
        return QUrl::fromLocalFile(QFileInfo(f).absoluteDir().absolutePath());
    }

    return {};
}

QString MainCtx::displayMRL(const QUrl &mrl) const
{
    return urlToDisplayString(mrl);
}

void MainCtx::setPlayerPlaylistWidth( double factor )
{
    if (factor > 0.0)
    {
        m_playerPlaylistWidth = factor;
        emit playerPlaylistWidthChanged(factor);
    }
}

void MainCtx::setbgCone(bool bgCone)
{
    if (m_bgCone == bgCone)
        return;

    m_bgCone = bgCone;

    emit bgConeToggled();
}

void MainCtx::setMinimalView(bool enable)
{
    if (m_mainInterfaceModes.testFlag(MAININTERFACE_MODE_MINIMAL) == enable)
        return;

    m_mainInterfaceModes.setFlag(MAININTERFACE_MODE_MINIMAL, enable);
    emit mainInterfaceModesChanged(m_mainInterfaceModes);
}

void MainCtx::setPlayerView(bool enable)
{
    if (m_mainInterfaceModes.testFlag(MAININTERFACE_MODE_PLAYER) == enable)
        return;

    m_mainInterfaceModes.setFlag(MAININTERFACE_MODE_PLAYER, enable);
    emit mainInterfaceModesChanged(m_mainInterfaceModes);
}

void MainCtx::setShowRemainingTime( bool show )
{
    m_showRemainingTime = show;
    emit showRemainingTimeChanged(show);
}

void MainCtx::setGridView(bool asGrid)
{
    m_gridView = asGrid;
    emit gridViewChanged( asGrid );
}

void MainCtx::setGrouping(Grouping grouping)
{
    m_grouping = grouping;

    emit groupingChanged(grouping);
}

void MainCtx::setAlbumSections(bool enabled)
{
    if (m_albumSections == enabled)
        return;

    m_albumSections = enabled;
    emit albumSectionsChanged(enabled);
}

void MainCtx::setLyricsMode(bool enabled)
{
    if (m_lyricsMode == enabled)
        return;

    m_lyricsMode = enabled;
    emit lyricsModeChanged(enabled);
}

void MainCtx::setInterfaceAlwaysOnTop( bool on_top )
{
    if (b_interfaceOnTop == on_top)
        return;

    b_interfaceOnTop = on_top;
    emit interfaceAlwaysOnTopChanged(on_top);
}

bool MainCtx::hasEmbededVideo() const
{
    return m_videoSurfaceProvider && m_videoSurfaceProvider->hasVideoEmbed();
}

void MainCtx::setVideoSurfaceProvider(VideoSurfaceProvider* videoSurfaceProvider)
{
    if (m_videoSurfaceProvider)
        disconnect(m_videoSurfaceProvider, &VideoSurfaceProvider::hasVideoEmbedChanged, this, &MainCtx::hasEmbededVideoChanged);
    m_videoSurfaceProvider = videoSurfaceProvider;
    if (m_videoSurfaceProvider)
        connect(m_videoSurfaceProvider, &VideoSurfaceProvider::hasVideoEmbedChanged,
                this, &MainCtx::hasEmbededVideoChanged,
                Qt::QueuedConnection);
    emit hasEmbededVideoChanged(m_videoSurfaceProvider && m_videoSurfaceProvider->hasVideoEmbed());
}

QJSValue MainCtx::urlListToMimeData(const QJSValue &array) {
    // NOTE: Due to a Qt regression since 17318c4
    //       (Nov 11, 2022), it is not possible to
    //       use RFC-2483 compliant string here.
    //       This regression was later corrected by
    //       c25f53b (Jul 31, 2024).
    // NOTE: Qt starts supporting string list since
    //       17318c4, so starting from 6.5.0 a string
    //       list can be used which is not affected
    //       by the said issue. For Qt versions below
    //       6.5.0, use byte array which is used as is
    //       by Qt.
    assert(array.property("length").toInt() > 0);

    QJSEngine* const engine = qjsEngine(this);
    assert(engine);

    QJSValue data;
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    QString string;
    for (int i = 0; i < array.property(QStringLiteral("length")).toInt(); ++i)
    {
        QString decodedUrl;
        const QJSValue element = array.property(i);
        if (element.isUrl())
            // QJSValue does not have `toUrl()`
            decodedUrl = QJSManagedValue(element, engine).toUrl().toString(QUrl::FullyEncoded);
        else if (element.isString())
            // If the element is string, we assume it is already encoded
            decodedUrl = element.toString();
        else if (element.isVariant())
        {
            const QVariant variant = element.toVariant();
            if (variant.typeId() == QMetaType::QUrl)
                decodedUrl = variant.toUrl().toString(QUrl::FullyEncoded);
            else if (variant.typeId() == QMetaType::QString)
                decodedUrl = variant.toString();
            else
                Q_UNREACHABLE();
        }
        else
            Q_UNREACHABLE(); // Assertion failure in debug builds
        string += decodedUrl + QStringLiteral("\r\n");
    }
    string.chop(2);
    data = engine->toScriptValue(string);
#else
    data = array;
#endif
    QJSValue ret = engine->newObject();
    ret.setProperty(QStringLiteral("text/uri-list"), data);
    return ret;
}

VideoSurfaceProvider* MainCtx::getVideoSurfaceProvider() const
{
    return m_videoSurfaceProvider;
}

/************************************************************************
 * Events stuff
 ************************************************************************/

void MainCtx::onWindowClose( QWindow* )
{
    PlaylistController* playlistController = p_intf->p_mainPlaylistController;
    PlayerController* playerController = p_intf->p_mainPlayerController;

    if (m_videoSurfaceProvider)
        m_videoSurfaceProvider->onWindowClosed();
    //We need to make sure that noting is playing anymore otherwise the vout will be closed
    //after the main interface, and it requires (at least with OpenGL) that the OpenGL context
    //from the main window is still valid.
    //vlc_window_ReportClose is currently stubbed
    if (playerController && playerController->hasVideoOutput()) {
        connect(playerController, &PlayerController::playingStateChanged, [this](PlayerController::PlayingState state){
            if (state == PlayerController::PLAYING_STATE_STOPPED) {
                emit askToQuit();
            }
        });
        playlistController->stop();
    }
    else
    {
        emit askToQuit(); /* ask THEDP to quit, so we have a unique method */
    }
}

void MainCtx::toggleToolbarMenu()
{
    setHasToolbarMenu(!m_hasToolbarMenu);
}

void MainCtx::toggleInterfaceFullScreen()
{
    emit setInterfaceFullScreen( m_windowVisibility != QWindow::FullScreen );
}

void MainCtx::emitBoss()
{
    emit askBoss();
}

void MainCtx::emitShow()
{
    emit askShow();
}

void MainCtx::emitRaise()
{
    emit askRaise();
}

VLCVarChoiceModel* MainCtx::getExtraInterfaces()
{
    return m_extraInterfaces;
}

bool MainCtx::pasteFromClipboard()
{
    assert(qApp);
    const QClipboard *const clipboard = qApp->clipboard();
    if (Q_UNLIKELY(!clipboard))
        return false;
    const QMimeData *mimeData = clipboard->mimeData(QClipboard::Selection);
    if (!mimeData || !mimeData->hasUrls())
        mimeData = clipboard->mimeData(QClipboard::Clipboard);

    if (Q_UNLIKELY(!mimeData))
        return false;

    QList<QUrl> urlList = mimeData->urls();
    QString text = mimeData->text();

    if (urlList.count() > 1 || text.contains('\n'))
    {
        // NOTE: The reason that mime data for `text/uri-list` is not used
        //       directly as the placeholder of the input dialog instead of
        //       re-constructing is to decode the urls in the list.
        QString placeholder;

        if (urlList.isEmpty())
        {
            placeholder = std::move(text);
        }
        else
        {
            for (const auto& i : urlList)
                placeholder += i.toString(QUrl::PrettyDecoded) + '\n';
            placeholder.chop(1);
        }

        bool ok = false;
        const QString ret = QInputDialog::getMultiLineText(nullptr,
                                                           qtr("Paste from clipboard"),
                                                           qtr("Do you want to enqueue the following URLs into the play queue?"),
                                                           placeholder,
                                                           &ok);
        if (!ok)
            return false;

        for (const auto& i : QStringView(ret).split('\n'))
        {
            if (i.length() > 0)
                THEMPL->append(i.trimmed().toString(), false);
        }

        return true;
    }
    else if ((urlList.count() == 1) || mimeData->hasText())
    {
        THEDP->openUrlDialog();
        return true;
    }

    return false;
}

/*****************************************************************************
 * PopupMenuCB: callback triggered by the intf-popupmenu playlist variable.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PopupMenuCB( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t new_val, void *param )
{
    qt_intf_t *p_intf = (qt_intf_t *)param;

    if( p_intf->pf_show_dialog )
    {
        p_intf->pf_show_dialog( p_intf->intf, INTF_DIALOG_POPUPMENU,
                                new_val.b_bool, NULL );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfShowCB: callback triggered by the intf-toggle-fscontrol libvlc variable.
 *****************************************************************************/
static int IntfShowCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void *param )
{
    qt_intf_t *p_intf = (qt_intf_t *)param;
    p_intf->p_mi->emitShow();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfRaiseMainCB: callback triggered by the intf-show-main libvlc variable.
 *****************************************************************************/
static int IntfRaiseMainCB( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void *param )
{
    qt_intf_t *p_intf = (qt_intf_t *)param;
    p_intf->p_mi->emitRaise();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfBossCB: callback triggered by the intf-boss libvlc variable.
 *****************************************************************************/
static int IntfBossCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void *param )
{
    qt_intf_t *p_intf = (qt_intf_t *)param;
    p_intf->p_mi->emitBoss();

    return VLC_SUCCESS;
}

bool MainCtx::acrylicActive() const
{
    return m_acrylicActive;
}

void MainCtx::setAcrylicActive(bool newAcrylicActive)
{
    if (m_acrylicActive == newAcrylicActive)
        return;

    m_acrylicActive = newAcrylicActive;
    emit acrylicActiveChanged();
}

bool MainCtx::preferHotkeys() const
{
    return m_preferHotkeys;
}

void MainCtx::setPreferHotkeys(bool enable)
{
    if (m_preferHotkeys == enable)
        return;

    m_preferHotkeys = enable;

    emit preferHotkeysChanged();
}

QWindow *MainCtx::intfMainWindow() const
{
    if (p_intf->p_compositor)
        return p_intf->p_compositor->interfaceMainWindow();
    else
        return nullptr;
}

QVariant MainCtx::settingValue(const QString &key, const QVariant &defaultValue) const
{
    return settings->value(key, defaultValue);
}

void MainCtx::setSettingValue(const QString &key, const QVariant &value)
{
    settings->setValue(key, value);
}

QVariant MainCtx::redesignConfigValue(const QString &key) const
{
    const auto name = key.toUtf8();
    const auto *option = config_FindConfig(name.constData());
    if (!option)
        return {};
    switch (CONFIG_CLASS(option->i_type))
    {
    case CONFIG_ITEM_BOOL:
        return config_GetInt(name.constData()) != 0;
    case CONFIG_ITEM_INTEGER:
        return QVariant::fromValue(config_GetInt(name.constData()));
    case CONFIG_ITEM_FLOAT:
        return config_GetFloat(name.constData());
    case CONFIG_ITEM_STRING:
    {
        char *value = config_GetPsz(name.constData());
        const QString text = QString::fromUtf8(value ? value : "");
        free(value);
        return text;
    }
    default:
        return {};
    }
}

void MainCtx::requestRedesignWaveform(const QUrl &url)
{
    if (!m_redesignWaveform) {
        m_redesignWaveform = std::make_unique<RedesignWaveform>([this](QVariantList peaks) {
            m_redesignWaveformPeaks = std::move(peaks);
            emit redesignWaveformChanged();
        }, this, m_threadRunner);
    }
    char *cache = config_GetUserDir(VLC_CACHE_DIR);
    if (!cache) {
        m_redesignWaveform->cancel();
        return;
    }
    const QString directory = QDir(QString::fromUtf8(cache ? cache : "")).filePath(QStringLiteral("redesign-waveforms"));
    free(cache);
    m_redesignWaveform->request(url, directory);
}

// A few core options (for example key-projection-toggle) have no text;
// show a readable label instead of the raw option name.
static QString redesignOptionLabel(const module_config_t &option)
{
    if (option.psz_text && *option.psz_text)
        return qfut(option.psz_text);
    QString name = QString::fromUtf8(option.psz_name);
    if (name == QLatin1String("key-projection-toggle") || name == QLatin1String("global-key-projection-toggle"))
        return qtr("Toggle 360° projection");
    for (const auto prefix : {QLatin1String("global-key-"), QLatin1String("key-")})
        if (name.startsWith(prefix)) { name.remove(0, prefix.size()); break; }
    name.replace(QLatin1Char('-'), QLatin1Char(' ')).replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!name.isEmpty()) name[0] = name[0].toUpper();
    return name;
}

QVariantList MainCtx::redesignConfigOptions() const
{
    QVariantList result;
    QSet<QString> presented;
    size_t count = 0;
    module_t **modules = module_list_get(&count);
    if (!modules)
        return result;
    for (size_t i = 0; i < count; ++i)
    {
        unsigned size = 0;
        module_config_t *config = module_config_get(modules[i], &size);
        auto subcategory = SUBCAT_UNKNOWN;
        for (unsigned j = 0; j < size; ++j)
        {
            const auto &option = config[j];
            if (option.i_type == CONFIG_SUBCATEGORY)
            {
                subcategory = static_cast<vlc_config_subcat>(option.value.i);
                continue;
            }
            if (!CONFIG_ITEM(option.i_type) || !option.psz_name ||
                subcategory == SUBCAT_UNKNOWN || subcategory == SUBCAT_HIDDEN)
                continue;
            const auto type = CONFIG_CLASS(option.i_type);
            QVariantMap row;
            const auto key = QString::fromUtf8(option.psz_name);
            if (presented.contains(key) || !config_FindConfig(option.psz_name)) continue;
            presented.insert(key);
            row.insert(QStringLiteral("key"), key);
            row.insert(QStringLiteral("label"), redesignOptionLabel(option));
            row.insert(QStringLiteral("description"), qfut(option.psz_longtext ? option.psz_longtext : ""));
            row.insert(QStringLiteral("module"), QString::fromUtf8(module_get_object(modules[i])));
            row.insert(QStringLiteral("subcategory"), static_cast<int>(subcategory));
            row.insert(QStringLiteral("type"), type == CONFIG_ITEM_BOOL ? QStringLiteral("bool") :
                       type == CONFIG_ITEM_INTEGER ? QStringLiteral("int") :
                       type == CONFIG_ITEM_FLOAT ? QStringLiteral("float") : QStringLiteral("string"));
            row.insert(QStringLiteral("password"), option.i_type == CONFIG_ITEM_PASSWORD);
            row.insert(QStringLiteral("hotkey"), option.i_type == CONFIG_ITEM_KEY);
            row.insert(QStringLiteral("color"), option.i_type == CONFIG_ITEM_RGB);
            row.insert(QStringLiteral("path"), option.i_type == CONFIG_ITEM_LOADFILE ||
                       option.i_type == CONFIG_ITEM_SAVEFILE || option.i_type == CONFIG_ITEM_DIRECTORY);
            row.insert(QStringLiteral("value"), redesignConfigValue(key));
            row.insert("minimum", type == CONFIG_ITEM_FLOAT ? option.min.f : double(option.min.i));
            row.insert("maximum", type == CONFIG_ITEM_FLOAT ? option.max.f : double(option.max.i));
            row.insert("defaultValue", type == CONFIG_ITEM_FLOAT ? QVariant(option.orig.f) :
                (type == CONFIG_ITEM_BOOL ? QVariant(bool(option.orig.i)) :
                (type == CONFIG_ITEM_INTEGER ? QVariant::fromValue(option.orig.i) : QVariant(QString::fromUtf8(option.orig.psz ? option.orig.psz : "")))));
            QVariantList choices;
            // Dynamic module choices continue to use VLC's native editor. Static
            // choices can be represented directly without invoking module code.
            for (unsigned k = 0; k < option.list_count; ++k)
            {
                QVariantMap choice;
                if (type == CONFIG_ITEM_STRING)
                    choice.insert(QStringLiteral("value"), QString::fromUtf8(option.list.psz[k]));
                else if (type == CONFIG_ITEM_INTEGER || type == CONFIG_ITEM_BOOL)
                    choice.insert(QStringLiteral("value"), option.list.i[k]);
                else
                    continue;
                choice.insert(QStringLiteral("label"), qfut(option.list_text && option.list_text[k] ? option.list_text[k] : ""));
                choices.append(choice);
            }
            if (key == "qt-fullscreen-screennumber") {
                choices.append(QVariantMap{{"value",-1},{"label",qtr("Current screen")}});
                const auto screens = QGuiApplication::screens();
                for (int screen=0;screen<screens.size();++screen)
                    choices.append(QVariantMap{{"value",screen},{"label",QStringLiteral("%1 — %2").arg(screen+1).arg(screens[screen]->name())}});
            }
            row.insert(QStringLiteral("choices"), choices);
            if (key == QStringLiteral("qt-redesign-language"))
            {
                QVariantList languages;
                languages.append(QVariantMap{{"value", "auto"}, {"label", qtr("System language")}});
                languages.append(QVariantMap{{"value", "en"}, {"label", QStringLiteral("English")}});
                const QDir locale(QCoreApplication::applicationDirPath() + QStringLiteral("/locale"));
                for (const auto &code : locale.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                {
                    const QLocale language(code);
                    const QString label = language.nativeLanguageName().isEmpty() ? code : language.nativeLanguageName();
                    languages.append(QVariantMap{{"value", code}, {"label", QStringLiteral("%1 (%2)").arg(label, code)}});
                }
                row.insert(QStringLiteral("choices"), languages);
            }
            result.append(row);
        }
        module_config_free(config);
    }
    module_list_free(modules);
    return result;
}

bool MainCtx::saveRedesignConfig(const QVariantMap &changes, bool persist)
{
    QVariantMap previous;
    // Validate the entire draft before changing the registry. This also rejects
    // retired/unknown keys and malformed numerical values from UI input.
    for (auto i = changes.cbegin(); i != changes.cend(); ++i)
    {
        const auto name = i.key().toUtf8();
        const module_config_t *option = config_FindConfig(name.constData());
        if (!option || !CONFIG_ITEM(option->i_type))
            return false;
        if (i.key() == QStringLiteral("qt-redesign-language") &&
            !QRegularExpression(QStringLiteral("^[A-Za-z_@-]{1,24}$")).match(i.value().toString()).hasMatch())
            return false;
        const auto type = CONFIG_CLASS(option->i_type);
        bool valid = true;
        if (type == CONFIG_ITEM_BOOL)
            valid = i.value().metaType().id() == QMetaType::Bool;
        else if (type == CONFIG_ITEM_INTEGER)
        {
            const auto number = i.value().toLongLong(&valid);
            bool realValid = false;
            const double real = i.value().toDouble(&realValid);
            valid = valid && realValid && std::isfinite(real) && std::floor(real) == real
                && number >= option->min.i && number <= option->max.i;
        }
        else if (type == CONFIG_ITEM_FLOAT)
        {
            const auto number = i.value().toDouble(&valid);
            valid = valid && std::isfinite(number) && number >= option->min.f && number <= option->max.f;
        }
        else if (type != CONFIG_ITEM_STRING)
            valid = false;
        if (!valid)
            return false;
        previous.insert(i.key(), redesignConfigValue(i.key()));
    }
    const auto apply = [](const QVariantMap &values) {
        for (auto i = values.cbegin(); i != values.cend(); ++i)
        {
            const auto name = i.key().toUtf8();
            const auto *option = config_FindConfig(name.constData());
            if (!option)
                continue;
            switch (CONFIG_CLASS(option->i_type))
            {
            case CONFIG_ITEM_BOOL:
            case CONFIG_ITEM_INTEGER: config_PutInt(name.constData(), i.value().toLongLong()); break;
            case CONFIG_ITEM_FLOAT: config_PutFloat(name.constData(), i.value().toFloat()); break;
            case CONFIG_ITEM_STRING: config_PutPsz(name.constData(), qtu(i.value().toString())); break;
            default: break;
            }
        }
    };
    apply(changes);
    if (persist && config_SaveConfigFile(VLC_OBJECT(p_intf)) != 0)
    {
        apply(previous);
        return false;
    }
    if (persist && changes.contains(QStringLiteral("qt-redesign-language")))
    {
        const auto path = QFileInfo(getSettings()->fileName()).absolutePath() + QStringLiteral("/interface-language.txt");
        QSaveFile file(path);
        const QByteArray code = changes.value(QStringLiteral("qt-redesign-language")).toString().toUtf8();
        if (!file.open(QFile::WriteOnly) || file.write(code) != code.size() || !file.commit())
        {
            apply(previous);
            config_SaveConfigFile(VLC_OBJECT(p_intf));
            return false;
        }
    }
    reloadPrefs();
    applyRedesignWidgetStyle(this);
    if (std::any_of(changes.keyBegin(), changes.keyEnd(), [](const QString &key) { return key.startsWith("qt-redesign-"); }))
        emit redesignPresentationChanged();
    return true;
}

QVariant MainCtx::redesignLiveValue(const QString &key) const
{
    const auto name = key.toUtf8();
    const auto *option = config_FindConfig(name.constData());
    if (!option) return {};
    if (key == QStringLiteral("sub-text-scale")) {
        vlc_player_Lock(p_intf->p_player);
        const unsigned scale = vlc_player_GetSubtitleTextScale(p_intf->p_player);
        vlc_player_Unlock(p_intf->p_player);
        return scale;
    }
    auto audio = p_intf->p_mainPlayerController->getAout();
    auto video = p_intf->p_mainPlayerController->getVout();
    vlc_object_t *object = audio && var_Type(audio.get(), name.constData())
        ? VLC_OBJECT(audio.get()) : video && var_Type(video.get(), name.constData()) ? VLC_OBJECT(video.get()) : nullptr;
    if (!object) return redesignConfigValue(key);
    switch (config_GetType(name.constData())) {
    case VLC_VAR_BOOL: return var_GetBool(object, name.constData());
    case VLC_VAR_INTEGER: return QVariant::fromValue(var_GetInteger(object, name.constData()));
    case VLC_VAR_FLOAT: return var_GetFloat(object, name.constData());
    case VLC_VAR_STRING: {
        char *value = var_GetString(object, name.constData());
        const QString text = QString::fromUtf8(value ? value : "");
        free(value);
        return text;
    }
    default: return redesignConfigValue(key);
    }
}

bool MainCtx::applyRedesignLiveOptions(const QVariantMap &changes)
{
    const QStringList oldAudio = redesignLiveValue("audio-filter").toString().split(':', Qt::SkipEmptyParts);
    if (!saveRedesignConfig(changes, false)) return false;
    if (changes.contains("sub-text-scale")) {
        vlc_player_Lock(p_intf->p_player);
        vlc_player_SetSubtitleTextScale(p_intf->p_player, changes.value("sub-text-scale").toUInt());
        vlc_player_Unlock(p_intf->p_player);
    }
    if (changes.contains("subsdelay-factor"))
        setRedesignFilter("subsdelay", changes.value("subsdelay-factor").toDouble() > 0);
    if (changes.contains("audio-filter")) {
        const QStringList newAudio = changes.value("audio-filter").toString().split(':', Qt::SkipEmptyParts);
        for (const auto &filter : oldAudio)
            if (!newAudio.contains(filter)) vlc_player_aout_EnableFilter(p_intf->p_player, qtu(filter), false);
        for (const auto &filter : newAudio)
            if (!oldAudio.contains(filter)) vlc_player_aout_EnableFilter(p_intf->p_player, qtu(filter), true);
    }
    auto audio = p_intf->p_mainPlayerController->getAout();
    auto video = p_intf->p_mainPlayerController->getVout();
    for (auto i = changes.cbegin(); i != changes.cend(); ++i) {
        const auto name = i.key().toUtf8();
        const int type = config_GetType(name.constData());
        vlc_value_t value;
        const QByteArray text = i.value().toString().toUtf8();
        switch (type) {
        case VLC_VAR_BOOL: value.b_bool = i.value().toBool(); break;
        case VLC_VAR_INTEGER: value.i_int = i.value().toLongLong(); break;
        case VLC_VAR_FLOAT: value.f_float = i.value().toFloat(); break;
        case VLC_VAR_STRING: value.psz_string = const_cast<char *>(text.constData()); break;
        default: continue;
        }
        // The existing effects dialog updates these same output variables.
        if (audio && var_Type(audio.get(), name.constData()))
            var_SetChecked(audio.get(), name.constData(), type, value);
        if (video && var_Type(video.get(), name.constData()))
            var_SetChecked(video.get(), name.constData(), type, value);
    }
    return true;
}

bool MainCtx::setRedesignFilter(const QString &name, bool enabled)
{
    const auto moduleName = name.toUtf8();
    module_t *module = module_find(moduleName.constData());
    if (!module) return false;
    const bool isAudio = module_provides(module, "audio filter");
    const bool isSubtitle = module_provides(module, "sub filter");
    if (!isAudio && !isSubtitle && !module_provides(module, "video filter")) return false;
    const QString key = isAudio ? QStringLiteral("audio-filter") : isSubtitle ? QStringLiteral("sub-filter") : QStringLiteral("video-filter");
    QStringList filters = redesignLiveValue(key).toString().split(':', Qt::SkipEmptyParts);
    filters.removeAll(name);
    if (enabled) filters.append(name);
    const auto chain = filters.join(':').toUtf8();
    config_PutPsz(qtu(key), chain.constData());
    if (isAudio) vlc_player_aout_EnableFilter(p_intf->p_player, moduleName.constData(), enabled);
    else {
        auto video = p_intf->p_mainPlayerController->getVout();
        if (video) var_SetString(video.get(), qtu(key), chain.constData());
    }
    return true;
}

void MainCtx::setRedesignSleepTimer(int minutes)
{
    if (minutes < 0 || minutes > 1440) return;
    m_redesignSleepTimer.stop();
    if (minutes) m_redesignSleepTimer.start(minutes * 60000);
    emit redesignSleepTimerChanged();
}

QVariantList MainCtx::redesignFrequentFolders() const
{
    if (!var_InheritBool(p_intf, "save-recentplay")) return {};
    QList<QVariantMap> folders;
    const auto values = settings->value(QStringLiteral("Redesign/frequent-folders")).toMap();
    for (auto i = values.cbegin(); i != values.cend(); ++i) {
        if (!QFileInfo(i.key()).isDir()) continue;
        auto entry = i.value().toMap();
        entry["path"] = i.key();
        entry["label"] = QFileInfo(i.key()).fileName();
        folders.append(entry);
    }
    std::sort(folders.begin(), folders.end(), [](const auto &a, const auto &b) {
        if (a.value("count").toInt() != b.value("count").toInt()) return a.value("count").toInt() > b.value("count").toInt();
        return a.value("lastPlayed").toLongLong() > b.value("lastPlayed").toLongLong();
    });
    QVariantList result;
    for (const auto &folder : folders) { result.append(folder); if (result.size() == 12) break; }
    return result;
}

QVariant MainCtx::redesignFolderTree(const QString &path) const
{
    if (!QFileInfo(path).isDir()) return {};
    const QUrl url = QUrl::fromLocalFile(path);
    input_item_t *item = input_item_NewDirectory(qtu(url.toString(QUrl::FullyEncoded)), qtu(QFileInfo(path).fileName()), ITEM_LOCAL);
    if (!item) return {};
    SharedInputItem input(item, false);
    MediaTreePtr tree(vlc_media_tree_New(), false);
    if (!tree) return {};
    { MediaTreeLocker lock(tree); vlc_media_tree_Add(tree.get(), &tree->root, item); }
    return QVariant::fromValue(NetworkTreeItem(tree, input));
}

bool MainCtx::addRedesignMediaFolder(const QString &path)
{
    if (!m_medialib || !QFileInfo(path).isDir()) return false;
    const QUrl url = QUrl::fromLocalFile(path);
    m_medialib->runOnMLThread(this, [url](vlc_medialibrary_t *ml) {
        vlc_ml_add_folder(ml, qtu(url.toString(QUrl::FullyEncoded)));
    });
    return true;
}

QVariantList MainCtx::redesignSubtitleCues() const
{
    const QString media = p_intf->p_mainPlayerController->getUrl().toString(QUrl::FullyEncoded);
    const auto saved = settings->value(QStringLiteral("Redesign/subtitle-files")).toMap();
    const QUrl subtitle(saved.value(media).toString());
    if (!subtitle.isLocalFile()) return {};
    return readRedesignSubtitleCues(subtitle.toLocalFile(), redesignConfigValue("subsdec-encoding").toString());
}

void MainCtx::cancelRedesignSubtitleTimeline()
{
    if (m_redesignTimeline) m_redesignTimeline->cancel();
    m_redesignTimelineBusy = false;
    emit redesignTimelineChanged();
}

QVariantMap MainCtx::redesignMediaLocation() const
{
    const QUrl url = p_intf->p_mainPlayerController->getUrl();
    QString playlist;
    { vlc_player_locker lock(p_intf->p_player);
      if (auto *item = vlc_player_GetCurrentMedia(p_intf->p_player)) {
          char *name = input_item_GetMetaExtra(item, "VLC playlist");
          if (name) { playlist = QString::fromUtf8(name); free(name); }
      } }
    if (!url.isLocalFile()) return {{"file", url.fileName()}, {"folder", QString{}}, {"playlist", playlist}};
    const QFileInfo file(url.toLocalFile());
    return {{"file", file.fileName()}, {"folder", QDir(file.absolutePath()).dirName()}, {"playlist", playlist}};
}

void MainCtx::refreshRedesignFavorite(bool toggle)
{
    if (!m_medialib || (toggle && m_redesignFavoriteBusy)) return;
    const QString uri = p_intf->p_mainPlayerController->getUrl().toString(QUrl::FullyEncoded);
    if (uri.isEmpty()) return;
    const quint64 revision = ++m_redesignFavoriteRevision;
    m_redesignFavoriteBusy = true; emit redesignFavoriteChanged();
    struct Result { bool available = false, favorite = false; };
    m_medialib->runOnMLThread<Result>(this,
        [uri, toggle](vlc_medialibrary_t *ml, Result &result) {
            const QByteArray encoded = uri.toUtf8();
            ml_unique_ptr<vlc_ml_media_t> media{vlc_ml_get_media_by_mrl(ml, encoded.constData())};
            if (!media) return;
            result.available = true; result.favorite = media->b_is_favorite;
            if (toggle && vlc_ml_media_set_favorite(ml, media->i_id, !result.favorite) == VLC_SUCCESS)
                result.favorite = !result.favorite;
        },
        [this, uri, revision](quint64, Result &result) {
            if (revision != m_redesignFavoriteRevision
                || uri != p_intf->p_mainPlayerController->getUrl().toString(QUrl::FullyEncoded)) return;
            m_redesignFavoriteAvailable = result.available; m_redesignIsFavorite = result.favorite;
            m_redesignFavoriteBusy = false; emit redesignFavoriteChanged();
        }, "redesign-current-media");
}

void MainCtx::addRedesignBookmark()
{
    if (!m_medialib) { emit redesignBookmarkAdded(-1, false); return; }
    const QString uri = p_intf->p_mainPlayerController->getUrl().toString(QUrl::FullyEncoded);
    // The controller also tracks completed seeks while paused. The raw player
    // clock may still interpolate its pre-seek point until decoding resumes.
    const vlc_tick_t time = p_intf->p_mainPlayerController->getTime().toVLCTick();
    if (time == VLC_TICK_INVALID || time < 0) { emit redesignBookmarkAdded(-1, false); return; }
    const int64_t milliseconds = MS_FROM_VLC_TICK(time);
    struct Result { bool success = false; };
    m_medialib->runOnMLThread<Result>(this, [uri, milliseconds](vlc_medialibrary_t *ml, Result &result) {
        const QByteArray encoded = uri.toUtf8();
        ml_unique_ptr<vlc_ml_media_t> media{vlc_ml_get_media_by_mrl(ml, encoded.constData())};
        if (!media) media.reset(vlc_ml_new_external_media(ml, encoded.constData()));
        if (!media) return;
        if (vlc_ml_media_add_bookmark(ml, media->i_id, milliseconds) == VLC_SUCCESS) {
            const QByteArray name = qtr("Bookmark at %1").arg(VLCDuration::fromMS(milliseconds).formatHMS()).toUtf8();
            result.success = vlc_ml_media_update_bookmark(ml, media->i_id, milliseconds, name.constData(), nullptr) == VLC_SUCCESS;
        }
    }, [this, milliseconds](quint64, Result &result) { emit redesignBookmarkAdded(milliseconds, result.success); }, "redesign-current-media");
}

void MainCtx::requestRedesignSubtitleTimeline()
{
    cancelRedesignSubtitleTimeline();
    m_redesignTimelineExternal = false;
    m_redesignTimelineCues.clear(); m_redesignTimelineMessage.clear();
    const QUrl media = p_intf->p_mainPlayerController->getUrl();
    QString id, external; vlc_fourcc_t codec = 0;
    {
        vlc_player_locker lock(p_intf->p_player);
        const auto *track = vlc_player_GetSelectedTrack(p_intf->p_player, SPU_ES);
        if (track) {
            id = QString::fromUtf8(vlc_es_id_GetStrId(track->es_id)); codec = track->fmt.i_codec;
            auto *item = vlc_player_GetCurrentMedia(p_intf->p_player);
            if (item) {
                vlc_mutex_locker itemLock(&item->lock);
                for (int i = 0; i < item->i_slaves; ++i) {
                    const auto *slave = item->pp_slaves[i];
                    if (slave->i_type != SLAVE_TYPE_SPU) continue;
                    const QString uri = QString::fromUtf8(slave->psz_uri);
                    const QString hash = QString::fromLatin1(QCryptographicHash::hash(uri.toUtf8(), QCryptographicHash::Md5).toHex());
                    if (id.startsWith(hash + '/')) { external = uri; break; }
                }
            }
        }
    }
    const auto saved = settings->value("Redesign/subtitle-files").toMap();
    const QString known = saved.value(media.toString(QUrl::FullyEncoded)).toString();
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(known.toUtf8(), QCryptographicHash::Md5).toHex());
    if (external.isEmpty() && !known.isEmpty() && id.startsWith(hash + '/')) external = known;
    if (!external.isEmpty()) {
        m_redesignTimelineExternal = true;
        const QUrl subtitle(external);
        if (subtitle.isLocalFile()) m_redesignTimelineCues = readRedesignSubtitleCues(subtitle.toLocalFile(), redesignConfigValue("subsdec-encoding").toString());
        if (m_redesignTimelineCues.isEmpty()) m_redesignTimelineMessage = qtr("This subtitle file has no readable text timeline.");
        emit redesignTimelineChanged(); return;
    }
    if (id.isEmpty()) { m_redesignTimelineMessage = qtr("Select a subtitle track or add a subtitle file to view its timeline."); emit redesignTimelineChanged(); return; }
    m_redesignTimelineMedia = media.toString(QUrl::FullyEncoded); m_redesignTimelineTrack = id;
    if (!m_redesignTimeline)
        m_redesignTimeline = std::make_unique<RedesignSubtitleTimeline>(this, [this](QVariantList cues, bool busy, QString message) {
            QString selected;
            { vlc_player_locker lock(p_intf->p_player);
              if (const auto *track = vlc_player_GetSelectedTrack(p_intf->p_player, SPU_ES)) selected = QString::fromUtf8(vlc_es_id_GetStrId(track->es_id)); }
            if (m_redesignTimelineMedia != p_intf->p_mainPlayerController->getUrl().toString(QUrl::FullyEncoded)
                || selected != m_redesignTimelineTrack) { m_redesignTimelineBusy = false; emit redesignTimelineChanged(); return; }
            m_redesignTimelineCues = std::move(cues); m_redesignTimelineBusy = busy; m_redesignTimelineMessage = std::move(message);
            emit redesignTimelineChanged();
        });
    m_redesignTimeline->request(media, id, codec == VLC_CODEC_SSA, QDir(QFileInfo(settings->fileName()).absolutePath()).filePath("SubtitleTimelines"));
}

double MainCtx::redesignSubtitleCueTime(double seconds) const
{
    if (!std::isfinite(seconds) || seconds < 0) return 0;
    vlc_player_locker lock(p_intf->p_player);
    const double scale = m_redesignTimelineExternal ? vlc_player_GetAssociatedSubsTimeScale(p_intf->p_player) : 1.0;
    return seconds * scale + double(vlc_player_GetSubtitleDelay(p_intf->p_player)) / CLOCK_FREQ;
}

void MainCtx::cancelRedesignSeekPreview()
{
    if (m_redesignSeekPreview) m_redesignSeekPreview->cancel();
    m_redesignSeekPreviewUrl.clear(); m_redesignSeekPreviewTime = 0; emit redesignSeekPreviewChanged();
}

void MainCtx::requestRedesignSeekPreview(double position)
{
    const auto *player = p_intf->p_mainPlayerController;
    const double length = player->getLength().toSeconds();
    const QUrl media = player->getUrl();
    if (!std::isfinite(position) || !media.isLocalFile() || !player->hasVideoOutput() || length <= 0) { cancelRedesignSeekPreview(); return; }
    if (!m_redesignSeekPreview)
        m_redesignSeekPreview = std::make_unique<RedesignSeekPreview>(this, [this](QUrl image, double seconds) {
            m_redesignSeekPreviewUrl = image; m_redesignSeekPreviewTime = seconds; emit redesignSeekPreviewChanged();
        });
    m_redesignSeekPreview->request(media, std::clamp(position, 0.0, .999) * length,
        QDir(QFileInfo(settings->fileName()).absolutePath()).filePath("SeekPreviews"));
}

QVariantMap MainCtx::redesignSubtitleDownloadState() const
{
    return m_redesignSubtitles ? m_redesignSubtitles->state() : QVariantMap{{"busy", false}, {"authenticated", false}, {"rows", QVariantList{}}};
}

void MainCtx::loginRedesignSubtitles(const QString &username, const QString &password, const QString &applicationKey)
{
    if (!m_redesignSubtitles)
        m_redesignSubtitles = std::make_unique<RedesignSubtitles>(this, QDir(QFileInfo(settings->fileName()).absolutePath()).filePath("DownloadedSubtitles"),
            [this] { emit redesignSubtitleDownloadChanged(); });
    m_redesignSubtitles->login(username, password, applicationKey);
}

void MainCtx::searchRedesignSubtitles(const QVariantList &media, const QString &language)
{
    if (!m_redesignSubtitles)
        m_redesignSubtitles = std::make_unique<RedesignSubtitles>(this, QDir(QFileInfo(settings->fileName()).absolutePath()).filePath("DownloadedSubtitles"),
            [this] { emit redesignSubtitleDownloadChanged(); });
    m_redesignSubtitles->search(media, language);
}
void MainCtx::logoutRedesignSubtitles() { if (m_redesignSubtitles) m_redesignSubtitles->logout(); }
void MainCtx::selectRedesignSubtitleDownload(int index, int choice, bool enabled) { if (m_redesignSubtitles) m_redesignSubtitles->select(index, choice, enabled); }
void MainCtx::downloadRedesignSubtitles() { if (m_redesignSubtitles) m_redesignSubtitles->download(); }
void MainCtx::cancelRedesignSubtitleDownloads() { if (m_redesignSubtitles) m_redesignSubtitles->cancel(); }
bool MainCtx::applyRedesignSubtitleDownloads()
{
    if (!m_redesignSubtitles || m_redesignSubtitles->state().value("busy").toBool()) return false;
    const auto pairs = m_redesignSubtitles->downloadedPairs();
    return !pairs.isEmpty() && saveRedesignSubtitlePairs(pairs);
}

QVariantMap MainCtx::redesignInterfaceState() const
{
    QVariantMap values;
    for (const auto &entry : redesignConfigOptions()) {
        const auto option = entry.toMap();
        if (option.value("password").toBool()) continue;
        const QString key = option.value("key").toString();
        values[key] = redesignConfigValue(key);
    }
    return {{"options", values}, {"scheme", m_colorScheme->currentIndex()}};
}

bool MainCtx::applyRedesignInterfaceState(const QVariantMap &state, bool persist)
{
    const auto values = state.value("options").toMap();
    const int scheme = state.value("scheme", 2).toInt();
    if (values.isEmpty() || scheme < 0 || scheme > 3 || !saveRedesignConfig(values, persist)) return false;
    m_colorScheme->setCurrentIndex(scheme);
    return true;
}

void MainCtx::storeRedesignInterfaceProfile(const QString &name)
{
    if (name.isEmpty()) return;
    const QString key = QStringLiteral("Redesign/profile-settings/") + QString::fromLatin1(QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha256).toHex());
    settings->setValue(key, redesignInterfaceState());
    config_SaveConfigFile(VLC_OBJECT(p_intf));
}

bool MainCtx::loadRedesignInterfaceProfile(const QString &name)
{
    const QString key = QStringLiteral("Redesign/profile-settings/") + QString::fromLatin1(QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha256).toHex());
    return applyRedesignInterfaceState(settings->value(key).toMap(), false);
}

namespace {
QString redesignEpisodeKey(const QString &stem)
{
    static const QRegularExpression episode(QStringLiteral("(?:s(\\d{1,3})[ ._-]*e(\\d{1,4})|(\\d{1,3})x(\\d{1,4}))"), QRegularExpression::CaseInsensitiveOption);
    const auto match = episode.match(stem);
    if (!match.hasMatch()) return {};
    const int season = (match.captured(1).isEmpty() ? match.captured(3) : match.captured(1)).toInt();
    const int number = (match.captured(2).isEmpty() ? match.captured(4) : match.captured(2)).toInt();
    return QString::number(season) + ':' + QString::number(number);
}
QString redesignShowKey(const QString &stem)
{
    static const QRegularExpression episode(QStringLiteral("(?:s\\d{1,3}[ ._-]*e\\d{1,4}|\\d{1,3}x\\d{1,4})"), QRegularExpression::CaseInsensitiveOption);
    const auto match = episode.match(stem);
    if (!match.hasMatch()) return {};
    QString show = stem.left(match.capturedStart()).toCaseFolded();
    show.remove(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]")));
    return show;
}
bool redesignIsSubtitle(const QFileInfo &file)
{
    static const QStringList extensions { "srt", "vtt", "ass", "ssa", "sub", "smi", "ttml", "idx" };
    return file.isFile() && extensions.contains(file.suffix().toLower());
}
}

QVariantList MainCtx::previewRedesignSubtitles(const QString &scope, const QString &folder)
{
    QList<QUrl> media;
    if (scope == QStringLiteral("current")) media.append(p_intf->p_mainPlayerController->getUrl());
    else if (scope == QStringLiteral("playlist")) {
        vlc_playlist_locker lock(p_intf->p_playlist);
        const size_t count = std::min<size_t>(500, vlc_playlist_Count(p_intf->p_playlist));
        for (size_t i = 0; i < count; ++i) {
            char *uri = input_item_GetURI(vlc_playlist_item_GetMedia(vlc_playlist_Get(p_intf->p_playlist, i)));
            media.append(QUrl(QString::fromUtf8(uri ? uri : "")));
            free(uri);
        }
    } else if (scope == QStringLiteral("folder")) {
        const QDir directory(folder);
        if (!QFileInfo(folder).isDir()) return {};
        const QStringList extensions = QString::fromLatin1(EXTENSIONS_VIDEO).split(';');
        const auto files = directory.entryInfoList(extensions, QDir::Files | QDir::Readable, QDir::Name);
        for (const auto &file : files) {
            if (media.size() == 500) break;
            media.append(QUrl::fromLocalFile(file.absoluteFilePath()));
        }
    } else return {};
    const auto saved = settings->value(QStringLiteral("Redesign/subtitle-files")).toMap();
    QVariantList result;
    QSet<QString> seen;
    for (const auto &url : media) {
        const QString uri = url.toString(QUrl::FullyEncoded);
        if (!url.isValid() || url.isEmpty() || seen.contains(uri)) continue;
        seen.insert(uri);
        const QFileInfo file(url.isLocalFile() ? url.toLocalFile() : url.path());
        const QString stem = file.completeBaseName();
        const QDir directory(!folder.isEmpty() ? folder : file.absolutePath());
        const QString episode = redesignEpisodeKey(stem);
        QVariantList candidates;
        for (const auto &subtitle : directory.entryInfoList(QDir::Files | QDir::Readable, QDir::Name)) {
            if (!redesignIsSubtitle(subtitle)) continue;
            const QString subtitleStem = subtitle.completeBaseName();
            const bool exact = subtitleStem.compare(stem, Qt::CaseInsensitive) == 0
                || subtitleStem.startsWith(stem + '.', Qt::CaseInsensitive);
            const bool sameEpisode = !episode.isEmpty() && episode == redesignEpisodeKey(subtitleStem)
                && redesignShowKey(stem) == redesignShowKey(subtitleStem);
            if (!exact && !sameEpisode) continue;
            candidates.append(QVariantMap{{"label", subtitle.fileName()}, {"url", QUrl::fromLocalFile(subtitle.absoluteFilePath()).toString(QUrl::FullyEncoded)}});
            if (candidates.size() == 100) break;
        }
        const QString old = saved.value(uri).toString();
        int selected = -1;
        for (qsizetype i = 0; i < candidates.size(); ++i)
            if (candidates[i].toMap().value("url").toString() == old) selected = i;
        if (selected < 0 && candidates.size() == 1) selected = 0;
        result.append(QVariantMap{{"media", uri}, {"label", file.fileName()}, {"candidates", candidates}, {"selected", selected}, {"enabled", selected >= 0}});
    }
    return result;
}

bool MainCtx::saveRedesignSubtitlePairs(const QVariantList &pairs)
{
    if (pairs.size() > 500) return false;
    auto saved = settings->value(QStringLiteral("Redesign/subtitle-files")).toMap();
    for (const auto &entry : pairs) {
        const auto pair = entry.toMap();
        if (!pair.value("enabled").toBool()) continue;
        const QUrl media(pair.value("media").toString()), subtitle(pair.value("subtitle").toString());
        if (!media.isValid() || media.isEmpty() || !subtitle.isLocalFile()
            || !redesignIsSubtitle(QFileInfo(subtitle.toLocalFile()))) return false;
        if (media.isLocalFile() && !QFileInfo(media.toLocalFile()).isFile()) return false;
        saved[media.toString(QUrl::FullyEncoded)] = subtitle.toString(QUrl::FullyEncoded);
    }
    settings->setValue(QStringLiteral("Redesign/subtitle-files"), saved);
    settings->sync();
    if (settings->status() != QSettings::NoError) return false;
    const QString current = p_intf->p_mainPlayerController->getUrl().toString(QUrl::FullyEncoded);
    if (saved.contains(current)) p_intf->p_mainPlayerController->associateSubtitleFile(saved.value(current).toString());
    return true;
}

QString MainCtx::pickRedesignPath(const QString &key, const QString &current)
{
    if (key == QStringLiteral("redesign-addon-file"))
        return QFileDialog::getOpenFileName(nullptr,qtr("Import an add-on package"),current,qtr("VLC add-on packages (*.vlp);;All files (*)"));
    if (key == QStringLiteral("redesign-media-folder"))
        return QFileDialog::getExistingDirectory(nullptr, qtr("Choose a media folder"), current);
    if (key == QStringLiteral("redesign-subtitle-file"))
        return QFileDialog::getOpenFileName(nullptr, qtr("Choose subtitles"), current, qtr("Subtitle files (*.srt *.vtt *.ass *.ssa *.sub *.smi *.ttml *.idx)"));
    const auto name = key.toUtf8();
    const auto *option = config_FindConfig(name.constData());
    if (!option) return {};
    const QString title = qfut(option->psz_text ? option->psz_text : option->psz_name);
    if (option->i_type == CONFIG_ITEM_DIRECTORY)
        return QFileDialog::getExistingDirectory(nullptr, title, current);
    if (option->i_type == CONFIG_ITEM_SAVEFILE)
        return QFileDialog::getSaveFileName(nullptr, title, current);
    if (option->i_type == CONFIG_ITEM_LOADFILE)
        return QFileDialog::getOpenFileName(nullptr, title, current);
    return {};
}

QString MainCtx::pickRedesignColor(const QString &current)
{
    const QColor picked = QColorDialog::getColor(QColor(current), nullptr, qtr("Text color"));
    return picked.isValid() ? picked.name(QColor::HexRgb) : QString{};
}

void MainCtx::setAttachedToolTip(QObject *toolTip)
{
    // See QQuickToolTipAttachedPrivate::instance(bool create)
    assert(toolTip);

    // Prevent possible invalid down-casting:
    assert(toolTip->inherits("QQuickToolTip"));

    QQmlEngine* const engine = qmlEngine(toolTip);
    assert(engine);
    assert(engine->objectOwnership(toolTip) == QQmlEngine::ObjectOwnership::JavaScriptOwnership);

    // Dynamic internal property:
#if QT_VERSION < QT_VERSION_CHECK(6, 12, 0)
    static const char* const name = "_q_QQuickToolTip";
#else
    static const char* const name = "_q_shared_QQuickToolTip";
#endif

    if (const auto obj = engine->property(name).value<QObject *>())
    {
        if (engine->objectOwnership(obj) == QQmlEngine::ObjectOwnership::CppOwnership)
            obj->deleteLater();
    }

    // setProperty() will return false, so there is no
    // need to check the return value:
    engine->setProperty(name, QVariant::fromValue(toolTip));

    // Check if the attached tooltip is actually the
    // one that is set
#ifndef NDEBUG
    QMetaObject::invokeMethod(toolTip, [toolTip]() {
        const auto engine = qmlEngine(toolTip);
        if (Q_UNLIKELY(!engine)) // Very unlikely, if not impossible
            return;
        QQmlComponent component(engine);
        component.setData(QByteArrayLiteral("import QtQuick; import QtQuick.Templates as T; Item { }"), {});
        QObject* const obj = component.create();
        assert(obj);
        // Consider disabling setting of custom attached
        // tooltip if the following assertion fails:
        if (QQmlProperty::read(obj, QStringLiteral("T.ToolTip.toolTip"), qmlContext(obj)).value<QObject*>() != toolTip)
            qmlWarning(obj) << "Could not set self as custom ToolTip!";
        obj->deleteLater();
    }, Qt::QueuedConnection);
#endif
}

MainCtx::MainInterfaceMode MainCtx::getEffectiveMainInterfaceMode() const {
    //priority applies across modes
    if (m_mainInterfaceModes & MainCtx::MAININTERFACE_MODE_MINIMAL)
        return MAININTERFACE_MODE_MINIMAL;
    else if (m_mainInterfaceModes & MAININTERFACE_MODE_PLAYER)
        return MAININTERFACE_MODE_PLAYER;
    else if (m_mainInterfaceModes & MAININTERFACE_MODE_MAINDISPLAY)
        return MAININTERFACE_MODE_MAINDISPLAY;
    else
        return MAININTERFACE_MODE_INVALID;
}

double MainCtx::dp(const double px, const double scale)
{
    return std::round(px * scale);
}

double MainCtx::dp(const double px) const
{
    return dp(px, m_intfScaleFactor);
}

bool MainCtx::useXmasCone() const
{
    return (QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY)
            && var_InheritBool( p_intf, "qt-icon-change" );
}

bool WindowStateHolder::holdFullscreen(QWindow *window, Source source, bool hold)
{
    QVariant prop = window->property("__windowFullScreen");
    bool ok = false;
    unsigned fullscreenCounter = prop.toUInt(&ok);
    if (!ok)
        fullscreenCounter = 0;

    if (hold)
        fullscreenCounter |= source;
    else
        fullscreenCounter &= ~source;

    Qt::WindowStates oldflags = window->windowStates();
    Qt::WindowStates newflags;

    if( fullscreenCounter != 0 )
        newflags = oldflags | Qt::WindowFullScreen;
    else
        newflags = oldflags & ~Qt::WindowFullScreen;

    window->setProperty("__windowFullScreen", QVariant::fromValue(fullscreenCounter));

    if( newflags != oldflags )
    {
        window->setWindowStates( newflags );
    }

    return fullscreenCounter != 0;
}

bool WindowStateHolder::holdOnTop(QWindow *window, Source source, bool hold)
{
    QVariant prop = window->property("__windowOnTop");
    bool ok = false;
    unsigned onTopCounter = prop.toUInt(&ok);
    if (!ok)
        onTopCounter = 0;

    if (hold)
        onTopCounter |= source;
    else
        onTopCounter &= ~source;

#ifdef _WIN32
    if (!window->handle()) // do not call hold on top if there is no platform window
        return false;
#endif

    if( onTopCounter != 0 )
    {
#ifdef _WIN32
        // Qt::WindowStaysOnTopHint is broken on Windows: QTBUG-133034, QTBUG-132522.
        // Reported to be fixed by Qt 6.9, but needs additional checking if the solution does not interfere with `CSDWin32EventHandler`.
        SetWindowPos(reinterpret_cast<HWND>(window->winId()), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#else
        window->setFlag(Qt::WindowStaysOnTopHint, true);
#endif
    }
    else
    {
#ifdef _WIN32
        // Qt::WindowStaysOnTopHint is broken on Windows: QTBUG-133034, QTBUG-132522.
        // Reported to be fixed by Qt 6.9, but needs additional checking if the solution does not interfere with `CSDWin32EventHandler`.
        SetWindowPos(reinterpret_cast<HWND>(window->winId()), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#else
        window->setFlag(Qt::WindowStaysOnTopHint, false);
#endif
    }

    window->setProperty("__windowOnTop", QVariant::fromValue(onTopCounter));

    return onTopCounter != 0;
}

double MainCtx::artistAlbumsWidth() const
{
    return dp(m_artistAlbumsWidth);
}

void MainCtx::setArtistAlbumsWidth(double newArtistAlbumsWidth)
{
    double unscaledValue = newArtistAlbumsWidth / getIntfScaleFactor();
    if (qFuzzyCompare(m_artistAlbumsWidth, unscaledValue))
        return;

    m_artistAlbumsWidth = unscaledValue;
    emit artistAlbumsWidthChanged( dp(m_artistAlbumsWidth) );
}

#ifdef UPDATE_CHECK
UpdateModel* MainCtx::getUpdateModel() const
{
    if (!m_updateModel)
    {
        m_updateModel = std::make_unique<UpdateModel>(p_intf);

        connect(m_updateModel.get(), &UpdateModel::updateStatusChanged, this, [this](){
            // No-op, modern update pane opens itself automatically when status changes.
            // However, the old update dialog is used as fallback in case for any reason
            // the modern pane is not available.

            const auto openLegacyDialogIfNecessary = [&]() {
                // Fallback:

                if (Q_LIKELY(p_intf &&
                             p_intf->p_compositor &&
                             p_intf->p_compositor->quickWindow() &&
                             p_intf->p_compositor->quickWindow()->contentItem()))
                {
                    const auto target = p_intf->p_compositor->quickWindow()->contentItem();
                    QMetaObject::invokeMethod(target, [target]() {
                        const auto updatePaneLoader = target->findChild<QQuickItem*>(QStringLiteral("updatePaneLoader"));
                        if (!updatePaneLoader || (updatePaneLoader->property("status").toInt() != 1 /* Loader.Ready */))
                        {
                            qDebug("No acknowledgement from the modern update loader, falling back to the old update dialog.");
                            THEDP->updateDialog(DialogsProvider::Show);
                        }
                    }, Qt::QueuedConnection);
                }
                else
                {
                    qDebug("Compositor is not available, falling back to the old update dialog.");
                    THEDP->updateDialog();
                }
            };

            if (m_updateModel->explicitCheck())
            {
                if (m_updateModel->updateStatus() != UpdateModel::Unchecked)
                    openLegacyDialogIfNecessary();
            }
            else
            {
                switch (m_updateModel->updateStatus())
                {
                case UpdateModel::UpToDate:
                case UpdateModel::Unchecked:
                case UpdateModel::Checking:
                    break;
                case UpdateModel::NeedUpdate:
                    qWarning() << "Need update";
                    [[fallthrough]];
                default:
                    openLegacyDialogIfNecessary();
                }
            }
        });
    }

    return m_updateModel.get();
}
#endif
