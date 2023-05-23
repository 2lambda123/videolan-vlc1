/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * Author: Prince Gupta <guptaprince8832@gmail.com>
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

import QtQuick 2.12
import QtQuick.Templates 2.12 as T
import QtQml.Models 2.12

import QtGraphicalEffects 1.12

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///playlist/" as Playlist
import "qrc:///util/Helpers.js" as Helpers

Item {
    id: dragItem

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    readonly property int coverSize: VLCStyle.icon_normal

    property var indexes: []

    // data from last setData
    readonly property alias indexesData: dragItem._data

    property string defaultCover: VLCStyle.noArtAlbumCover

    property string defaultText: I18n.qtr("Unknown")

    // function(index, data) - returns cover for the index in the model in the form {artwork: <string> (file-name), cover: <component>}
    property var coverProvider: null

    // string => role
    property string coverRole: "cover"

    // function(index, data) - returns title text for the index in the model i.e <string> title
    property var titleProvider: null

    // string => role
    property string titleRole: "title"

    readonly property var inputItems: _inputItems

    function coversXPos(index) {
        return VLCStyle.margin_small + (coverSize / 3) * index;
    }

    signal requestData(var identifier)

    function getSelectedInputItem(cb) {
        console.assert(false, "getSelectedInputItem is not implemented.")

        return undefined
    }

    function setData(id, data) {
        if (id !== dragItem._currentRequest)
            return

        console.assert(data.length === indexes.length)
        _data = data

        if (!dragItem._active)
            return

        Qt.callLater(dragItem.getSelectedInputItem, dragItem.setInputItems)

        const covers = []
        const titleList = []

        for (let i in indexes) {
            if (covers.length === _maxCovers)
                break

            const cover = _getCover(indexes[i], data[i])
            const itemTitle = _getTitle(indexes[i], data[i])
            if (!cover || !itemTitle) continue

            covers.push(cover)
            titleList.push(itemTitle)
        }

        if (covers.length === 0)
            covers.push({artwork: dragItem.defaultCover})

        if (titleList.length === 0)
            titleList.push(defaultText)

        _covers = covers
        _title = titleList.join(",") + (indexes.length > _maxCovers ? "..." : "")
    }

    function setInputItems(inputItems) {
        if (!Array.isArray(inputItems) || inputItems.length === 0) {
            console.warn("can't convert items to input items");
            dragItem._inputItems = null
            return
        }

        dragItem._inputItems = inputItems
    }

    //---------------------------------------------------------------------------------------------
    // Private

    readonly property int _maxCovers: 3

    readonly property bool _active: Drag.active

    readonly property int _indexesSize: !!indexes ? indexes.length : 0

    readonly property int _displayedCoversCount: Math.min(_indexesSize, _maxCovers + 1)

    property var _inputItems

    property var _data: []

    property var _covers: []

    property string _title: ""

    property int _currentRequest: 0

    Drag.onActiveChanged: {
        // FIXME: This should not be ideally necessary
        // TODO: Rework D&D positioning
        if (!Drag.active)
            x = y = -1

        if (!Drag.active)
            dragItem._inputItems = undefined
    }

    //---------------------------------------------------------------------------------------------
    // Implementation
    //---------------------------------------------------------------------------------------------

    parent: g_mainDisplay

    width: VLCStyle.colWidth(2)

    height: coverSize + VLCStyle.margin_small * 2

    opacity: visible ? 0.90 : 0

    visible: Drag.active
    enabled: visible

    function _getCover(index, data) {
        console.assert(dragItem.coverRole)
        if (!!dragItem.coverProvider)
            return dragItem.coverProvider(index, data)
        else
            return {artwork: data[dragItem.coverRole] || dragItem.defaultCover}
    }

    function _getTitle(index, data) {
        console.assert(dragItem.titleRole)
        if (!!dragItem.titleProvider)
            return dragItem.titleProvider(index, data)
        else
            return data[dragItem.titleRole] || dragItem.defaultText
    }

    //NoRole because I'm not sure we need this to be accessible
    //can drag items be considered Tooltip ? or is another class better suited
    Accessible.role: Accessible.NoRole
    Accessible.name: I18n.qtr("drag item")

    on_ActiveChanged: {
        if (_active) {

            // reset any data from previous drags before requesting new data,
            // so that we don't show invalid data while data is being requested
            _title = ""
            _covers = []
            _data = []

            dragItem._currentRequest += 1
            dragItem.requestData(dragItem._currentRequest)

            MainCtx.setCursor(Qt.DragMoveCursor)
        } else {
            MainCtx.restoreCursor()
        }
    }

    Behavior on opacity {
        NumberAnimation {
            easing.type: Easing.InOutSine
            duration: VLCStyle.duration_short
        }
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Rectangle {
        /* background */
        anchors.fill: parent
        color: theme.bg.primary
        border.color: theme.border
        border.width: VLCStyle.dp(1, VLCStyle.scale)
        radius: VLCStyle.dp(6, VLCStyle.scale)
    }

    RectangularGlow {
        anchors.fill: parent
        glowRadius: VLCStyle.dp(8, VLCStyle.scale)
        color: theme.shadow
        spread: 0.2
        z: -1
    }

    Repeater {
        id: coverRepeater

        model: dragItem._covers

        Item {
            x: dragItem.coversXPos(index)
            y: (dragItem.height - height) / 2
            width: dragItem.coverSize
            height: dragItem.coverSize

            Rectangle {
                id: bg

                radius: coverRepeater.count > 1 ? dragItem.coverSize : VLCStyle.dp(2, VLCStyle.scale)
                anchors.fill: parent
                color: theme.bg.primary

                DoubleShadow {
                    anchors.fill: parent

                    z: -1

                    xRadius: bg.radius
                    yRadius: bg.radius

                    primaryBlurRadius: VLCStyle.dp(3)
                    primaryVerticalOffset: VLCStyle.dp(1, VLCStyle.scale)
                    primaryHorizontalOffset: 0

                    secondaryBlurRadius: VLCStyle.dp(14)
                    secondaryVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
                    secondaryHorizontalOffset: 0
                }
            }


            Loader {
                // parent may provide extra data with covers
                property var model: modelData

                anchors.centerIn: parent
                sourceComponent: (!modelData.artwork || modelData.artwork.toString() === "") ? modelData.cover : artworkLoader
                layer.enabled: true
                layer.effect: OpacityMask {
                    maskSource: Rectangle {
                        width: bg.width
                        height: bg.height
                        radius: bg.radius
                        visible: false
                    }
                }

                onItemChanged: {
                    if (modelData.artwork && modelData.artwork.toString() !== "")
                        item.source = modelData.artwork
                }
            }

            Rectangle {
                // for cover border
                color: "transparent"
                border.width: VLCStyle.dp(1, VLCStyle.scale)
                border.color: theme.border
                anchors.fill: parent
                radius: bg.radius
            }
        }
    }

    Rectangle {
        id: extraCovers

        x: dragItem.coversXPos(_maxCovers)
        y: (dragItem.height - height) / 2
        width: dragItem.coverSize
        height: dragItem.coverSize
        radius: dragItem.coverSize
        visible: dragItem._indexesSize > dragItem._maxCovers
        color: theme.bg.secondary
        border.width: VLCStyle.dp(1, VLCStyle.scale)
        border.color: theme.border

        MenuLabel {
            anchors.centerIn: parent
            color: theme.accent
            text: "+" + (dragItem._indexesSize - dragItem._maxCovers)
        }

        DoubleShadow {
            z: -1
            anchors.fill: parent
            xRadius: extraCovers.radius
            yRadius: extraCovers.radius

            primaryBlurRadius: VLCStyle.dp(3)
            primaryVerticalOffset: VLCStyle.dp(1, VLCStyle.scale)
            primaryHorizontalOffset: 0

            secondaryBlurRadius: VLCStyle.dp(14)
            secondaryVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
            secondaryHorizontalOffset: 0
        }
    }


    Column {
        id: labelColumn

        anchors.verticalCenter: parent.verticalCenter
        x: dragItem.coversXPos(_displayedCoversCount - 1) + dragItem.coverSize + VLCStyle.margin_small
        width: parent.width - x - VLCStyle.margin_small
        spacing: VLCStyle.margin_xxxsmall

        ScrollingText {
            label: titleLabel
            height: VLCStyle.fontHeight_large
            width: parent.width

            clip: scrolling
            forceScroll: dragItem.visible
            hoverScroll: false

            T.Label {
                id: titleLabel

                text: dragItem._title
                visible: text && text !== ""
                font.pixelSize: VLCStyle.fontSize_large
                color: theme.fg.primary
            }
        }

        MenuCaption {
            id: subtitleLabel

            visible: text && text !== ""
            width: parent.width
            text: I18n.qtr("%1 selected").arg(dragItem._indexesSize)
            color: theme.fg.secondary
        }
    }

    Component {
        id: artworkLoader

        ScaledImage {
            fillMode: Image.PreserveAspectCrop
            width: coverSize
            height: coverSize
            asynchronous: true
            cache: false
        }
    }
}
