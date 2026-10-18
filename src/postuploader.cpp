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

#include "postuploader.h"

#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkReply>

#include "gagbookmanager.h"
#include "ninegagapiclient.h"

namespace {

const int MaxAttempts = 10;             // while 9GAG answers "not ready"
const int RetryIntervalMsecs = 2000;
const qint64 MaxImageBytes = 10 * 1024 * 1024;   // 9GAG's limits for images and GIFs
const qint64 MaxGifBytes = 20 * 1024 * 1024;
const int StatusReady = 1;              // ApiUploadResponse.Data STATUS_READY

QString localPath(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
}

// error codes the 9GAG app handles (UploadService), as plain sentences
QString uploadErrorText(const QString &code)
{
    if (code == QLatin1String("UPLOAD_QUOTA_EXCEEDED"))
        return QStringLiteral("You've reached 9GAG's upload limit for now. Try again later.");
    if (code == QLatin1String("ACCOUNT_UNVERIFIED"))
        return QStringLiteral("Verify your email address with 9GAG before posting.");
    if (code == QLatin1String("ACCOUNT_TOO_NEW"))
        return QStringLiteral("Your 9GAG account is too new to post yet.");
    if (code == QLatin1String("ACCOUNT_SECURITY_CHECK"))
        return QStringLiteral("9GAG wants a security check for your account. Post once from the 9GAG app or website.");
    if (code == QLatin1String("ACTION_BLOCKED"))
        return QStringLiteral("9GAG has blocked uploads from your account for now.");
    if (code == QLatin1String("INVALID_FILE_SIZE"))
        return QStringLiteral("The file is too big for 9GAG (images up to 10 MB, GIFs up to 20 MB).");
    if (code == QLatin1String("INVALID_TYPE"))
        return QStringLiteral("9GAG doesn't accept this file type. Use JPEG, PNG or GIF.");
    if (code == QLatin1String("INVALID_SOURCE"))
        return QStringLiteral("9GAG didn't accept the source of the post.");
    if (code == QLatin1String("INVALID_TITLE"))
        return QStringLiteral("9GAG didn't accept the title.");
    if (code == QLatin1String("INVALID_IMAGE_DIMENSION"))
        return QStringLiteral("9GAG doesn't accept the image's dimensions.");
    if (code == QLatin1String("VIOLATING_RULES"))
        return QStringLiteral("9GAG says the post breaks its rules.");
    return QString();
}

} // namespace

PostUploader::PostUploader(QObject *parent) :
    QObject(parent), m_manager(0), m_reply(0), m_retryTimer(new QTimer(this)), m_step(Idle),
    m_progress(0), m_attempts(0), m_nsfw(false)
{
    m_retryTimer->setSingleShot(true);
    m_retryTimer->setInterval(RetryIntervalMsecs);
}

GagBookManager *PostUploader::manager() const
{
    return m_manager;
}

void PostUploader::setManager(GagBookManager *manager)
{
    m_manager = manager;
}

bool PostUploader::isBusy() const
{
    return m_step != Idle;
}

qreal PostUploader::progress() const
{
    return m_progress;
}

QString PostUploader::statusText() const
{
    return m_statusText;
}

bool PostUploader::canCancel() const
{
    return m_step == Session || m_step == Image || m_step == Meta;
}

QStringList PostUploader::splitTags(const QString &tags)
{
    QStringList result;
    foreach (const QString &tag, tags.split(QLatin1Char(','))) {
        const QString trimmed = tag.simplified();
        if (!trimmed.isEmpty() && !result.contains(trimmed, Qt::CaseInsensitive))
            result << trimmed;
    }
    return result;
}

QString PostUploader::validate(const QString &filePath, const QString &title, const QString &tags) const
{
    const QFileInfo info(localPath(filePath));
    if (filePath.isEmpty() || !info.isFile() || !info.isReadable())
        return QStringLiteral("Choose an image to post.");

    const QString suffix = info.suffix().toLower();
    const QStringList types = QStringList() << "jpg" << "jpeg" << "png" << "gif" << "webp";
    if (!types.contains(suffix))
        return QStringLiteral("9GAG takes JPEG, PNG, GIF or WebP images.");

    if (info.size() > (suffix == QLatin1String("gif") ? MaxGifBytes : MaxImageBytes))
        return QStringLiteral("The file is too big for 9GAG (images up to 10 MB, GIFs up to 20 MB).");

    const int titleLength = title.simplified().length();
    if (titleLength < TitleMinLength || titleLength > TitleMaxLength)
        return QStringLiteral("The title needs %1 to %2 characters.").arg(TitleMinLength).arg(TitleMaxLength);

    if (splitTags(tags).size() > MaxTags)
        return QStringLiteral("Use at most %1 tags.").arg(MaxTags);

    return QString();
}

