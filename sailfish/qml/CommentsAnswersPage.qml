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

/*
  This shows the second-level-comments (answers) on a seperate page.
  The code is almost equal to 'CommentsPage.qml' with the difference that it is possible
  to set a specific CommentModel with a given ModelIndex (and some other minor differences).

  TODO Decide on the final implementation: Showing the answers on a seperate page or loading
  them into the same ListView like in a TreeView.
*/
Page {
    property CommentModel commentModel
    property var parentIndex
    property string commentsCount
    // the post and the comment whose answers these are
    property string postUrl
    property string parentId
    property string parentName
    property int modelStatus: commentModel.loadingStatus

    id: childCommentsPage

    SilicaListView {
        id: commentsListView
        anchors { top: parent.top; left: parent.left; right: parent.right; bottom: commentBox.top }
        clip: true
        model: visualModel
        spacing: Theme.paddingMedium

        header: PageHeader {
            id: header
            title: qsTr("Answers")
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

        // TODO First this requires the support by the CommentModel/NineGagApiClient
//        PullDownMenu {
//            MenuItem {
//                //: Refetches the content via the network.
//                text: qsTr("Refresh")
//                onClicked: {
//                    commentModel.refresh(visualModel.rootIndex)
//                }
//            }
//        }

        PushUpMenu {
            visible: modelStatus === CommentModel.FetchMoreFailure

            MenuItem {
                //: Try to load further items (via network) after there was on error on the initial execution.
                text: qsTr("Retry loading")
                onClicked: {
                    if (commentModel.canFetchMore(visualModel.rootIndex))
                        commentModel.fetchMore(visualModel.rootIndex)
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

    // every comment here is a reply to the thread's first comment
    CommentBox {
        id: commentBox
        anchors.bottom: parent.bottom
        postUrl: childCommentsPage.postUrl
        parentId: childCommentsPage.parentId
        replyToName: childCommentsPage.parentName
        canStopReplying: false

        onPosted: commentModel.replyAdded(parentId)
    }

    DelegateModel {
        id: visualModel
        model: commentModel
        rootIndex: modelIndex(parentIndex)

        delegate: CommentDelegate { }
    }

    BusyIndicator {
        size: BusyIndicatorSize.Large
        anchors.centerIn: parent
        running: modelStatus === CommentModel.Refreshing
    }
}
