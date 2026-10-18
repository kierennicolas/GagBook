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

#ifndef POSTUPLOADER_H
#define POSTUPLOADER_H

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QJsonObject>

class GagBookManager;
class QNetworkReply;
class QTimer;

/*! Uploads an image as a new 9GAG post, the way the 9GAG app (8.24.7) does it:
    three POSTs to api.9gag.com/v2/post-submit with step=imageData (the file),
    step=metaData (title, tags, sensitive flag) and step=triggerCreation. */
class PostUploader : public QObject
{
    Q_OBJECT

    /*! Set the global instance of GagBookManager. */
    Q_PROPERTY(GagBookManager *manager READ manager WRITE setManager)
    /*! True while an upload runs. */
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    /*! Progress of the running upload, 0.0 to 1.0. */
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    /*! What the upload is doing, to show to the user. */
    Q_PROPERTY(QString statusText READ statusText NOTIFY progressChanged)

public:
    explicit PostUploader(QObject *parent = 0);

    GagBookManager *manager() const;
    void setManager(GagBookManager *manager);
    bool isBusy() const;
    qreal progress() const;
    QString statusText() const;

    /*! Title length the 9GAG app allows (its remote-config defaults). */
    static const int TitleMinLength = 6;
    static const int TitleMaxLength = 120;
    static const int MaxTags = 3;

    /*! Checks the input, returns an error message or an empty string. */
    Q_INVOKABLE QString validate(const QString &filePath, const QString &title, const QString &tags) const;

    /*! Upload the image \p filePath (a local path or file:// URL) as a post. \p tags is
        comma separated. Answers with succeeded() or failed(). */
    Q_INVOKABLE void upload(const QString &filePath, const QString &title, const QString &tags, bool nsfw);

    /*! Stop a running upload. */
    Q_INVOKABLE void cancel();

    /*! Splits comma separated tags, trimmed and without empty ones. */
    static QStringList splitTags(const QString &tags);

signals:
    void busyChanged();
    void progressChanged();
    /*! The post was created; \p postUrl is its 9GAG link (can be empty). */
    void succeeded(const QString &postUrl, const QString &postId);
    void failed(const QString &message);

private slots:
    void onImageFinished();
    void onImageProgress(qint64 sent, qint64 total);
    void onMetaFinished();
    void onCreationFinished();
    void sendMeta();
    void sendCreation();

private:
    enum Step { Idle, Image, Meta, Creation };

    void setStep(Step step, const QString &text, qreal progress);
    void finishWithError(const QString &message);
    void startReply(QNetworkReply *reply, const char *finishedSlot);
    bool readResponse(QNetworkReply *reply, QJsonObject *data, QString *error);

    GagBookManager *m_manager;
    QNetworkReply *m_reply;
    QTimer *m_retryTimer;
    Step m_step;
    QString m_statusText;
    qreal m_progress;
    int m_attempts;

    QString m_uploadId;
    QString m_title;
    QStringList m_tags;
    bool m_nsfw;
};

#endif // POSTUPLOADER_H
