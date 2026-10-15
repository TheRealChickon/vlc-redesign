// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import VLC.MediaLibrary
import VLC.MainInterface
import VLC.Widgets as Widgets

Widgets.PageExt {
    id: root
    objectName: "redesignFavoritesView"
    title: qsTr("Favorites")
    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0
    hasGridListMode: true
    isSearchable: true
    contentItem: MediaView {
        focus: true
        displayMarginBeginning: root.displayMarginBeginning
        displayMarginEnd: root.displayMarginEnd
        model: MLMediaModel {
            ml: MediaLib
            favoriteOnly: true
            sortCriteria: root.sort.criteria || "insertion"
            sortOrder: root.sort.order
            searchPattern: root.search.pattern
        }
    }
}
