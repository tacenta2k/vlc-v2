import QtQuick

import VLC.MainInterface
import VLC.Style
import VLC.Util
import VLC.Menus

NavigableRow {
    id: root

    spacing: VLCStyle.margin_normal
    enabled: gridListBtn.visible || sortControl.visible || searchField.visible

    property SortMenu sortMenu: null
    required property SortCtx sort
    required property SearchCtx search


    //TODO: visible value of root.sort.available and MainCtx.hasGridListMode is initialised correctly but on first load still shows up
    SortControl {
        id: sortControl

        visible: root.sort.available
        enabled: visible
        width: VLCStyle.bannerButton_width
        height: VLCStyle.bannerButton_height
        font.pixelSize: VLCStyle.icon_banner
        description: qsTr("Sort")

        model: root.sort.model

        menu: root.sortMenu

        sortKey:  root.sort.criteria
        sortOrder: root.sort.order

        onSortSelected: (key) => {
            root.sort.criteria = key
        }
        onSortOrderSelected: (type) => {
            root.sort.order = type
        }
    }

    IconToolButton {
        id: gridListBtn

        visible: MainCtx.hasGridListMode

        enabled: visible
        width: VLCStyle.bannerButton_width
        height: VLCStyle.bannerButton_height
        font.pixelSize: VLCStyle.icon_banner
        text: MainCtx.gridView ? VLCIcons.list : VLCIcons.grid
        description: qsTr("List/Grid")
        onClicked: MainCtx.gridView = !MainCtx.gridView
    }

    TextFieldExt {
        id: searchField

        visible: root.search.available
        placeholderText: qsTr("Search")
        selectByMouse: true
        color: VLCStyle.libraryTextPrimary
        placeholderTextColor: VLCStyle.libraryTextSecondary
        radius: VLCStyle.dp(6, VLCStyle.scale)
        leftPadding: VLCStyle.dp(30, VLCStyle.scale)
        font.pixelSize: VLCStyle.fontSize_normal

        Layout.minimumWidth: VLCStyle.dp(145, VLCStyle.scale)
        Layout.preferredWidth: VLCStyle.dp(245, VLCStyle.scale)
        Layout.maximumWidth: VLCStyle.dp(310, VLCStyle.scale)
        Layout.preferredHeight: VLCStyle.dp(30, VLCStyle.scale)

        onTextEdited: root.search.pattern = text

        Binding {
            target: searchField
            property: "text"
            value: root.search.pattern
            when: !searchField.activeFocus
        }

        Connections {
            target: root.search
            function onAskShow() {
                searchField.forceActiveFocus(Qt.ShortcutFocusReason)
            }
        }

        background: Item {
            implicitWidth: VLCStyle.dp(245, VLCStyle.scale)
            implicitHeight: VLCStyle.dp(30, VLCStyle.scale)

            Rectangle {
                anchors.fill: parent
                color: VLCStyle.librarySearchBackground
                radius: searchField.radius
                border.width: searchField.activeFocus ? VLCStyle.border : 0
                border.color: VLCStyle.playerAccent
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: VLCStyle.dp(10, VLCStyle.scale)
                anchors.verticalCenter: parent.verticalCenter
                text: VLCIcons.search
                color: VLCStyle.libraryTextSecondary
                font.family: VLCIcons.fontFamily
                font.pixelSize: VLCStyle.icon_small
            }
        }
    }

}
