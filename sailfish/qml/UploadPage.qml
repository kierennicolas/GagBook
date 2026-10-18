/*
 * Copyright (C) 2026 Kieren Nicolas.
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


import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import harbour.gagbook.Core 1.0

// Posts an image from the phone to 9GAG with a title, tags and the sensitive flag.
Page {
    id: uploadPage

    property string filePath: ""
    property string postedUrl: ""
    property bool posted: false
    property string message: ""

    readonly property string problem: uploader.validate(filePath, titleField.text, tagsField.text)

    // leaving the page would stop the upload
    backNavigation: !uploader.busy

    PostUploader {
        id: uploader
        manager: gagbookManager

        onSucceeded: {
            uploadPage.posted = true;
            uploadPage.postedUrl = postUrl;
            uploadPage.message = "";
        }
        onFailed: uploadPage.message = message
    }

    Component {
        id: imagePicker

        ImagePickerPage {
            onSelectedContentPropertiesChanged: {
                uploadPage.filePath = selectedContentProperties.filePath;
                uploadPage.message = "";
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            visible: uploadPage.posted
            MenuItem {
                text: "Post another image"
                onClicked: {
                    uploadPage.posted = false;
                    uploadPage.postedUrl = "";
                    uploadPage.filePath = "";
                    titleField.text = "";
                    tagsField.text = "";
                    sensitiveSwitch.checked = false;
                }
            }
        }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: "Post an image" }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: !appSettings.loggedIn
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: "Log in to 9GAG in Settings to post."
            }

            BackgroundItem {
                id: chooseItem
                width: parent.width
                height: Math.max(Theme.itemSizeLarge, preview.height + 2 * Theme.paddingMedium)
                enabled: !uploader.busy && !uploadPage.posted
                onClicked: pageStack.push(imagePicker)

                Image {
                    id: preview
                    anchors.centerIn: parent
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    height: status === Image.Ready ? Math.min(implicitHeight, Screen.height / 3) : 0
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    sourceSize.width: width
                    source: uploadPage.filePath ? "file://" + uploadPage.filePath : ""
                    visible: status === Image.Ready
                }

                Label {
                    anchors.centerIn: parent
                    visible: !preview.visible
                    color: chooseItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: uploadPage.filePath ? "Can't show the image, tap to choose another" : "Choose an image"
                }
            }

            TextField {
                id: titleField
                width: parent.width
                enabled: !uploader.busy && !uploadPage.posted
                // 9GAG takes 6 to 120 characters (PostUploader::TitleMinLength/TitleMaxLength)
                label: "Title, %1 of 120 characters".arg(text.trim().length)
                placeholderText: "Title"
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: tagsField.forceActiveFocus()
            }

            TextField {
                id: tagsField
                width: parent.width
                enabled: !uploader.busy && !uploadPage.posted
                inputMethodHints: Qt.ImhNoAutoUppercase
                label: "Tags, separated by commas (up to 3)"
                placeholderText: "Tags, e.g. cat, funny"
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: focus = false
            }

            TextSwitch {
                id: sensitiveSwitch
                enabled: !uploader.busy && !uploadPage.posted
                text: "Sensitive"
                description: "Mark the post as NSFW"
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: "Post only images you may share, and follow 9GAG's rules. 9GAG may review a post before others can see it."
            }

            ProgressBar {
                width: parent.width
                visible: uploader.busy
                minimumValue: 0
                maximumValue: 1
                value: uploader.progress
                label: uploader.statusText
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                visible: text.length > 0
                color: uploadPage.posted ? Theme.highlightColor : Theme.errorColor || Theme.highlightColor
                text: uploadPage.posted ? "Posted to 9GAG." : uploadPage.message
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !uploadPage.posted
                enabled: appSettings.loggedIn && (uploader.busy || uploadPage.problem.length === 0)
                text: uploader.busy ? "Stop" : "Post"
                onClicked: {
                    if (uploader.busy) {
                        uploader.cancel();
                        uploadPage.message = "Stopped.";
                    } else {
                        uploadPage.message = "";
                        uploader.upload(uploadPage.filePath, titleField.text, tagsField.text, sensitiveSwitch.checked);
                    }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: uploadPage.posted && uploadPage.postedUrl.length > 0
                text: "Open the post"
                onClicked: pageStack.push(Qt.resolvedUrl("OpenLinkDialog.qml"), {
                                              url: QMLUtils.toMobileUrl(uploadPage.postedUrl)
                                          })
            }

            // what's still missing, once something has been filled in
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                visible: !uploader.busy && !uploadPage.posted && uploadPage.message.length === 0
                         && (uploadPage.filePath.length > 0 || titleField.text.length > 0)
                text: uploadPage.problem
            }
        }

        VerticalScrollDecorator {}
    }
}
