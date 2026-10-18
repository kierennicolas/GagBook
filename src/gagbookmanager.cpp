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

#include "gagbookmanager.h"

#include <algorithm>

#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QDateTime>
#include <QtCore/QJsonObject>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkReply>

#include "networkmanager.h"
#include "gagimagedownloader.h"
#include "appsettings.h"
#include "ninegagapiclient.h"
#include "ninegagapirequest.h"

GagBookManager::GagBookManager(QObject *parent) :
    QObject(parent), m_isBusy(false), m_settings(0),
    m_netManager(new NetworkManager(this)), m_apiClient(0), m_gagRequest(0),
    m_sectionsWanted(false), m_sectionsUpdated(false), m_sectionsReply(0),
    m_suggestWanted(false), m_suggestReply(0), m_commentReply(0), m_commentTimer(new QTimer(this)),
    m_lastCommentRequestId(0), m_commentRenewing(false), m_commentRenewed(false)
{
    m_commentTimer->setSingleShot(true);
#ifdef GAGBOOK_TEST_COMMENT_TIMEOUT_MSECS
    m_commentTimer->setInterval(GAGBOOK_TEST_COMMENT_TIMEOUT_MSECS);     // only for tests
#else
    m_commentTimer->setInterval(30000);
#endif
    connect(m_commentTimer, &QTimer::timeout, this, &GagBookManager::onCommentTimeout);

    GagImageDownloader::initializeCache();
    connect(m_netManager, SIGNAL(downloadCounterChanged()), SIGNAL(downloadCounterChanged()));

    m_apiClient = new NineGagApiClient(m_netManager, this);
    connect(m_apiClient, &NineGagApiClient::userLoginFinished, this, &GagBookManager::onUserLoginFinished);
    connect(m_apiClient, &NineGagApiClient::userSessionEnded, this, &GagBookManager::onUserSessionEnded);
    connect(m_apiClient, &NineGagApiClient::sessionReady, this, &GagBookManager::onSessionReady);
    connect(m_apiClient, &NineGagApiClient::sessionFailed, this, &GagBookManager::onSessionFailed);
}

bool GagBookManager::isBusy() const
{
    return m_isBusy;
}

void GagBookManager::setBusy(bool busy)
{
    if (m_isBusy != busy) {
        m_isBusy = busy;
        emit busyChanged();
    }
}

QString GagBookManager::downloadCounter() const
{
    return m_netManager->downloadCounter();
}

QString GagBookManager::userName() const
{
    return m_apiClient->userDisplayName();
}

QString GagBookManager::userId() const
{
    return m_apiClient->userId();
}

QString GagBookManager::accountId() const
{
    return m_apiClient->accountId();
}

bool GagBookManager::canComment() const
{
    return m_apiClient->canComment();
}

bool GagBookManager::isCommentBusy() const
{
    return m_commentReply != 0 || m_commentRenewing || !m_commentQueue.isEmpty();
}

AppSettings *GagBookManager::settings() const
{
    return m_settings;
}

void GagBookManager::setSettings(AppSettings *settings)
{
    m_settings = settings;
    syncLoginState();
}

NetworkManager *GagBookManager::networkManager() const
{
    return m_netManager;
}

NineGagApiClient *GagBookManager::apiClient() const
{
    return m_apiClient;
}

/*!
 * \brief GagBookManager::syncLoginState Makes the 'loggedIn' setting match the API session.
 *  Older versions logged in through the website and only stored a cookie, those logins are
 *  not usable with the API.
 */
void GagBookManager::syncLoginState()
{
    if (m_settings != 0)
        m_settings->setLoggedIn(m_apiClient->isUserLoggedIn());

    emit userNameChanged();
}

void GagBookManager::login(const QString &username, const QString &password, bool rememberLogin)
{
    Q_ASSERT(m_apiClient);

    setBusy(true);
    m_apiClient->loginUser(username, password, rememberLogin);
}

void GagBookManager::logout()
{
    m_apiClient->logout();

    // cookies of the old website login
    m_netManager->clearCookies();

    syncLoginState();
}

void GagBookManager::onUserLoginFinished(bool success, const QString &errorMessage)
{
    setBusy(false);
    syncLoginState();

    if (success)
        emit loginSuccess();
    else
        emit loginFailure(errorMessage);
}

void GagBookManager::onUserSessionEnded(const QString &message)
{
    syncLoginState();
    emit sessionExpired(message);
}

GagRequest *GagBookManager::gagRequest()
{
    initGagRequest();

    Q_ASSERT(m_gagRequest != 0);
    return m_gagRequest;
}

