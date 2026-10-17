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

#include "ninegagapirequest.h"
#include "commentmodel.h"   // for 'Sorting' enum

//#include <QJsonParseError>
#include <QTextDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDateTime>
#include <QDebug>

/*!
    \class NineGagApiRequest
    \since 1.3.0
    \brief The NineGagApiRequest class handles all API requests and parses the retrieved content.

    The NineGagApiRequest class accesses the content provider API via the NineGagApiClient and
    parses all the responses.

    \sa NineGagApiClient
*/

/*!
 * \brief NineGagApiRequest::NineGagApiRequest Constructor.
 * \param networkManager Pointer to the global NetworkManager instance.
 * \param apiClient Pointer to the global NineGagApiClient instance, which owns the session.
 * \param parent The parent object.
 */
NineGagApiRequest::NineGagApiRequest(NetworkManager *networkManager, NineGagApiClient *apiClient,
                                     QObject *parent)
    : GagRequest(networkManager, parent), m_apiClient(apiClient), m_waitingForSession(false),
      m_retriedAfterAuthError(false)
{
    Q_ASSERT(m_apiClient != 0);

    connect(m_apiClient, &NineGagApiClient::sessionReady, this, &NineGagApiRequest::onSessionReady);
    connect(m_apiClient, &NineGagApiClient::sessionFailed, this, &NineGagApiRequest::onSessionFailed);
}

/*!
 * \brief NineGagApiRequest::startGagsRequest Reimplementation to initiate the request for
 *  fetching the gags. Requests a (guest or renewed user) session first if needed.
 */
void NineGagApiRequest::startGagsRequest()
{
    if (m_apiClient->sessionIsValid()) {
        m_waitingForSession = false;    // don't fetch again when a pending session arrives
        emit readyToRequestGags();
        return;
    }

    m_waitingForSession = true;
    m_apiClient->requestSession();
}

/*!
 * \brief NineGagApiRequest::retryAfterGagsError Reimplementation to recover from a rejected token.
 *  On HTTP 401 the session is dropped and the request is sent once more with a new session.
 */
bool NineGagApiRequest::retryAfterGagsError(QNetworkReply *reply)
{
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (httpStatus != 401) {
        m_retriedAfterAuthError = false;
        return false;
    }

    // the token is no good either way, the next request needs a new session
    m_apiClient->invalidateSession();

    if (m_retriedAfterAuthError) {
        m_retriedAfterAuthError = false;    // allow one retry again for the next refresh
        return false;
    }

    qDebug() << "The API rejected the session token, requesting a new session...";
    m_retriedAfterAuthError = true;
    startGagsRequest();
    return true;
}

/*!
 * \brief NineGagApiRequest::fetchGagsImpl Reimplementation to perform the network
 *  request for fetching the gags.
 * \param feed The home feed, interest or tag to fetch.
 * \param lastId The ids of the last posts (pagination of home feeds and interests).
 * \param fromIndex The number of posts already loaded (pagination of tags).
 * \return Returns the QNetworkReply object of the request.
 */
QNetworkReply *NineGagApiRequest::fetchGagsImpl(const GagFeed &feed, const QString &lastId, int fromIndex)
{
    switch (feed.kind) {
    case GagFeed::Interest:
        return m_apiClient->getInterestPosts(feed.key, lastId);
    case GagFeed::Tag:
        return m_apiClient->getTagPosts(feed.key, fromIndex);
    case GagFeed::Home:
    default:
        return m_apiClient->getHomePosts(feed.key, lastId);
    }
}

/*!
 * \brief NineGagApiRequest::parseGags Reimplementation to parse the network request
 *  to a list of gags.
 * \param response The response of the network request.
 * \return Returns a list of GagObjects.
 */
