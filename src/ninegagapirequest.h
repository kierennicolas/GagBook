/*
 * Copyright (C) 2018 Alexander Seibel.
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

#ifndef NINEGAGAPIREQUEST_H
#define NINEGAGAPIREQUEST_H

#include <QNetworkReply>

#include "gagrequest.h"
#include "networkmanager.h"
#include "ninegagapiclient.h"
#include "commentmediaobject.h"

class NineGagApiRequest : public GagRequest
{
    Q_OBJECT

public:
    /*! \p apiClient is owned by the caller and must outlive this object. */
    NineGagApiRequest(NetworkManager *networkManager, NineGagApiClient *apiClient, QObject *parent = 0);

protected:
    void startGagsRequest();
    QNetworkReply *fetchGagsImpl(const GagFeed &feed, const QString &lastId, int fromIndex);
    QList<GagObject> parseGags(const QByteArray &response);
    bool retryAfterGagsError(QNetworkReply *reply);
    QNetworkReply *fetchCommentsImpl(const QVariantList &data);
    QList<CommentObject *> parseComments(const QByteArray &response, CommentObject *parentComment);

private slots:
    void onSessionReady();
    void onSessionFailed(const QString &errorMessage);

private:
    NineGagApiClient *m_apiClient;
    bool m_waitingForSession;
    bool m_retriedAfterAuthError;

    QList<CommentObject *> parseChildComments(const QJsonArray &jsonCommentsArray,
                                              CommentObject *parentComment);
    CommentMediaObject parseCommentMedia(const QJsonObject &jsonMedia, ContentType mediaType);
    UserObject parseUser(const QJsonObject &jsonUser);
};

#endif // NINEGAGAPIREQUEST_H
