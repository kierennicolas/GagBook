/*
 * Copyright (C) 2019 Alexander Seibel.
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

ListItem {
    id: commentItem
    width: parent.width
    contentHeight: contentColumn.height + loadAnswersLoader.height

    property string timestamp: model.timestamp
    property var user: model.user
    property var media: model.media
    // written by the logged in user, so it can be deleted
    readonly property bool isOwn: gagbookManager.canComment && gagbookManager.isOwnUser(user.userId(), user.accountId())

    menu: ContextMenu {
        MenuItem {
            text: qsTr("Reply")
            visible: gagbookManager.canComment
            // replies go to the thread's first comment; answering a reply mentions its author
            onClicked: commentBox.replyTo(model.threadId, commentItem.user.name(), model.threadId !== model.id)
        }
        MenuItem {
            text: qsTr("Delete")
            visible: commentItem.isOwn
            onClicked: {
                // the item can be gone when the remorse timer ends; keep what the request needs
                var postUrl = commentBox.postUrl;
                var commentId = model.id;
                commentItem.remorseAction(qsTr("Deleting"), function() {
                    gagbookManager.deleteComment(postUrl, commentId);
                });
            }
        }
        MenuItem {
            //: Puts the comment text into the Clipboard
            text: qsTr("Copy text")
            onClicked: {
                Clipboard.text = model.textBody
                infoBanner.alert(qsTr("Copied comment to clipboard"))
            }
        }
        MenuItem {
            //: Puts the comment's permalink into the Clipboard
            text: qsTr("Copy link")
            onClicked: {
                Clipboard.text = model.url
                infoBanner.alert(qsTr("Copied permalink to clipboard"))
            }
        }
    }

    Image {
        id: userAvatar
        x: Theme.horizontalPageMargin
        fillMode: Image.PreserveAspectFit
        width: Screen.sizeCategory >= Screen.Large ? Theme.iconSizeExtraLarge : Theme.iconSizeLarge
        height: Screen.sizeCategory >= Screen.Large ? Theme.iconSizeExtraLarge : Theme.iconSizeLarge
        sourceSize.width: Screen.sizeCategory >= Screen.Large ? Theme.iconSizeExtraLarge : Theme.iconSizeLarge
        sourceSize.height: Screen.sizeCategory >= Screen.Large ? Theme.iconSizeExtraLarge : Theme.iconSizeLarge
        cache: false
        asynchronous: true
        smooth: !commentsListView.moving

        source: user.avatarUrl()

        BusyIndicator {
            size: BusyIndicatorSize.Medium
            anchors.centerIn: parent

            running: userAvatar.status != Image.Ready
        }
    }

    // commentBubble
    Rectangle {
        id: commentBubble
        anchors.fill: contentColumn
        z: -1
        color: Theme.highlightColor
        opacity: 0.3
        radius: Theme.paddingSmall
    }

    // comment body
    Column {
        id: contentColumn
        width: parent.width - x - Theme.horizontalPageMargin
        anchors {
            left: userAvatar.right; leftMargin: Theme.paddingLarge
        }

        Row {
            width: contentColumn.width - 2 * Theme.paddingMedium

            Label {
                id: userName
                color: commentItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                font.bold: true
                truncationMode: TruncationMode.Fade
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                padding: Theme.paddingMedium

                text: user.name()
            }

            // TODO add emojiStatus

            Label {
                id: opLabel
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
                horizontalAlignment: Text.AlignLeft
                anchors.baseline: userName.baseline
                padding: Theme.paddingMedium
                visible: model.isOriginalPoster

                //: The abbreviation for 'original poster'. The translation should be as short as possible.
                text: qsTr("OP")
            }

            Label {
                id: proUserLabel
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
                horizontalAlignment: Text.AlignLeft
                anchors.baseline: userName.baseline
                padding: Theme.paddingMedium
                visible: user.isProUser()

                //: The label is displayed for users who purchased special features. The translation should
                // be as short as possible.
                text: qsTr("PRO")
            }

            Label {
                id: staffUserLabel
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
                horizontalAlignment: Text.AlignLeft
                anchors.baseline: userName.baseline
                padding: Theme.paddingMedium
                visible: user.isStaffUser()

                //: The label is displayed for staff users.
                text: qsTr("Staff")
            }
        }

        Text {
            id: commentText
            anchors { left: parent.left; right: parent.right; margins: Theme.paddingMedium }
            font.pixelSize: Theme.fontSizeMedium
            color: commentItem.highlighted ? Theme.highlightColor : Theme.primaryColor
            wrapMode: Text.Wrap
            text: model.textBody
            visible: text
        }

        Loader {
            id: mediaLoader
            anchors { left: parent.left; right: parent.right; margins: Theme.paddingMedium;
            }
            sourceComponent: {
                switch (media.mediaType()) {
                case CommentMediaObject.Invalid:
                    return undefined;
                case CommentMediaObject.Static:
                    return imageComponent;
                case CommentMediaObject.Animated:
                    return animatedComponent;
                }
            }   // TODO add a BusyIndicator

            Component {
                id: imageComponent

                Image {
                    fillMode: Image.PreserveAspectFit
                    cache: false
                    asynchronous: true
                    smooth: !commentsListView.moving

                    source: media.imageUrl()
                }
            }

            Component {
                id: animatedComponent

                AnimatedImage {
                    id: gifImage
                    fillMode: Image.PreserveAspectFit
                    cache: false
                    smooth: !commentsListView.moving
                    asynchronous: true

                    source: media.gifUrl()

                    /* TODO implement a way to disk-cache images and be able to loop AnimatedImages
                       with deactivated cache property (requieres a local source path)
                    */
                }
            }
        }

        Row {
            id: infoRow
            width: parent.width

            Label {
                property int points: model.upvotes

                id: upvotes
                color: commentItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                font.pixelSize: Theme.fontSizeTiny
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                padding: Theme.paddingMedium

                //: Upvotes/Likes of a comment
                text: points === 1 ? qsTr("1 Point") : qsTr("%L1 Points").arg(points)
            }

            Label {
                id: timestampLabel
                //width: parent.width
                color: commentItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                font.pixelSize: Theme.fontSizeTiny
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                padding: Theme.paddingMedium

                text: Format.formatDate(commentItem.timestamp, Format.DurationElapsed)
            }
        }
    }

    Loader {
        id: loadAnswersLoader

        anchors { top: contentColumn.bottom; left: contentColumn.left; }
        width: contentColumn.width
        sourceComponent: model.hasModelChildren ? loadAnswersComponent : undefined

        Component {
            id: loadAnswersComponent

            BackgroundItem {

                id: loadAnswersItem

                onClicked: {
                    if (model.hasModelChildren) {
                        pageStack.push(Qt.resolvedUrl("CommentsAnswersPage.qml"), {
                                           commentModel: commentModel,
                                           parentIndex: index,
                                           commentsCount: model.childCommentCount,
                                           postUrl: commentBox.postUrl,
                                           parentId: model.id,
                                           parentName: commentItem.user.name()
                                       })
                    }
                }

                Label {
                    property int commentCount: model.childCommentCount

                    id: loadAnswersLabel
                    anchors.fill: parent
                    anchors.margins: Theme.paddingMedium
                    verticalAlignment: Text.AlignVCenter
                    color: loadAnswersItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    font.pixelSize: Theme.fontSizeSmall

                    text: commentCount === 1 ? qsTr("View 1 answer...") : qsTr("View %L1 answers...").arg(commentCount)
                }
            }
        }
    }

    // trigger timestamp string update every minute
    Timer {
        interval: 60000
        repeat: true
        running: !commentModel.isEmpty
        onTriggered: timestampLabel.text = Format.formatDate(commentItem.timestamp, Format.DurationElapsed)
    }

}
