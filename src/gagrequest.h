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

#ifndef GAGREQUEST_H
#define GAGREQUEST_H

#include <QtNetwork/QNetworkReply>
#include <QtCore/QObject>
#include <QtCore/QList>

#include "networkmanager.h"
#include "gagobject.h"
#include "gagmodel.h"
#include "gagfeed.h"
#include "commentobject.h"

class GagRequest : public QObject
{
    Q_OBJECT

public:
    explicit GagRequest(NetworkManager *networkManager, QObject *parent = 0);

    void initiateGagsRequest();
    /*! Fetch a page of \p feed. For the next page, \p lastId holds the ids of the last
     *  posts (home feeds and interests) and \p fromIndex the number of posts already
     *  loaded (tags). */
    void fetchGags(const GagFeed &feed, const QString &lastId, int fromIndex);
    void fetchComments(const QVariantList &data, CommentObject *parentComment);
    void abortCommentsRequest();

signals:
    /*! Emit this if the network request succeeds on fetching the gags data and
     *  the content has been parsed successful.
     *  \p gagList Contains the parsed GagObjects. */
    void fetchGagsSuccess(const QList<GagObject> &gagList);

    /*! Emit this if the network request failed on fetching the gags data.
     *  \p errorMessage Contains the reason of the failure. */
    void fetchGagsFailure(const QString &errorMessage);

    /*! Emit this to initiate the request to fetch the data for the gags/posts.
     *  \sa fetchGags, fetchGagsImpl */
    void readyToRequestGags();

    /*! Emit this if there are no further gags/posts available for the current
     *  section/groupId and therefore the end of the list has been reached. */
    void reachedEndOfList();

    /*! Emit this if the network request succeeds on fetching the comments data and
     *  the content has been parsed successful.
     *  \p commentList Contains the parsed CommentObjects. */
    void fetchCommentsSuccess(const QList<CommentObject *> &commentList);

    /*! Emit this if the network request failed on fetching the comments data.
     *  \p error Contains the reason of the failure. */
    void fetchCommentsFailure(const QString &error);

protected:
    /*! Get the global instance of NetworkManager. */
    NetworkManager *networkManager() const;

    /*! Implement this to start/send the request by emitting the readyToRequestGags
     *  signal. This function is useful if some preparation is needed prior to
     *  the request or to achieve a specific state of the derived GagRequest
     *  object (e.g. a login).
     *  \sa readyToRequestGags */
    virtual void startGagsRequest() = 0;

    /*! Implement this to make your own network request for a page of \p feed.
     *  \sa fetchGags */
    virtual QNetworkReply *fetchGagsImpl(const GagFeed &feed, const QString &lastId, int fromIndex) = 0;

    /*! Implement this to parse the network response to a list of GagObjects.
     *  \p response This is the content of the network reply. */
    virtual QList<GagObject> parseGags(const QByteArray &response) = 0;

    /*! Reimplement this to recover from a failed gags request, e.g. by renewing a session
     *  and sending the request again. Return true if the failure has been handled, then
     *  fetchGagsFailure() will not be emitted. The default implementation returns false.
     *  \p reply The failed network reply (it is deleted afterwards). */
    virtual bool retryAfterGagsError(QNetworkReply *reply);

    /*! Implement this to initiate a network request to retrieve the comments data.
     *  \p data Contains all the parameters that are needed for the implementing
     *  class to retrieve the comments. */
    virtual QNetworkReply *fetchCommentsImpl(const QVariantList &data) = 0;

    /*! Implement this to parse the network response to a list of CommentObjects.
     *  \p response This is the content of the network reply.
     *  \p parentComment Specifies the CommentObject that is the parent for the
     *   retrieved data. */
    virtual QList<CommentObject *> parseComments(const QByteArray &response,
                                                 CommentObject *parentComment) = 0;

private slots:
    void onFetchGagsFinished();
    void onFetchCommentsFinished(CommentObject *parentComment);

private:
    NetworkManager *m_networkManager;
    QNetworkReply *m_gagsReply;
    QList<GagObject> m_gagList;

    QNetworkReply *m_commentsReply;
};

#endif // GAGREQUEST_H
