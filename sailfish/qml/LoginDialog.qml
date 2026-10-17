/*
 * Copyright (c) 2014 Bob Jelica (@b0bben)
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

Dialog {
    id: loginDialog

    property alias username: usernameField.text
    property alias password: passwordField.text
    property alias rememberLogin: rememberSwitch.checked

    canAccept: usernameField.acceptableInput && passwordField.text.length > 0

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            anchors { left: parent.left; right: parent.right }
            spacing: constant.paddingMedium

            DialogHeader { title: "Login" }

            TextField {
                id: usernameField
                anchors { left: parent.left; right: parent.right }
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                label: "Email or username"
                placeholderText: label
                validator: RegExpValidator { regExp: /^\S+$/ }
                EnterKey.enabled: acceptableInput
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: passwordField.forceActiveFocus();
            }

            TextField {
                id: passwordField
                anchors { left: parent.left; right: parent.right }
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                echoMode: TextInput.Password
                label: "Password"
                placeholderText: label
                EnterKey.enabled: text.length > 0
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: loginDialog.accept();
            }

            TextSwitch {
                id: rememberSwitch
                anchors { left: parent.left; right: parent.right }
                text: "Stay logged in"
                description: "Saves your login name and a hash of your password on this phone, " +
                             "so GagBook can renew your session when it expires."
            }

            Label {
                anchors { left: parent.left; right: parent.right; margins: constant.paddingMedium }
                font.pixelSize: constant.fontSizeSmall
                color: Theme.highlightColor
                wrapMode: Text.Wrap
                text: "Log in with the email address (or username) and password of your 9GAG account. " +
                      "Logging in with Facebook, Google or Apple is not supported. " +
                      "Unless you choose to stay logged in, your password is not stored by the app."
            }
        }
    }
}

