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
#include <QtCore/QJsonObject>
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
    m_suggestWanted(false), m_suggestReply(0), m_commentReply(0)
{
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
    return m_commentReply != 0;
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
    sendPendingRequests();
}

void GagBookManager::onSessionFailed()
{
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

void GagBookManager::addComment(const QString &postUrl, const QString &text, const QString &parentId)
{
    const QString trimmed = text.trimmed();
    if (m_commentReply != 0 || trimmed.isEmpty())
        return;

    if (!m_apiClient->canComment()) {
        emit commentFailure(QStringLiteral("Log in again from the app settings to write comments."));
        return;
    }

    m_commentReply = m_apiClient->addComment(postUrl, trimmed, parentId);
    m_commentReply->setParent(this);
    m_commentReply->setProperty("postUrl", postUrl);
    connect(m_commentReply, &QNetworkReply::finished, this, &GagBookManager::onCommentReplyFinished);
    emit commentBusyChanged();
}

void GagBookManager::deleteComment(const QString &postUrl, const QString &commentId)
{
    if (m_commentReply != 0 || commentId.isEmpty())
        return;

    if (!m_apiClient->canComment()) {
        emit commentFailure(QStringLiteral("Log in again from the app settings to delete comments."));
        return;
    }

    m_commentReply = m_apiClient->deleteComment(postUrl, commentId);
    m_commentReply->setParent(this);
    m_commentReply->setProperty("postUrl", postUrl);
    m_commentReply->setProperty("deletedId", commentId);
    connect(m_commentReply, &QNetworkReply::finished, this, &GagBookManager::onCommentReplyFinished);
    emit commentBusyChanged();
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
    reply->deleteLater();
    emit commentBusyChanged();

    const QString postUrl = reply->property("postUrl").toString();
    const QString deletedId = reply->property("deletedId").toString();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // answers look like {"status": "OK"|"ERROR", "code": ..., "error": ..., "payload": {...}}
    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const QString status = root.value(QStringLiteral("status")).toString();
    const QString code = root.value(QStringLiteral("code")).toString();
    const QString error = root.value(QStringLiteral("error")).toString();

    const QJsonObject payload = root.value(QStringLiteral("payload")).toObject();
    const bool statusOk = status.compare(QLatin1String("OK"), Qt::CaseInsensitive) == 0
            || status.compare(QLatin1String("Success"), Qt::CaseInsensitive) == 0;
    const bool success = httpStatus >= 200 && httpStatus < 300 && code.isEmpty() && error.isEmpty()
            && (statusOk || payload.contains(QStringLiteral("comment")));

    if (success) {
        if (deletedId.isEmpty())
            emit commentAdded(postUrl);
        else
            emit commentDeleted(postUrl, deletedId);
        return;
    }

    QString message;
    if (code == QLatin1String("CODE_COMMENT_RATE_LIMIT"))
        message = QStringLiteral("You're commenting too fast. Wait a moment and try again.");
    else if (code == QLatin1String("NO_QUOTA"))
        message = QStringLiteral("9GAG doesn't allow more comments from your account right now.");
    else if (!error.isEmpty())
        message = error;
    else if (httpStatus == 0)
        message = QStringLiteral("Couldn't connect to 9GAG. Check your connection and try again.");
    else if (deletedId.isEmpty())
        message = QStringLiteral("9GAG didn't accept the comment (HTTP %1).").arg(httpStatus);
    else
        message = QStringLiteral("9GAG didn't delete the comment (HTTP %1).").arg(httpStatus);

    qWarning() << "GagBookManager: comment request failed, HTTP" << httpStatus << "status" << status
               << "code" << code << "error" << error;
    emit commentFailure(message);
}
