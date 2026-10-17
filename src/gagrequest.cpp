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

#include "gagrequest.h"

/*!
    \class GagRequest
    \since 0.9.0
    \brief The GagRequest class handles all network requests to download the required data.

    GagRequest handles all network requests to download the required data (e.g. a list of gags).
    This is an abstract class that encapsulates a request to download the data.
    This can be a subclass to support different ways of getting gags.

    \sa NineGagApiRequest
*/

/*!
 * \brief GagRequest::GagRequest Constructor.
 * \param networkManager Pointer to the global NetworkManager instance.
 * \param parent Parent QObject.
 */
GagRequest::GagRequest(NetworkManager *networkManager, QObject *parent) :
    QObject(parent), m_networkManager(networkManager), m_gagsReply(0),
    m_commentsReply(0)
{
}

/*!
 * \brief GagRequest::initiateGagsRequest Initiates the request for fetching the gags.
 */
void GagRequest::initiateGagsRequest()
{
    startGagsRequest();
}

/*!
 * \brief GagRequest::fetchGags Initiates the request to fetch the gags.
 * \param feed The home feed, interest or tag to fetch.
 * \param lastId The ids of the last posts, to get the next page of a home feed or interest.
 * \param fromIndex The number of posts already loaded, to get the next page of a tag.
 */
void GagRequest::fetchGags(const GagFeed &feed, const QString &lastId, int fromIndex)
{
    // a newer request replaces one that is still running (e.g. a refresh after logging in)
    if (m_gagsReply != 0) {
        qDebug() << "GagRequest::fetchGags(): Aborting the previous request.";
        m_gagsReply->disconnect(this);
        m_gagsReply->abort();
        m_gagsReply->deleteLater();
        m_gagsReply = 0;
    }

    m_gagsReply = fetchGagsImpl(feed, lastId, fromIndex);

    // make sure the QNetworkReply will be destroyed when this object is destroyed
    m_gagsReply->setParent(this);
    connect(m_gagsReply, SIGNAL(finished()), this, SLOT(onFetchGagsFinished()), Qt::UniqueConnection);
}

/*!
 * \brief GagRequest::fetchComments Initiates the request to fetch comments.
 * \param data A list of the required parameters to perform the comments request.
 * \param parentComment The parent comment for that the comments should be fetched.
 */
void GagRequest::fetchComments(const QVariantList &data, CommentObject *parentComment)
{
    if (m_commentsReply != 0) {
        qWarning() << "GagRequest::fetchComments(): A request is still active and will be aborted!";
        m_commentsReply->abort();   // emits finished() signal!
        m_commentsReply->deleteLater();
        m_commentsReply = 0;
    }

    m_commentsReply = fetchCommentsImpl(data);
    m_commentsReply->setParent(this);

    // It is assured that the connection is a QUniqueConnection since a new reply is created everytime
    connect(m_commentsReply, &QNetworkReply::finished,
            [this, parentComment](){ this->onFetchCommentsFinished(parentComment); });
}

/*!
 * \brief GagRequest::abortCommentsRequest Aborts all active comment requests, if there are any active,
 *  and closes down any network connections.
 *  Note: The finished() signal will be NOT emitted.
 */
void GagRequest::abortCommentsRequest()
{
    if (m_commentsReply != 0) {
        // prevent emitting the 'finished()' signal since calling this method implies an intended action
        m_commentsReply->disconnect();
        m_commentsReply->abort();
        m_commentsReply->deleteLater();
        m_commentsReply = 0;
    }
}

/*!
 * \brief GagRequest::onFetchGagsFinished Slot to process the QNetworkReply after fetching the posts.
 */
void GagRequest::onFetchGagsFinished()
{
    if (m_gagsReply->error()) {
        qDebug() << "QNetworkReply error: " << m_gagsReply->error()
                 << "\nQNetworkReply object: " << m_gagsReply->readAll();
        QNetworkReply *reply = m_gagsReply;
        QString errorStr = reply->errorString();
        reply->disconnect();
        reply->deleteLater();
        m_gagsReply = 0;

        if (!retryAfterGagsError(reply))
            emit fetchGagsFailure(errorStr);
        return;
    }

    QByteArray response = m_gagsReply->readAll();
    m_gagsReply->disconnect();
    m_gagsReply->deleteLater();
    m_gagsReply = 0;

    m_gagList = parseGags(response);

    // TODO improve the error handling (e.g. to differentiate between parsing errors and endOfList)
    if (m_gagList.isEmpty()) {
        //emit fetchGagsFailure("Unable to parse response");
        return; // assume that NineGagApiRequest emitted 'reachedEndOfList()'
    }
    else
        emit fetchGagsSuccess(m_gagList);
}

/*!
 * \brief GagRequest::onFetchCommentsFinished Slot to process the QNetworkReply after fetching
 *  the comments.
 * \p parentComment The parent comment for that the comments has been fetched.
 */
void GagRequest::onFetchCommentsFinished(CommentObject *parentComment)
{
    if (m_commentsReply->error()) {
        qDebug() << "QNetworkReply error on fetching comments: " << m_commentsReply->error()
                 << "\nQNetworkReply object: " << m_commentsReply->readAll();
        QString errorStr = m_commentsReply->errorString();
        m_commentsReply->disconnect();
        m_commentsReply->deleteLater();
        m_commentsReply = 0;
        emit fetchCommentsFailure(errorStr);
        return;
    }

    QByteArray response = m_commentsReply->readAll();
    m_commentsReply->disconnect();
    m_commentsReply->deleteLater();
    m_commentsReply = 0;

    // TODO check for an empty list & parse errors!
    const QList<CommentObject *> &commentList = parseComments(response, parentComment);

    emit fetchCommentsSuccess(commentList);
}

/*!
 * \brief GagRequest::retryAfterGagsError Default implementation, does not retry.
 */
bool GagRequest::retryAfterGagsError(QNetworkReply *reply)
{
    Q_UNUSED(reply);
    return false;
}

/*!
 * \brief GagRequest::networkManager Getter for retrieving the global NetworkManager instance.
 * \return Returns the pointer to the global NetworkManager instance.
 */
NetworkManager *GagRequest::networkManager() const
{
    return m_networkManager;
}