QList<GagObject> NineGagApiRequest::parseGags(const QByteArray &response)
{
    QJsonObject rootObj = QJsonDocument::fromJson(response).object();
    QJsonArray postsArr = rootObj.value("data").toObject().value("posts").toArray();
    const QVariantList postsList = postsArr.toVariantList();

    m_retriedAfterAuthError = false;

    if (postsList.isEmpty()) {
        qWarning("Empty JSON API response!");
        qDebug() << "### API response is: ###\n" << response.left(1000);

        /* if reached end of the list and there are no further posts
         * (e.g. possible inside 'Fresh' section) */
        // 'didEndOfList' has been sent as a number, accept a bool as well
        if (rootObj.value("data").toObject().value("didEndOfList").toVariant().toBool())
        {
            qDebug() << "Reached end of the list. There are no further posts to fetch.";
            emit reachedEndOfList();
        }
        else {
            QString errorMessage = NineGagApiClient::apiErrorMessage(rootObj);
            if (errorMessage.isEmpty())
                errorMessage = QStringLiteral("9GAG sent no posts. Try refreshing.");
            emit fetchGagsFailure(errorMessage);
        }
    }

    QList<GagObject> gagList;

    foreach (const QVariant &gagJson, postsList) {
        const QVariantMap gagMap = gagJson.toMap();

        GagObject gag;
        gag.setId(gagMap.value("id").toString());
        gag.setUrl(gagMap.value("url").toUrl());

        // title: convert included entity numbers
        QString titleStr = gagMap.value("title").toString();
        QTextDocument textDoc;
        textDoc.setHtml(titleStr);
        gag.setTitle(textDoc.toPlainText());

        gag.setVotesCount(gagMap.value("totalVoteCount").toInt());
        gag.setCommentsCount(gagMap.value("commentsCount").toInt());
        bool nsfw = gagMap.value("nsfw").toBool();

        // ToDo: disabled until user login is fully implemented
        if (nsfw) {
            //gag.setIsNSFW(true);

            // ToDo: if set then don't load content if client is not logged in as
            //       user, since the json still contains the urls with a guest login

            //qDebug() << "Following Gag is a NSFW gag: " << gag.title();
        }

        QString gagType = gagMap.value("type").toString();
        QVariantMap imagesMap = gagMap.value("images").toMap();

        // ToDo: set 'imageSize' for each gag

        if (gagType == QString("Photo")) {
            bool longPost = gagMap.value("hasLongPostCover").toBool();

            // Image
            if (!longPost) {
                gag.setImageUrl(imagesMap.value("image700").toMap().value("url").toUrl());
            }
            // Long image
            else {
                gag.setIsPartialImage(true);
                gag.setImageUrl(imagesMap.value("image460c").toMap().value("url").toUrl());
                gag.setFullImageUrl(imagesMap.value("image700").toMap().value("url").toUrl());

                //qDebug() << "Following Gag is a long image: " << gag.title();
            }
        }
        else if (gagType == QString("Animated")) {

            // GIFs are only available as a video source
            gag.setIsVideo(true);
            gag.setImageUrl(imagesMap.value("image700").toMap().value("url").toUrl());
            gag.setVideoUrl(imagesMap.value("image460sv").toMap().value("url").toUrl());

            /*
            int duration = imagesMap.value("image460sv").toMap().value("duration").toInt();

            // GIF
            if (!duration) {
                gag.setIsGIF(true);
                gag.setImageUrl(imagesMap.value("image700").toMap().value("url").toUrl());
                // GIF (this urls are not accessible/existend)
                //gag.setGifImageUrl(imagesMap.value("image700ba").toMap().value("url").toUrl());
                // mp4-Video
                gag.setGifImageUrl(imagesMap.value("image460sv").toMap().value("url").toUrl());

                // ToDo: update model to support 'GIFs' with video source
            }
            // mp4-Video
            else {
                gag.setIsVideo(true);
                gag.setImageUrl(imagesMap.value("image700").toMap().value("url").toUrl());
                gag.setVideoUrl(imagesMap.value("image460sv").toMap().value("url").toUrl());
            }
            */
        }
        else if (gagType == QString("Album")) {
            /* An Album consists of multiple separate gags with its own id, all specified
             * in the 'article' json object */

            gag.setImageUrl(imagesMap.value("image700").toMap().value("url").toUrl());
            qDebug() << "Found an unsupported 'Album' Gag: " << gag.title();

            // ToDo: extend the model to support albums
            // ToDo: for the meantime add a warning message to the user
        }
        else {      // "Article" and "Video", which is a Youtube video
            qWarning("Found an unsupported gag type!");
            // ToDo: extend the model to support videos
            // ToDo: for the meantime add a warning message to the user
        }

        gagList.append(gag);
    }

    return gagList;
}

/*!
 * \brief NineGagApiRequest::fetchCommentsImpl Reimplementation to perform the
 *  network request for fetching the comments.
 * \param data The required parameters to perform the request. The list has
 *  to contain the following data in the given order:
 *  data = {'URL of the post', 'count of comments to fetch', 'depth level',
 *  'reference comment for pagination', 'sorting value', 'auth string'}
 *  \sa NineGagApiClient
 * \return Returns the QNetworkReply object of the request.
 */
