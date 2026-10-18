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
import harbour.gagbook.Core 1.0

// Writes a comment on the post postUrl, or a reply. Placed below the comment list rather
// than in it, so changes to the list can't take the keyboard away while typing.
Column {
    id: commentBox

    property string postUrl
    // the comment a reply goes to (the first comment of its thread) and whose comment it answers
    property string parentId: ""
    property string replyToName: ""
    // false on the answers page, where every comment is a reply to the same thread
    property bool canStopReplying: true
    // the text field scrolls instead of growing past this
    property real maxTextHeight: Screen.height / 4
    readonly property bool _sending: _requestId > 0
    // the comment request this box sent (see GagBookManager::addComment)
    property int _requestId: 0

    // reply to the thread threadId; mention puts "@name" in front, as 9GAG does for replies to replies
    function replyTo(threadId, name, mention) {
        parentId = threadId;
        replyToName = name;
        if (mention && name && textArea.text.indexOf("@" + name) !== 0)
            textArea.text = "@" + name + " " + textArea.text;
        textArea.forceActiveFocus();
        textArea.cursorPosition = textArea.text.length;
    }

    function stopReplying() {
        parentId = "";
        replyToName = "";
    }

    function send() {
        var text = textArea.text.trim();
        if (text.length === 0 || _sending)
            return;
        // 0 if it can't be sent; main.qml shows why
        _requestId = gagbookManager.addComment(postUrl, text, parentId);
    }

    width: parent ? parent.width : 0

    // the comment pages put the new comment into the list; main.qml shows the messages
    Connections {
        target: gagbookManager
        onCommentAdded: {
            if (requestId !== commentBox._requestId)
                return;
            commentBox._requestId = 0;
            textArea.text = "";
            textArea.focus = false;
            if (commentBox.canStopReplying)
                commentBox.stopReplying();
        }
        // the text stays so it can be sent again
        onCommentFailure: {
            if (requestId === commentBox._requestId)
                commentBox._requestId = 0;
        }
    }

    Separator {
        width: parent.width
        color: Theme.primaryColor
        horizontalAlignment: Qt.AlignHCenter
    }

    // not logged in, or logged in before GagBook could write comments
    BackgroundItem {
        id: loginHint
        width: parent.width
        height: loginLabel.height + 2 * Theme.paddingLarge
        visible: !gagbookManager.canComment
        onClicked: pageStack.push(Qt.resolvedUrl("AppSettingsPage.qml"))

        Label {
            id: loginLabel
            anchors {
                left: parent.left; right: parent.right; margins: Theme.horizontalPageMargin
                verticalCenter: parent.verticalCenter
            }
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            color: loginHint.highlighted ? Theme.highlightColor : Theme.secondaryColor
            text: appSettings.loggedIn ? qsTr("Log out and in again in Settings to write comments")
                                       : qsTr("Log in to 9GAG in Settings to write comments")
        }
    }

    Item {
        width: parent.width
        height: Theme.itemSizeExtraSmall
        visible: gagbookManager.canComment && commentBox.parentId.length > 0 && commentBox.replyToName.length > 0

        Label {
            anchors {
                left: parent.left; leftMargin: Theme.horizontalPageMargin
                right: stopReplyingButton.left; rightMargin: Theme.paddingMedium
                verticalCenter: parent.verticalCenter
            }
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.highlightColor
            text: qsTr("Replying to %1").arg(commentBox.replyToName)
        }

        IconButton {
            id: stopReplyingButton
            anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin - Theme.paddingMedium; verticalCenter: parent.verticalCenter }
            visible: commentBox.canStopReplying
            icon.source: "image://theme/icon-m-clear"
            onClicked: commentBox.stopReplying()
        }
    }

    TextArea {
        id: textArea
        width: parent.width
        height: Math.min(implicitHeight, Math.max(Theme.itemSizeLarge, commentBox.maxTextHeight))
        visible: gagbookManager.canComment
        enabled: !commentBox._sending
        placeholderText: commentBox.parentId.length > 0 ? qsTr("Write a reply") : qsTr("Write a comment")
        label: placeholderText
    }

    Item {
        width: parent.width
        height: sendButton.height + Theme.paddingMedium
        visible: gagbookManager.canComment && (textArea.text.length > 0 || commentBox._sending)

        BusyIndicator {
            anchors { right: sendButton.left; rightMargin: Theme.paddingLarge; verticalCenter: sendButton.verticalCenter }
            size: BusyIndicatorSize.Small
            running: commentBox._sending
        }

        Button {
            id: sendButton
            anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; top: parent.top }
            enabled: textArea.text.trim().length > 0 && !commentBox._sending
            text: commentBox.parentId.length > 0 ? qsTr("Reply") : qsTr("Send")
            onClicked: commentBox.send()
        }
    }
}