void GagBookManager::initGagRequest()
{
    if (m_gagRequest == 0) {

        // TODO: add and connect slot to change gagRequest object on changed GagSource!
        switch (m_settings->source()) {
            default:
                qWarning("GagBookManager::GagBookManager(): Invalid source, default source will be used.");
                // fallthrough
            case AppSettings::NineGagApiSource:
                m_gagRequest = new NineGagApiRequest(m_netManager, m_apiClient, this);
                break;
        }
    }
}

void GagBookManager::updateSections()
{
    if (m_sectionsUpdated || m_sectionsReply != 0 || m_settings == 0)
        return;

    m_sectionsWanted = true;
    sendPendingRequests();
}

void GagBookManager::suggestTags(const QString &text)
{
    if (m_suggestReply != 0) {
        m_suggestReply->disconnect(this);
        m_suggestReply->abort();
        m_suggestReply->deleteLater();
        m_suggestReply = 0;
    }

    m_suggestText = text.trimmed();
    if (m_suggestText.isEmpty()) {
        m_suggestWanted = false;
        return;
    }

    m_suggestWanted = true;
    sendPendingRequests();
}

/* Sends the waiting requests once there is a session. */
void GagBookManager::sendPendingRequests()
{
    if (!m_sectionsWanted && !m_suggestWanted)
        return;

    if (!m_apiClient->sessionIsValid()) {
        m_apiClient->requestSession();      // answers with onSessionReady() or onSessionFailed()
        return;
    }

    if (m_sectionsWanted)
        sendSectionsRequest();
    if (m_suggestWanted)
        sendSuggestRequest();
}

void GagBookManager::onSessionReady()
{
    // a renewed login brings a new comment key
    emit userNameChanged();
    if (m_commentRenewing) {
        m_commentRenewing = false;
        m_commentRenewed = true;
        sendNextCommentRequest();
    }
    sendPendingRequests();
}

void GagBookManager::onSessionFailed()
{
    if (m_commentRenewing) {
        m_commentRenewing = false;
        failCommentRequests(QStringLiteral("Couldn't renew the login to 9GAG. Try again later."));
    }

    // keep the built-in sections and local suggestions
    m_sectionsWanted = false;
    if (m_suggestWanted) {
        m_suggestWanted = false;
        emit tagSuggestions(m_suggestText, QVariantList());
    }
}

void GagBookManager::sendSectionsRequest()
{
    m_sectionsWanted = false;
    m_sectionsReply = m_apiClient->retrieveSections();
    m_sectionsReply->setParent(this);
    connect(m_sectionsReply, &QNetworkReply::finished, this, &GagBookManager::onSectionsReplyFinished);
}

void GagBookManager::sendSuggestRequest()
{
    m_suggestWanted = false;
    m_suggestReply = m_apiClient->suggestTags(m_suggestText);
    m_suggestReply->setParent(this);
    m_suggestReply->setProperty("text", m_suggestText);
    connect(m_suggestReply, &QNetworkReply::finished, this, &GagBookManager::onSuggestReplyFinished);
}

namespace {

// the items of a JSON list, or the values of a JSON object used as a map
QJsonArray jsonItems(const QJsonValue &value)
{
    if (value.isArray())
        return value.toArray();

    QJsonArray items;
    const QJsonObject object = value.toObject();
    for (QJsonObject::const_iterator it = object.constBegin(); it != object.constEnd(); ++it)
        items.append(it.value());
    return items;
}

QList<Section> interestSections(const QJsonValue &value, const QString &category)
{
    QList<Section> sections;
    const QJsonArray items = jsonItems(value);

    for (int i = 0; i < items.size(); ++i) {
        const QJsonObject item = items.at(i).toObject();
        const QString name = item.value(QStringLiteral("name")).toString().trimmed();
        const QString key = item.value(QStringLiteral("listType")).toString().trimmed();
        if (name.isEmpty() || key.isEmpty())
            continue;
        sections << Section::feed(name, key, QStringLiteral("interest"), category,
                                  item.value(QStringLiteral("isSensitive")).toVariant().toBool());
    }

    std::sort(sections.begin(), sections.end(), [](const Section &a, const Section &b) {
        return QString::localeAwareCompare(a.name(), b.name()) < 0;
    });
    return sections;
}

} // namespace

