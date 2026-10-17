/*
 * Copyright (C) 2019 Alexander Seibel.
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

ListItem {
    id: simpleListItem

    property alias text: labelItem.text
    property alias detail: detailItem.text     // small text on the right, e.g. "Sensitive"
    property bool selected: false

    contentHeight: Theme.itemSizeSmall
    width: ListView.view ? ListView.view.width : parent.width

    Label {
        id: detailItem
        anchors {
            right: parent.right; rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
        visible: text.length > 0
    }

    Label {
        id: labelItem
        anchors {
            left: parent.left; leftMargin: Theme.horizontalPageMargin
            right: detailItem.visible ? detailItem.left : parent.right
            rightMargin: detailItem.visible ? Theme.paddingMedium : Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        font.bold: selected
        font.pixelSize: Theme.fontSizeMedium
        color: simpleListItem.enabled ? (simpleListItem.selected || simpleListItem.highlighted ? Theme.highlightColor
                                                                                               : Theme.primaryColor)
                                      : Theme.secondaryColor
        elide: Text.ElideRight
    }
}