QNetworkReply *NineGagApiRequest::fetchCommentsImpl(const QVariantList &data)
{
    Q_ASSERT(data.count() == 6);

    CommentModel::Sorting sorting = data.at(4).value<CommentModel::Sorting>();
    NineGagApiClient::SortOrder sortOrder;
    NineGagApiClient::SortDirection sortDirection;

    switch (sorting) {
        default:
            qWarning("NineGagApiRequest::fetchCommentsImpl(): Using an invalid Sorting value, "
                     "default value will be used!");
            // fall through
        case CommentModel::Hot:
            sortOrder = NineGagApiClient::Score;
            sortDirection = NineGagApiClient::Descending;
            break;
        case CommentModel::Fresh:
            sortOrder = NineGagApiClient::Time;
            sortDirection = NineGagApiClient::Descending;
            break;
    }

    return m_apiClient->getComments(
                data.at(0).toUrl(),         // gagUrl
                data.at(1).toInt(),         // count
                data.at(2).toInt(),         // level
                data.at(3).toString(),      // refComment
                sortOrder,                  // sortOrder
                sortDirection,              // sortDirection
                data.at(5).toString());     // auth
}

/*!
 * \brief NineGagApiRequest::parseComments Reimplementation to parse the network request
 *  to a list of comments.
 * \param response The response of the network request.
 * \param parentComment The parent comment for which the comments has been fetched.
 * \return Returns a list of CommentObject pointers.
 */
QList<CommentObject *> NineGagApiRequest::parseComments(const QByteArray &response,
                                                        CommentObject *parentComment)
{
    QJsonObject rootObj = QJsonDocument::fromJson(response).object();
    QJsonObject payloadObj = rootObj.value("payload").toObject();
    QJsonArray commentsArr = payloadObj.value("comments").toArray();

    // set total comment count of the model (in the root comment)
    if (parentComment->isRootComment()) {
        // this API value represents ALL comments including all the replies (and not just the top-level comments!)
        parentComment->setTotalChildCount(payloadObj.value("total").toInt());

        // this is a workaround to be able to determine if there are further comments to fetch:
        // 'hasNext' determines if there are further top-level comments available
        parentComment->setHasMoreTopLvlComments(payloadObj.value("hasNext").toBool());

        // parse 'opUserId' (can be empty!)
        parentComment->setUser(UserObject(payloadObj.value("opUserId").toString()));
    }

    return parseChildComments(commentsArr, parentComment);
}

/*!
 * \brief NineGagApiRequest::onSessionReady Slot to continue a gags request once the
 *  API client has a session.
 */
void NineGagApiRequest::onSessionReady()
{
    if (m_waitingForSession) {
        m_waitingForSession = false;
        emit readyToRequestGags();
    }
}

/*!
 * \brief NineGagApiRequest::onSessionFailed Slot to report a failed session request.
 */
void NineGagApiRequest::onSessionFailed(const QString &errorMessage)
{
    if (m_waitingForSession) {
        m_waitingForSession = false;
        m_retriedAfterAuthError = false;
        emit fetchGagsFailure(errorMessage);
    }
}

/*!
 * \brief NineGagApiRequest::parseChildComments Helper method to recursively parse all the child
 *  comments for a parent comment.
 * \param jsonCommentsArray The QJsonArray containing the child comments of the parent.
 * \param parentComment The pointer to the parent comment.
 * \return Returns a list of CommentObject pointers.
 */