void PostUploader::upload(const QString &filePath, const QString &title, const QString &tags, bool nsfw)
{
    if (isBusy())
        return;

    const QString error = validate(filePath, title, tags);
    if (!error.isEmpty()) {
        emit failed(error);
        return;
    }

    NineGagApiClient *client = m_manager ? m_manager->apiClient() : 0;
    if (client == 0 || !client->isUserLoggedIn()) {
        emit failed(QStringLiteral("Log in from the app settings to post."));
        return;
    }

    m_filePath = localPath(filePath);
    m_title = title.simplified();
    m_tags = splitTags(tags);
    m_nsfw = nsfw;
    m_attempts = 0;

    // An expired token needs renewing first, and sessions from before GagBook 1.7 lack the
    // user id the upload id is made of; a stored login fixes both.
    if (!client->sessionIsValid() || client->userId().isEmpty()) {
        if (!client->canRenewUserSession()) {
            emit failed(QStringLiteral("Log out and in again from the app settings to post."));
            return;
        }
        setStep(Session, QStringLiteral("Renewing the login..."), 0.01);
        connect(client, &NineGagApiClient::sessionReady, this, &PostUploader::onSessionReady, Qt::UniqueConnection);
        connect(client, &NineGagApiClient::sessionFailed, this, &PostUploader::onSessionFailed, Qt::UniqueConnection);
        client->requestSession();
        return;
    }
    sendImage();
}

void PostUploader::onSessionReady()
{
    if (m_step != Session)
        return;
    NineGagApiClient *client = m_manager->apiClient();
    if (client->isGuestSession() || client->userId().isEmpty()) {
        finishWithError(QStringLiteral("Log out and in again from the app settings to post."));
        return;
    }
    sendImage();
}

void PostUploader::onSessionFailed(const QString &message)
{
    if (m_step == Session)
        finishWithError(message.isEmpty() ? QStringLiteral("Couldn't renew the login to 9GAG.") : message);
}

void PostUploader::sendImage()
{
    NineGagApiClient *client = m_manager->apiClient();
    m_uploadId = client->newUploadId();

    QNetworkReply *reply = client->uploadImage(m_uploadId, m_filePath);
    if (reply == 0) {
        finishWithError(QStringLiteral("Couldn't read the image."));
        return;
    }

    setStep(Image, QStringLiteral("Uploading the image..."), 0.02);
    connect(reply, &QNetworkReply::uploadProgress, this, &PostUploader::onImageProgress);
    startReply(reply, SLOT(onImageFinished()));
}

void PostUploader::cancel()
{
    m_retryTimer->stop();
    m_retryTimer->disconnect(this);
    if (m_reply != 0) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = 0;
    }
    setStep(Idle, QString(), 0);
}

void PostUploader::setStep(Step step, const QString &text, qreal progress)
{
    const bool busyChange = (m_step == Idle) != (step == Idle);
    m_step = step;
    m_statusText = text;
    m_progress = progress;
    emit progressChanged();
    if (busyChange)
        emit busyChanged();
}

void PostUploader::startReply(QNetworkReply *reply, const char *finishedSlot)
{
    m_reply = reply;
    m_reply->setParent(this);
    connect(m_reply, SIGNAL(finished()), this, finishedSlot);
}

void PostUploader::finishWithError(const QString &message)
{
    qWarning() << "PostUploader: upload" << m_uploadId << "failed:" << message;
    cancel();
    emit failed(message);
}

/*! Reads a post-submit answer ({meta, data}). Returns false and sets \p error if 9GAG
    reported an error or didn't answer properly. */
