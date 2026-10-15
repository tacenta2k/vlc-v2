/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Style
import VLC.Menus
import VLC.Dialogs
import VLC.Util

T.Page {
    id: root

    //path of the current page loader
    property var pagePrefix: []

    //note that T.Page introduce an intermediate Item, so to access our delegate,
    //we need to use contentChildren[0] rather than contentItem
    readonly property Item _firstChild: contentChildren[0] ?? null

    //indicates whether the subview support grid/list mode
    property bool hasGridListMode: _firstChild?.hasGridListMode ?? false

    property bool isSearchable: _firstChild?.isSearchable ?? false

    property var sortModel: _firstChild?.sortModel ?? null

    //property is *not* readOnly, a PageLoader may define a localMenuDelegate common for its subviews (music, video)
    property SortMenu sortMenu: null

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    readonly property SearchCtx search: _searchCtx
    readonly property SortCtx sort: _sortCtx

    // Number of rows in the active native media model, when this page exposes one.
    readonly property int mediaCount: {
        const count = _firstChild?.model?.count
        return (typeof count === "number" && Number.isFinite(count)) ? count : -1
    }

    // Translate VLC's route path into user-facing breadcrumb labels.
    // Group and folder titles come from the current native navigation context.
    readonly property var breadcrumbItems: {
        const path = History.viewPath ?? []
        const labels = {
            "home": qsTr("Home"),
            "video": qsTr("Videos"),
            "music": qsTr("Music"),
            "network": qsTr("Network"),
            "discover": qsTr("Discover"),
            "playlists": qsTr("Playlists"),
            "artists": qsTr("Artists"),
            "albums": qsTr("Albums"),
            "tracks": qsTr("Tracks"),
            "genres": qsTr("Genres"),
            "services": qsTr("Services"),
            "url": qsTr("URL")
        }
        const items = []

        for (let i = 0; i < path.length; ++i) {
            const key = String(path[i])

            // This route names an implementation page, not a user-facing crumb.
            if (key === "all")
                continue

            if (key === "group" || key === "folder") {
                if (i === path.length - 1) {
                    const currentTitle = History.viewProp?.title
                    if (currentTitle !== undefined && String(currentTitle).length > 0) {
                        items.push({
                            label: String(currentTitle),
                            path: path.slice(0, i + 1),
                            current: true
                        })
                    }
                }
                continue
            }

            const label = labels[key]
            if (!label)
                continue

            items.push({
                label: label,
                path: path.slice(0, i + 1),
                current: false
            })
        }

        if (items.length === 0) {
            items.push({ label: root.title || qsTr("VLC"), path: [], current: true })
        } else {
            items[items.length - 1].current = true
        }

        return items
    }


    SearchCtx {
        id: _searchCtx
        available: root.isSearchable
    }

    SortCtx {
        id: _sortCtx

        readonly property string _sortCriteriaKey: ["sortCriteria", ...root.pagePrefix].join("/")
        readonly property string _sortOrderKey:  ["sortOrder", ...root.pagePrefix].join("/")

        available: Helpers.isArray(root.sortModel) && (root.sortModel).length > 0
        model: root.sortModel

        //save & restore setting of the page
        Component.onCompleted: {
            const criteria = MainCtx.settingValue(_sortCriteriaKey, undefined)
            if (criteria !== undefined)
                _sortCtx.criteria = criteria

            const order = MainCtx.settingValue(_sortOrderKey, undefined)
            if (order !== undefined)
                _sortCtx.order = parseInt(order)
        }

        Component.onDestruction: {
            MainCtx.setSettingValue(_sortCriteriaKey, _sortCtx.criteria)
            MainCtx.setSettingValue(_sortOrderKey, _sortCtx.order)
        }
    }

    Binding {
        target: MainCtx
        property: "hasGridListMode";
        value: root.hasGridListMode
    }

    on_FirstChildChanged: {
        if (!_firstChild)
            return
        _firstChild.Navigation.parentItem = root
        _firstChild.Navigation.upItem = root.header
    }

    function positionContentAtBeginning() {
        _firstChild?.positionContentAtBeginning?.()
    }

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

    header: DefaultPageHeader {
        text: root.title

        leftPadding: VLCStyle.dynamicAppMargins(width) + VLCStyle.layout_left_margin + root.leftPadding
        rightPadding: VLCStyle.dynamicAppMargins(width) + VLCStyle.layout_right_margin + root.rightPadding

        sortMenu: root.sortMenu

        search: _searchCtx
        sort: _sortCtx

        Navigation.parentItem: root
        Navigation.downAction: function() {
            if (typeof root.setCurrentItemFocus === "function")
                root.setCurrentItemFocus(Qt.TabFocusReason)
            else if (typeof root._firstChild?.setCurrentItemFocus === "function")
             root._firstChild.setCurrentItemFocus(Qt.TabFocusReason)
            else if (root._firstChild)
                root._firstChild.forceActiveFocus(Qt.TabFocusReason)
        }
    }

    component DefaultPageHeader: T.ToolBar {
        id: defaultPageHeader

        // Properties
        property alias text: label.text

        property alias sortMenu: gridSortFilter.sortMenu

        property alias search: gridSortFilter.search
        property alias sort: gridSortFilter.sort

        position: T.ToolBar.Header

        topPadding: VLCStyle.margin_xsmall
        bottomPadding: VLCStyle.margin_xsmall

        // FIXME: `GridItem`'s background extends beyond its
        //        bounding rect, violating the hypothetical
        //        clip test (see 7e6b23db).
        bottomInset: (MainCtx.hasGridListMode && MainCtx.gridView) ? VLCStyle.gridItemSelectedBorder : undefined

        // Settings

        implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                implicitContentWidth + leftPadding + rightPadding)
        implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                 implicitContentHeight + topPadding + bottomPadding)

        Navigation.navigable: gridSortFilter.enabled

        // Children

        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: ColorContext.View
        }

        background: Rectangle {
            color: VLCStyle.libraryBackground

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: VLCStyle.border
                color: VLCStyle.libraryDivider
            }
        }

        RowLayout {
            id: row

            anchors.fill: parent
            spacing: VLCStyle.margin_xxsmall

            RowLayout {
                id: breadcrumbRow

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: VLCStyle.margin_xxsmall

                IconToolButton {
                    text: VLCIcons.back
                    description: qsTr("Back")
                    color: VLCStyle.libraryTextPrimary
                    backgroundColor: VLCStyle.libraryBackground
                    font.pixelSize: VLCStyle.icon_normal
                    Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)
                    enabled: !History.previousEmpty
                    onClicked: History.previous(Qt.MouseFocusReason)
                }

                // Forward navigation is wired to the native navigation history.
                IconToolButton {
                    text: VLCIcons.next
                    description: qsTr("Forward")
                    color: VLCStyle.libraryTextSecondary
                    backgroundColor: VLCStyle.libraryBackground
                    font.pixelSize: VLCStyle.icon_normal
                    Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)
                    enabled: !History.nextEmpty
                    onClicked: History.next(Qt.MouseFocusReason)
                }

                Repeater {
                    model: root.breadcrumbItems

                    delegate: RowLayout {
                        required property int index
                        required property var modelData

                        property var crumbData: modelData
                        spacing: VLCStyle.margin_xxsmall

                        T.Label {
                            visible: index > 0
                            text: "›"
                            color: VLCStyle.libraryTextSecondary
                            font.pixelSize: VLCStyle.fontSize_large
                            verticalAlignment: Text.AlignVCenter
                        }

                        T.ToolButton {
                            id: crumbButton

                            text: crumbData.label
                            enabled: !crumbData.current && crumbData.path.length > 0
                            padding: 0
                            Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)
                            Layout.preferredWidth: crumbLabel.implicitWidth

                            Accessible.name: crumbData.label
                            onClicked: History.push(crumbData.path, Qt.MouseFocusReason)

                            contentItem: T.Label {
                                id: crumbLabel
                                text: crumbButton.text
                                color: crumbButton.enabled ? VLCStyle.libraryTextPrimary
                                                           : VLCStyle.libraryTextActive
                                font.pixelSize: VLCStyle.fontSize_normal
                                font.weight: crumbButton.enabled ? Font.Normal : Font.DemiBold
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            background: Item {}
                        }
                    }
                }
            }

            T.Label {
                id: itemCountLabel

                visible: root.mediaCount >= 0
                text: visible ? qsTr("%1 items").arg(root.mediaCount) : ""
                color: VLCStyle.libraryTextSecondary
                font.pixelSize: VLCStyle.fontSize_small
                verticalAlignment: Text.AlignVCenter
            }

            GridSortFilterControls {
                id: gridSortFilter
                focus: true
                sort: _sortCtx
                search: _searchCtx
                Navigation.parentItem: defaultPageHeader
            }

            IconToolButton {
                id: addMediaButton

                text: VLCIcons.add
                description: qsTr("Add media")
                color: VLCStyle.libraryTextPrimary
                backgroundColor: VLCStyle.libraryBackground
                font.pixelSize: VLCStyle.icon_normal
                Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)

                onClicked: DialogsProvider.simpleOpenDialog()
            }

            IconToolButton {
                id: overflowButton

                text: VLCIcons.more
                description: qsTr("More options")
                color: VLCStyle.libraryTextPrimary
                backgroundColor: VLCStyle.libraryBackground
                font.pixelSize: VLCStyle.icon_normal
                Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)

                onClicked: globalMenu.popup(this.mapToGlobal(0, height))

                QmlGlobalMenu {
                    id: globalMenu
                    ctx: MainCtx
                }
            }
        }
    }
}
