/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

AbstractButton {
    id: artworkInfoItem

    property bool paintOnly: false

    readonly property real minimumWidth: coverRect.implicitWidth +
                                         + (leftPadding + rightPadding)

    property int maximumHeight: _preferredHeight

    readonly property int preferredWidth: minimumWidth + contentItem.spacing * 2
                                          +
                                          Math.max(titleLabel.implicitWidth,
                                                   artistLabel.implicitWidth,
                                                   progressIndicator.implicitWidth)

    property int _preferredHeight: VLCStyle.dp(60, VLCStyle.scale)

    property bool _keyPressed: false

    text: I18n.qtr("Open player")

    padding: VLCStyle.focus_border

    Keys.onPressed: {
        if (KeyHelper.matchOk(event)) {
            event.accepted = true

            _keyPressed = true
        } else {
            Navigation.defaultKeyAction(event)
        }
    }

    Keys.onReleased: {
        if (_keyPressed === false)
            return

        _keyPressed = false

        if (KeyHelper.matchOk(event)) {
            event.accepted = true

            g_mainDisplay.showPlayer()
        }
    }

    onClicked: g_mainDisplay.showPlayer()

    Accessible.onPressAction: artworkInfoItem.clicked()

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton
        focused: artworkInfoItem.visualFocus
        hovered: artworkInfoItem.hovered
    }

    background: Widgets.AnimatedBackground {
        active: visualFocus
        animate: theme.initialized
        activeBorderColor: theme.visualFocus
    }

    contentItem: RowLayout {
        spacing: infoColumn.visible ? VLCStyle.margin_xsmall : 0

        Rectangle {
            id: coverRect

            implicitWidth: implicitHeight

            implicitHeight: Math.min(artworkInfoItem._preferredHeight,
                                     artworkInfoItem.maximumHeight)

            color: theme.bg.primary

            Widgets.DoubleShadow {
                anchors.fill: parent

                primaryBlurRadius: VLCStyle.dp(3, VLCStyle.scale)
                primaryVerticalOffset: VLCStyle.dp(1, VLCStyle.scale)

                secondaryBlurRadius: VLCStyle.dp(14, VLCStyle.scale)
                secondaryVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
            }

            Widgets.ScaledImage {
                id: cover

                anchors.fill: parent

                source: {
                    if (!paintOnly
                        && Player.artwork
                        && Player.artwork.toString())
                        Player.artwork
                    else
                        VLCStyle.noArtAlbumCover
                }

                fillMode: Image.PreserveAspectFit

                asynchronous: true

                Accessible.role: Accessible.Graphic
                Accessible.name: I18n.qtr("Cover")

                ToolTip.visible: infoColumn.width < infoColumn.implicitWidth
                                 && (artworkInfoItem.hovered || artworkInfoItem.visualFocus)
                ToolTip.delay: VLCStyle.delayToolTipAppear
                ToolTip.text: I18n.qtr("%1\n%2\n%3").arg(titleLabel.text)
                                                    .arg(artistLabel.text)
                                                    .arg(progressIndicator.text)
            }
        }

        ColumnLayout {
            id: infoColumn

            Layout.fillWidth: true
            Layout.preferredHeight: coverRect.height
            Layout.minimumWidth: 0.1 // FIXME: Qt layout bug

            Widgets.MenuLabel {
                id: titleLabel

                Layout.fillWidth: true
                Layout.fillHeight: true

                text: {
                    if (paintOnly)
                        I18n.qtr("Title")
                    else
                        Player.title
                }
                color: theme.fg.primary
            }

            Widgets.MenuCaption {
                id: artistLabel

                Layout.fillWidth: true
                Layout.fillHeight: true

                text: {
                    if (paintOnly)
                        I18n.qtr("Artist")
                    else
                        Player.artist
                }
                color: theme.fg.secondary
            }

            Widgets.MenuCaption {
                id: progressIndicator

                Layout.fillWidth: true
                Layout.fillHeight: true

                visible: (infoColumn.height >= artworkInfoItem._preferredHeight)

                text: {
                    if (paintOnly)
                        " -- / -- "
                    else
                        Player.time.formatHMS() + " / " + Player.length.formatHMS()
                }
                color: theme.fg.secondary
            }
        }
    }
}
