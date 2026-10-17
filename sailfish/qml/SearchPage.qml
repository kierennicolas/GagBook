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
import "Tags.js" as Tags

// Search 9GAG tags. Suggestions come from 9GAG as you type; until they arrive (or if 9GAG
// can't be asked) the tags GagBook knows are suggested instead.
Page {
    id: searchPage

    property GagModel gagModel
    property string searchText: ""
    readonly property string query: searchText.trim()

    function showTag(name) {
        if (name.trim().length === 0)
            return;
        gagModel.showTag(name.trim());
        pageStack.pop();
    }

    function showLocalSuggestions() {
        suggestionModel.clear();
        if (query.length === 0)
            return;
        var tags = Tags.suggestions(query, appSettings.loggedIn, 20);
        for (var i = 0; i < tags.length; ++i)
            suggestionModel.append({ name: tags[i][0], count: 0 });
    }

    onSearchTextChanged: {
        showLocalSuggestions();
        suggestTimer.restart();
    }

    ListModel { id: suggestionModel }

    // ask 9GAG once typing pauses
    Timer {
        id: suggestTimer
        interval: 350
        onTriggered: {
            if (searchPage.query.length > 0)
                gagbookManager.suggestTags(searchPage.query);
        }
    }

    Connections {
        target: gagbookManager
        onTagSuggestions: {
            // ignore answers for older text, keep the local list if 9GAG had nothing
            if (text !== searchPage.query || tags.length === 0)
                return;
            suggestionModel.clear();
            for (var i = 0; i < tags.length; ++i)
                suggestionModel.append({ name: tags[i].name, count: tags[i].count });
        }
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: suggestionModel

        header: Column {
            width: listView.width

            PageHeader { title: qsTr("Search tags") }

            SearchField {
                id: searchField
                width: parent.width
                placeholderText: qsTr("Tag, e.g. cat or old meme")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onTextChanged: searchPage.searchText = text
                EnterKey.enabled: text.trim().length > 0
                EnterKey.iconSource: "image://theme/icon-m-search"
                EnterKey.onClicked: searchPage.showTag(text)
                Component.onCompleted: forceActiveFocus()
            }

            // the typed text itself, so any tag can be shown
            BackgroundItem {
                width: parent.width
                visible: searchPage.query.length > 0
                onClicked: searchPage.showTag(searchPage.query)

                Label {
                    anchors {
                        left: parent.left; right: parent.right; margins: Theme.horizontalPageMargin
                        verticalCenter: parent.verticalCenter
                    }
                    text: qsTr("Show posts tagged “%1”").arg(searchPage.query)
                    color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    elide: Text.ElideRight
                }
            }
        }

        delegate: SimpleListItem {
            text: model.name
            onClicked: searchPage.showTag(model.name)
        }

        ViewPlaceholder {
            enabled: listView.count === 0 && searchPage.query.length === 0
            text: qsTr("Search 9GAG tags")
            hintText: qsTr("Type a tag and pick a suggestion, or press Enter to show any tag")
        }

        VerticalScrollDecorator {}
    }
}