bool PostUploader::readResponse(QNetworkReply *reply, QJsonObject *data, QString *error)
{
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const int networkError = reply->error();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
    const QJsonObject root = doc.object();
    *data = root.value(QStringLiteral("data")).toObject();

    const QString apiError = NineGagApiClient::apiErrorMessage(root);
    const bool isJson = parseError.error == QJsonParseError::NoError && doc.isObject();
    if (isJson && apiError.isEmpty() && httpStatus >= 200 && httpStatus < 300)
        return true;

    const QString code = root.value(QStringLiteral("meta")).toObject().value(QStringLiteral("errorCode")).toString();
    qWarning() << "PostUploader: step" << m_step << "HTTP" << httpStatus << "network error" << networkError
               << "code" << code << "-" << apiError;

    *error = uploadErrorText(code);
    if (error->isEmpty())
        *error = data->value(QStringLiteral("message")).toString();
    if (error->isEmpty() && !apiError.isEmpty())
        *error = apiError;
    if (error->isEmpty() && httpStatus == 0)
        *error = QStringLiteral("Couldn't connect to 9GAG. Check your connection and try again.");
    if (error->isEmpty())
        *error = QStringLiteral("9GAG didn't accept the upload (HTTP %1).").arg(httpStatus);
    return false;
}

void PostUploader::onImageProgress(qint64 sent, qint64 total)
{
    if (total > 0) {
        m_progress = 0.02 + 0.73 * qreal(sent) / qreal(total);
        emit progressChanged();
    }
}

void PostUploader::onImageFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = 0;
    reply->deleteLater();

    QJsonObject data;
    QString error;
    if (!readResponse(reply, &data, &error)) {
        finishWithError(error);
        return;
    }

    m_attempts = 0;
    sendMeta();
}

void PostUploader::sendMeta()
{
    NineGagApiClient *client = m_manager->apiClient();
    setStep(Meta, m_attempts == 0 ? QStringLiteral("Sending the title and tags...")
                                  : QStringLiteral("Waiting for 9GAG to process the image..."), 0.8);
    startReply(client->uploadMeta(m_uploadId, m_title, m_tags, m_nsfw), SLOT(onMetaFinished()));
}

void PostUploader::onMetaFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = 0;
    reply->deleteLater();

    QJsonObject data;
    QString error;
    if (!readResponse(reply, &data, &error)) {
        finishWithError(error);
        return;
    }

    // The app creates the post once both the image and the details are ready, and sends the
    // details again until they are. Without status fields there is nothing to wait for.
    const bool hasStatus = data.contains(QStringLiteral("mediaStatus")) || data.contains(QStringLiteral("metaStatus"));
    // numbers, or numbers as text
    const bool ready = data.value(QStringLiteral("mediaStatus")).toVariant().toInt() == StatusReady
            && data.value(QStringLiteral("metaStatus")).toVariant().toInt() == StatusReady;

    if (!hasStatus || ready) {
        m_attempts = 0;
        sendCreation();
    } else if (++m_attempts < MaxAttempts) {
        m_retryTimer->disconnect(this);
        connect(m_retryTimer, &QTimer::timeout, this, &PostUploader::sendMeta);
        setStep(Meta, QStringLiteral("Waiting for 9GAG to process the image..."), 0.8);
        m_retryTimer->start();
    } else {
        finishWithError(QStringLiteral("9GAG is still processing the image. Try again in a while."));
    }
}

void PostUploader::sendCreation()
{
    NineGagApiClient *client = m_manager->apiClient();
    setStep(Creation, QStringLiteral("Creating the post..."), 0.9);
    startReply(client->triggerPostCreation(m_uploadId), SLOT(onCreationFinished()));
}

void PostUploader::onCreationFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = 0;
    reply->deleteLater();

    QJsonObject data;
    QString error;
    if (!readResponse(reply, &data, &error)) {
        finishWithError(error);
        return;
    }

    const QJsonObject post = data.value(QStringLiteral("post")).toObject();
    if (!post.isEmpty()) {
        const QString url = post.value(QStringLiteral("url")).toString();
        const QString id = post.value(QStringLiteral("id")).toString();
        setStep(Idle, QString(), 1.0);
        emit succeeded(url, id);
        return;
    }

    if (++m_attempts < MaxAttempts) {
        m_retryTimer->disconnect(this);
        connect(m_retryTimer, &QTimer::timeout, this, &PostUploader::sendCreation);
        setStep(Creation, QStringLiteral("Waiting for 9GAG to create the post..."), 0.95);
        m_retryTimer->start();
    } else {
        // accepted, but 9GAG didn't say which post it became
        setStep(Idle, QString(), 1.0);
        emit succeeded(QString(), QString());
    }
}
