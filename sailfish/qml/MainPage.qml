/*
 * Copyright (C) 2018 Alexander Seibel.
 * Copyright (c) 2014 Dickson Leong.
 * All rights reserved.
 *
 * This file is part of GagBook.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import QtQuick 2.2
import Sailfish.Silica 1.0
import harbour.gagbook.Core 1.0

Page {
    id: mainPage
    objectName: "mainPage"

    readonly property alias busy: votingManager.busy

    // for access by cover
    property alias gagModel: gagModel
    property int currentIndex: 0

    function positionAtIndex(index) {
        gagListView.positionViewAtIndex(index, ListView.Beginning);
        currentIndex = index;
    }

    property bool __pushedAttached: false
    onStatusChanged: {
        if (status == PageStatus.Active && !__pushedAttached) {
            pageStack.pushAttached(Qt.resolvedUrl("SectionPage.qml"), { gagModel: gagModel });
            __pushedAttached = true;
        }
    }

    property bool newCommentsIconExists

    Image {
        /* TODO Remove this later if the minimal required SFOS version has been increased to >= 2.2.0
         *
         * This checks for the existence of the new icon. This is a workaround to resolve the icon URL
         * and to prevent errors being printed by loading the (non existent) icon in each GagDelegate.
         */
        opacity: 0
        visible: false
        enabled: false
        source: "image://theme/icon-m-outline-chat" // this icon is available since SFOS 2.2.0.29
        onStatusChanged: {
            newCommentsIconExists = (status === Image.Ready)
        }
    }

    SilicaListView {
        id: gagListView
        anchors.fill: parent
        model: gagModel
        orientation: ListView.Vertical
        spacing: constant.paddingMedium

        PullDownMenu {
            MenuItem {
                text: "About GagBook"
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"));
            }
            MenuItem {
                text: "Settings"
                onClicked: pageStack.push(Qt.resolvedUrl("AppSettingsPage.qml"));
            }
            MenuItem {
                text: "Refresh"
                onClicked: gagModel.refresh(GagModel.RefreshAll);
            }
        }

        header: PageHeader { title: "/" + appSettings.sections.getSection(gagModel.selectedSection).name }

        delegate: GagDelegate { newCommentsIconExists: mainPage.newCommentsIconExists }

        footer: Item {
            width: parent.width
            height: footerLoader.status == Loader.Ready ? footerLoader.height + 2 * footerLoader.anchors.margins : 0

            // show downloading indicator at the end of the list
            Loader {
                id: footerLoader
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: constant.paddingMedium  }
                sourceComponent: gagModel.busy && gagListView.count > 0 ? downloadingIndicator : undefined
            }
        }

        // show downloading indicator when the list is empty
        Loader {
            anchors {
                left: parent.left; right: parent.right
                verticalCenter: parent.verticalCenter
                margins: constant.paddingMedium
            }
            sourceComponent: gagModel.busy && gagListView.count == 0 ? downloadingIndicator : undefined

            Component {
                id: downloadingIndicator

                Column {
                    Text {
                        anchors { left: parent.left; right: parent.right }
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        font.pixelSize: constant.fontSizeMedium
                        color: constant.colorLight
                        text: "Downloading..."
                    }

                    ProgressBar {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width * 0.75
                        value: gagModel.progress
                        indeterminate: gagModel.progress == 0
                    }
                }
            }
        }

        ViewPlaceholder {
            id: placeHolder
            text: "Couldn't get any gags"
            enabled: gagListView.count == 0 && !gagModel.busy
            hintText: qsTr("Check your WiFi or 3G settings and try refreshing from the menu.")
        }

        // ToDo: 'refresh' will be executed twice if 'failure' is emitted in GagRequest (only on specific list scroll behaviour)
        onAtYEndChanged: if (atYEnd && !gagModel.busy && count > 0) gagModel.refresh(GagModel.RefreshOlder)
        onMovementEnded: mainPage.currentIndex = gagListView.indexAt(0, gagListView.contentY + gagListView.height / 2);
    }

    GagModel {
        id: gagModel
        manager: gagbookManager
        onRefreshFailure: infoBanner.alert(errorMessage);
    }

    VotingManager {
        id: votingManager
        manager: gagbookManager
        onVoteSuccess: gagModel.changeLikes(id, likes);
        onFailure: infoBanner.alert(errorMessage);
    }

    // reload the posts with the new session, so votes and sensitive posts match the account
    Connections {
        target: appSettings
        onLoggedInChanged: gagModel.refresh(GagModel.RefreshAll)
    }

    Connections {
        target: volumeKeyListener
        onVolumeUpClicked: {
            if (currentIndex > 0)
                positionAtIndex(currentIndex - 1);
            else if (currentIndex == 0 && !gagListView.atYBeginning)
                gagListView.positionViewAtBeginning();
        }
        onVolumeDownClicked: {
            if (currentIndex < gagListView.count - 1)
                positionAtIndex(currentIndex + 1);
            else if (currentIndex == gagListView.count - 1 && !gagListView.atYEnd)
                gagListView.positionViewAtEnd();
        }
    }

    Binding {
        target: volumeKeyListener
        property: "enabled"
        value: appSettings.scrollWithVolumeKeys && Qt.application.active && mainPage.status == PageStatus.Active
    }
}