QList<CommentObject *> NineGagApiRequest::parseChildComments(const QJsonArray &jsonCommentsArray,
                                                             CommentObject *parentComment)
{
    const QVariantList variantList = jsonCommentsArray.toVariantList();
    QList<CommentObject *> commentsList;

    foreach (const QVariant &commentJson, variantList) {

        const QVariantMap commentMap = commentJson.toMap();
        CommentObject *comment = new CommentObject(parentComment);

        // commentId
        comment->setId(commentMap.value("commentId").toString());

        // text: convert included entity numbers, smilies etc.
        QString tmpStr = commentMap.value("mediaText").toString();
        QTextDocument tmpTextDoc;
        tmpTextDoc.setHtml(tmpStr); // TODO this removes line breaks
        comment->setText(tmpTextDoc.toPlainText());

        // type of text
        QString type = commentMap.value("type").toString();
        if (type == QString("text"))
            comment->setTextType(ContentType::Text);
        else if (type == QString("userMedia"))
            comment->setTextType(ContentType::UserMedia);
        else if (type == QString("media")) {
            comment->setTextType(ContentType::Media);

            // TODO fetch corresponding reaction website via API beforehand (memeful.com)
            // remove comment text since it contains just the media URL
            if (comment->text().contains(QString("memeful.com"), Qt::CaseInsensitive))
                comment->setText(QString());
        }
        else {
            qWarning() << "NineGagApiRequest::parseCommentMedia(): Found an unsupported media type:" << type;
            comment->setTextType(ContentType::Text);
        }

        // media - only existent if there is embed media (type!='text')
        if (comment->textType() != ContentType::Text) {
            ContentType mediaType = comment->textType();

            if (mediaType == ContentType::UserMedia) {
                const QJsonArray mediaArr = QJsonValue::fromVariant(commentJson).toObject().value("media").toArray();

                if (mediaArr.count() > 1)
                    qWarning("NineGagApiRequest::parseCommentMedia(): Unexpectedly the JSON contains several media objects!");

                if (!mediaArr.isEmpty())
                    comment->setMedia(parseCommentMedia(mediaArr.first().toObject(), mediaType));
            }
            else if (mediaType == ContentType::Media) {
                comment->setMedia(parseCommentMedia(QJsonValue::fromVariant(commentJson).toObject().value(
                                                        "embedMediaMeta").toObject(), mediaType));
            }
            else
                qWarning("NineGagApiRequest::parseChildComments(): An unsupported ContentType value is being used!");
        }

        // timestamp
        comment->setTimestamp(QDateTime::fromMSecsSinceEpoch(commentMap.value("timestamp").toLongLong() * (qint64) 1000));

        // permalink
        comment->setPermalink(commentMap.value("permalink").toUrl());

        // orderKey (field is only available for top-level comments) | TODO or is 0 for fetched secondLvlComments
        comment->setOrderKey(commentMap.value("orderKey").toString());

        // user
        const QJsonObject jsonUser = QJsonValue::fromVariant(commentJson).toObject().value("user").toObject();
        comment->setUser(parseUser(jsonUser));

        // upvotes
        comment->setUpvotes(commentMap.value("likeCount").toInt());

        // child count
        comment->setTotalChildCount(commentMap.value("childrenTotal").toInt());

        // parse child comments
        if (comment->totalChildCount() > 0) {
            const QJsonArray childComments = QJsonValue::fromVariant(
                        commentJson).toObject().value("children").toArray();

            comment->appendChildren(parseChildComments(childComments, comment));
        }

        commentsList.append(comment);
    }

    return commentsList;
}

CommentMediaObject NineGagApiRequest::parseCommentMedia(const QJsonObject &jsonMedia,
                                                        ContentType mediaType)
{
    CommentMediaObject mediaObj;
    QJsonObject embedMedia;

    if (mediaType == ContentType::UserMedia)
        embedMedia = jsonMedia.value("imageMetaByType").toObject();
    else if (mediaType == ContentType::Media)
        embedMedia = jsonMedia.value("embedImage").toObject();
    else
        return mediaObj;

    QString jsonType = embedMedia.value("type").toString();
    CommentMediaObject::CommentMediaType type = CommentMediaObject::Invalid;

    if (jsonType == QString("ANIMATED"))
        type = CommentMediaObject::Animated;
    else if (jsonType == QString("STATIC"))
        type = CommentMediaObject::Static;
    else
        qWarning() << "NineGagApiRequest::parseCommentMedia(): Found an unsupported media type:" << jsonType << "!";

    mediaObj.setMediaType(type);

    QJsonObject embedMediaData = embedMedia.value("image").toObject();
    mediaObj.setImageUrl(embedMediaData.value("url").toVariant().toUrl());
    mediaObj.setImageSize(QSize(embedMediaData.value("width").toInt(), embedMediaData.value("height").toInt()));

    if (type == CommentMediaObject::Animated)
    {
        embedMediaData = embedMedia.value("animated").toObject();
        mediaObj.setGifUrl(embedMediaData.value("url").toVariant().toUrl());
        mediaObj.setGifSize(QSize(embedMediaData.value("width").toInt(), embedMediaData.value("height").toInt()));

        embedMediaData = embedMedia.value("video").toObject();
        mediaObj.setVideoUrl(embedMediaData.value("url").toVariant().toUrl());
        mediaObj.setVideoSize(QSize(embedMediaData.value("width").toInt(), embedMediaData.value("height").toInt()));
    }

    return mediaObj;
}

UserObject NineGagApiRequest::parseUser(const QJsonObject &jsonUser)
{
    UserObject userObj = UserObject();
    userObj.setName(jsonUser.value("displayName").toString());
    userObj.setUserId(jsonUser.value("userId").toString());
    userObj.setAvatarUrl(jsonUser.value("avatarUrl").toVariant().toUrl());
    const QString emojiStr = jsonUser.value("emojiStatus").toString();

    if (!emojiStr.isEmpty()) {
        QTextDocument textDoc;
        textDoc.setHtml(emojiStr);
        userObj.setEmojiStatus(textDoc.toPlainText());
    }

    const QJsonObject userPermission = jsonUser.value("permissions").toObject();

    if (!userPermission.isEmpty()) {

        if (userPermission.contains("9GAG_Pro"))
            userObj.setIsProUser(true);

        if (userPermission.contains("9GAG_Staff"))
            userObj.setIsStaffUser(true);
    }

    return userObj;
}
