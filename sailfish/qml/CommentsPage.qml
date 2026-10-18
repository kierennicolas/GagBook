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
import QtQml.Models 2.2 // for DelegateModel QML type
import Sailfish.Silica 1.0
import harbour.gagbook.Core 1.0

Page {
    id: commentsPage

    property GagBookManager gagManager: null
    property url gagURL

    property string sortingText: qsTr("Sorting:")
    property string commentsCount: ""
    property int modelStatus: commentModel.loadingStatus

    function getSortingString() {
        if (commentModel.sorting === CommentModel.Hot)
            //: The 'Hot' sorting lists the best rated items at the top (e.g. comments with the most upvotes).
            //% Synonyms: Popular, Favoured
            return sortingText + " " + qsTr("Hot");
        else
            //: The 'Fresh' sorting lists the newest items (by the elapsed time) at the top.
            //% Synonyms: New, Recent
            return sortingText + " " + qsTr("Fresh");
    }

    function onSelectedSortingChanged(sortType) {
        // see SortPage.qml for possible sortTypes
        commentModel.sorting = sortType === "hot" ? CommentModel.Hot : CommentModel.Fresh
    }

    SilicaListView {
        id: commentsListView
        anchors { top: parent.top; left: parent.left; right: parent.right; bottom: commentBox.top }
        clip: true
        model: visualModel
        spacing: Theme.paddingMedium
        //clip: false  // http://doc.qt.io/qt-5/qtquick-performance.html#clipping

        header: PageHeader {
            id: pageHeader
            title: qsTr("Comments")
            description: commentsCount
        }

        footer: Item {
            id: footer
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            height: (footerLoader.status == Loader.Ready) ? (footerLoader.height + Theme.paddingMedium * 2)
                                                          : Theme.paddingMedium

            Loader {
                id: footerLoader
                anchors { verticalCenter: parent.verticalCenter; horizontalCenter: parent.horizontalCenter; }
                sourceComponent: {
                    switch (modelStatus) {
                    case CommentModel.FetchMoreProcessing:
                        return footerBusyIndicator;
                    case CommentModel.FetchMoreFailure:
                        return footerErrorLabel;
                    default:
                        return undefined;
                    }
                }

                Component {
                    id: footerBusyIndicator

                    BusyIndicator {
                        size: BusyIndicatorSize.Medium
                        running: true
                    }
                }

                Component {
                    id: footerErrorLabel

                    Label {
                        id: errorLabel
                        // TODO this should be as big as needed for the content (text) but not exceed footer.width!
                        width: footer.width //Math.min(paintedWidth, footer.width)
                        padding: Theme.paddingMedium
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: Theme.secondaryColor
                        font.italic: true

                        text: qsTr("Failed to load more comments!")
                    }
                }
            }
        }

        PullDownMenu {
            busy: modelStatus === CommentModel.Refreshing

            MenuItem {
                id: sortingMenuItem
                text: getSortingString()
                visible: !commentModel.isEmpty
                onClicked: {
                    pageStack.push(sortPage)
                }
            }
            MenuItem {
                //: Refetches the content via the network.
                text: qsTr("Refresh")
                onClicked: {
                    commentModel.refresh()
                }
            }
        }

        PushUpMenu {
            visible: modelStatus === CommentModel.FetchMoreFailure

            MenuItem {
                //: Try to load further items (via network) after there was on error on the initial execution.
                text: qsTr("Retry loading")
                onClicked: {
                    if (commentModel.canFetchMore(commentsListView.model.rootIndex))
                        commentModel.fetchMore(commentsListView.model.rootIndex)
                }
            }
        }

        ViewPlaceholder {
            id: emptyViewPlaceHolder
            enabled: commentModel.isEmpty && (modelStatus === CommentModel.Idle)

            text: qsTr("There are no comments available yet")
            //hintText: qsTr("Be the first one to comment")    // TODO
        }

        ViewPlaceholder {
            id: errorViewPlaceHolder
            enabled: commentModel.isEmpty && (modelStatus === CommentModel.RefreshFailure)

            //: Unable to fetch/load the required content because of a network issue.
            text: qsTr("Couldn't get any Comments")
            hintText: qsTr("Please check your data connection")
        }

        VerticalScrollDecorator { }
    }

    CommentBox {
        id: commentBox
        anchors.bottom: parent.bottom
        postUrl: commentsPage.gagURL.toString()

        onPosted: {
            if (parentId.length > 0) {
                commentModel.replyAdded(parentId);
            } else if (commentModel.sorting !== CommentModel.Fresh) {
                // newest first, so the new comment is at the top (changing it reloads)
                sortPage.currentlySelectedIndex = 1;
                commentModel.sorting = CommentModel.Fresh;
            } else {
                commentModel.refresh();
            }
        }
    }

    // this page stays under the answers page, so it also handles deletes made there
    Connections {
        target: gagbookManager
        onCommentDeleted: {
            if (postUrl === commentBox.postUrl)
                commentModel.removeCommentById(commentId);
        }
    }

    // Needed to display the hierarchical tree model in QML
    DelegateModel {
        id: visualModel
        model: commentModel
        rootIndex: modelIndex(0)

        delegate: CommentDelegate { }
    }

    CommentModel {
        id: commentModel
        manager: gagManager
        gagUrl: gagURL

        onSortingChanged: {
            sortingMenuItem.text = getSortingString()
            commentModel.refresh()
        }

        onLoadingStatusChanged: {
            // update commentsCount property
            if ((status == CommentModel.Idle) && !commentModel.isEmpty)
                commentsPage.commentsCount = commentModel.totalCommentCount()
            else if (status == CommentModel.Refreshing)
                commentsPage.commentsCount = ""
        }

        //onLoadingFailure: infoBanner.alert(errorMsg)
    }

    SortPage {
        id: sortPage

        onSelectedSortTypeChanged: onSelectedSortingChanged(sortType)
    }

//    InfoBanner {
//        id: infoBanner
//    }

    BusyIndicator {
        size: BusyIndicatorSize.Large
        anchors.centerIn: parent
        running: modelStatus === CommentModel.Refreshing
    }
}