void GagBookManager::onSectionsReplyFinished()
{
    QNetworkReply *reply = m_sectionsReply;
    m_sectionsReply = 0;
    reply->deleteLater();

    const QJsonObject data = QJsonDocument::fromJson(reply->readAll()).object()
            .value(QStringLiteral("data")).toObject();
    const QList<Section> interests = interestSections(data.value(QStringLiteral("interests")),
                                                      QStringLiteral("Interests"));

    if (reply->error() != QNetworkReply::NoError || interests.isEmpty()) {
        qWarning() << "GagBookManager: couldn't update the sections, keeping the built-in list. HTTP"
                   << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        return;
    }

    m_sectionsUpdated = true;

    SectionModel *model = m_settings->sections();
    QList<Section> sections;
    for (int i = 0; i < model->rowCount(); ++i) {
        const QModelIndex index = model->index(i, 0);
        if (model->data(index, SectionModel::KindRole).toString() == QLatin1String("home")) {
            sections << Section::feed(model->data(index, SectionModel::NameRole).toString(),
                                      model->data(index, SectionModel::UrlPathRole).toString(),
                                      QStringLiteral("home"),
                                      model->data(index, SectionModel::CategoryRole).toString(), false);
        }
    }
    sections << interests
             << interestSections(data.value(QStringLiteral("localInterests")), QStringLiteral("Local"));

    // leave the model alone if nothing changed, a reset moves the list view to the top
    bool changed = (sections.size() != model->rowCount());
    for (int i = 0; !changed && i < sections.size(); ++i) {
        const QModelIndex index = model->index(i, 0);
        changed = sections.at(i).name() != model->data(index, SectionModel::NameRole).toString()
                || sections.at(i).urlPath() != model->data(index, SectionModel::UrlPathRole).toString()
                || sections.at(i).isSensitive() != model->data(index, SectionModel::IsSensitiveRole).toBool();
    }

    if (changed) {
        model->setSections(sections);
        m_settings->saveSections();
        emit sectionsUpdated();
    }
}

void GagBookManager::onSuggestReplyFinished()
{
    QNetworkReply *reply = m_suggestReply;
    m_suggestReply = 0;
    reply->deleteLater();

    const QString text = reply->property("text").toString();
    QVariantList tags;

    if (reply->error() == QNetworkReply::NoError) {
        const QJsonArray items = QJsonDocument::fromJson(reply->readAll()).object()
                .value(QStringLiteral("data")).toObject().value(QStringLiteral("tags")).toArray();

        for (int i = 0; i < items.size(); ++i) {
            const QJsonObject item = items.at(i).toObject();
            const QString name = item.value(QStringLiteral("value")).toString().trimmed();
            if (name.isEmpty())
                continue;
            QVariantMap tag;
            tag.insert(QStringLiteral("name"), name);
            tag.insert(QStringLiteral("count"), item.value(QStringLiteral("count")).toVariant().toInt());
            tags << tag;
        }
    }

    emit tagSuggestions(text, tags);
}

int GagBookManager::addComment(const QString &postUrl, const QString &text, const QString &parentId)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return 0;
    if (!m_apiClient->canComment()) {
        emit commentFailure(QStringLiteral("Log in again from the app settings to write comments."), 0);
        return 0;
    }
    return queueCommentRequest(false, postUrl, trimmed, parentId);
}

int GagBookManager::deleteComment(const QString &postUrl, const QString &commentId)
{
    if (commentId.isEmpty())
        return 0;
    if (!m_apiClient->canComment()) {
        emit commentFailure(QStringLiteral("Log in again from the app settings to delete comments."), 0);
        return 0;
    }
    return queueCommentRequest(true, postUrl, QString(), commentId);
}

int GagBookManager::queueCommentRequest(bool isDelete, const QString &postUrl, const QString &text,
                                        const QString &id)
{
    CommentRequest request;
    request.id = ++m_lastCommentRequestId;
    request.isDelete = isDelete;
    request.postUrl = postUrl;
    request.text = text;
    request.targetId = id;

    const bool wasBusy = isCommentBusy();
    m_commentQueue.append(request);
    if (!wasBusy)
        emit commentBusyChanged();
    sendNextCommentRequest();
    return request.id;
}

void GagBookManager::sendNextCommentRequest()
{
    if (m_commentReply != 0 || m_commentRenewing || m_commentQueue.isEmpty())
        return;

    if (!m_apiClient->hasValidCommentKey()) {
        // sessions from before 1.7 have no key, and keys expire: a stored login gets a new one
        if (m_apiClient->canRenewUserSession() && !m_commentRenewed) {
            m_commentRenewing = true;
            m_apiClient->requestSession();      // continues in onSessionReady() or onSessionFailed()
            return;
        }
        failCommentRequests(QStringLiteral("Log out and in again from the app settings to write comments."));
        return;
    }

    m_currentComment = m_commentQueue.takeFirst();
    const CommentRequest &request = m_currentComment;
    m_commentReply = request.isDelete ? m_apiClient->deleteComment(request.postUrl, request.targetId)
                                      : m_apiClient->addComment(request.postUrl, request.text, request.targetId);
    m_commentReply->setParent(this);
    connect(m_commentReply, &QNetworkReply::finished, this, &GagBookManager::onCommentReplyFinished);
    m_commentTimer->start();
}

void GagBookManager::failCommentRequests(const QString &message)
{
    const QList<CommentRequest> failed = m_commentQueue;
    m_commentQueue.clear();
    m_commentRenewed = false;
    emit commentBusyChanged();
    foreach (const CommentRequest &request, failed)
        emit commentFailure(message, request.id);
}

void GagBookManager::onCommentTimeout()
{
    if (m_commentReply != 0) {
        qWarning() << "GagBookManager: comment request timed out";
        m_commentReply->setProperty("timedOut", true);
        m_commentReply->abort();     // finishes the reply
    }
}

bool GagBookManager::isOwnUser(const QString &userId, const QString &accountId) const
{
    const QString myUserId = m_apiClient->userId();
    const QString myAccountId = m_apiClient->accountId();
    // which of the two ids the comment service and the login answer use isn't certain,
    // so either may match either
    QStringList mine;
    if (!myUserId.isEmpty())
        mine << myUserId;
    if (!myAccountId.isEmpty())
        mine << myAccountId;
    return (!userId.isEmpty() && mine.contains(userId)) || (!accountId.isEmpty() && mine.contains(accountId));
}

void GagBookManager::onCommentReplyFinished()
{
    QNetworkReply *reply = m_commentReply;
    m_commentReply = 0;
    m_commentTimer->stop();
    reply->deleteLater();
    const CommentRequest request = m_currentComment;

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool timedOut = reply->property("timedOut").toBool();

    // answers look like {"status": "OK"|"ERROR", "code": ..., "error": ..., "payload": {...}}
    const QByteArray body = reply->readAll();
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QString status = root.value(QStringLiteral("status")).toString();
    const QString code = root.value(QStringLiteral("code")).toString();
    const QString error = root.value(QStringLiteral("error")).toString();

    const QJsonObject payload = root.value(QStringLiteral("payload")).toObject();
    const bool statusOk = status.compare(QLatin1String("OK"), Qt::CaseInsensitive) == 0
            || status.compare(QLatin1String("Success"), Qt::CaseInsensitive) == 0;
    // an empty answer to a successful request counts as done
    const bool success = !timedOut && httpStatus >= 200 && httpStatus < 300 && code.isEmpty() && error.isEmpty()
            && (statusOk || payload.contains(QStringLiteral("comment")) || body.trimmed().isEmpty());

    QString message;
    if (!success) {
        if (code == QLatin1String("CODE_COMMENT_RATE_LIMIT"))
            message = QStringLiteral("You're commenting too fast. Wait a moment and try again.");
        else if (code == QLatin1String("NO_QUOTA"))
            message = QStringLiteral("9GAG doesn't allow more comments from your account right now.");
        else if (!error.isEmpty())
            message = error;
        else if (timedOut)
            message = QStringLiteral("9GAG didn't answer. Check your connection and try again.");
        else if (httpStatus == 0)
            message = QStringLiteral("Couldn't connect to 9GAG. Check your connection and try again.");
        else if (request.isDelete)
            message = QStringLiteral("9GAG didn't delete the comment (HTTP %1).").arg(httpStatus);
        else
            message = QStringLiteral("9GAG didn't accept the comment (HTTP %1).").arg(httpStatus);
        qWarning() << "GagBookManager: comment request failed, HTTP" << httpStatus << "status" << status
                   << "code" << code << "error" << error;
    }

    // the next request goes out before the answer is handed on
    if (m_commentQueue.isEmpty())
        m_commentRenewed = false;
    sendNextCommentRequest();
    if (!isCommentBusy())
        emit commentBusyChanged();

    if (!success) {
        emit commentFailure(message, request.id);
    } else if (request.isDelete) {
        emit commentDeleted(request.postUrl, request.targetId, request.id);
    } else {
        QVariantMap comment = payload.value(QStringLiteral("comment")).toObject().toVariantMap();
        if (!comment.isEmpty()) {
            // fill in what the list needs if the answer leaves it out
            if (!comment.contains(QStringLiteral("user"))) {
                QVariantMap user;
                user.insert(QStringLiteral("displayName"), userName());
                user.insert(QStringLiteral("userId"), userId());
                user.insert(QStringLiteral("accountId"), accountId());
                comment.insert(QStringLiteral("user"), user);
            }
            if (comment.value(QStringLiteral("timestamp")).toLongLong() <= 0)
                comment.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch() / 1000);
            if (comment.value(QStringLiteral("mediaText")).toString().isEmpty()
                    && comment.value(QStringLiteral("text")).toString().isEmpty())
                comment.insert(QStringLiteral("text"), request.text.toHtmlEscaped());
            if (!comment.contains(QStringLiteral("type")))
                comment.insert(QStringLiteral("type"), QStringLiteral("text"));
        }
        emit commentAdded(request.postUrl, request.targetId, comment, request.id);
    }
}
