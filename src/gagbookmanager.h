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

#ifndef GAGBOOKMANAGER_H
#define GAGBOOKMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QVariantList>

#include "gagrequest.h"

class NetworkManager;
class NineGagApiClient;
class AppSettings;
class QNetworkReply;

/*! Handle login and hold other global instance class

    A single global instance class that hold and expose other global instance
    classes (AppSettings, NetworkManager and NineGagApiClient) so they can be easily
    accessible by other classes that need them. Also handle 9GAG account login.
 */
class GagBookManager : public QObject
{
    Q_OBJECT

    /*! True if there is an active login request. Busy visual feedback should
        show to user and login should be disable when busy. */
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

    /*! The download counter for current app session, in MB with 2 decimal points. */
    Q_PROPERTY(QString downloadCounter READ downloadCounter NOTIFY downloadCounterChanged)

    /*! The global instance of AppSettings. Must be set before component completed and
        can not be change afterward. */
    Q_PROPERTY(AppSettings *settings READ settings WRITE setSettings)

    /*! The 9GAG user name of the logged in user, empty if not logged in. */
    Q_PROPERTY(QString userName READ userName NOTIFY userNameChanged)

    /*! Ids of the logged in user, to recognise own comments. */
    Q_PROPERTY(QString userId READ userId NOTIFY userNameChanged)
    Q_PROPERTY(QString accountId READ accountId NOTIFY userNameChanged)

    /*! True if comments can be written (logged in, with the comment key from the login). */
    Q_PROPERTY(bool canComment READ canComment NOTIFY userNameChanged)

    /*! True while a comment is being posted or deleted. */
    Q_PROPERTY(bool commentBusy READ isCommentBusy NOTIFY commentBusyChanged)

public:
    explicit GagBookManager(QObject *parent = 0);

    bool isBusy() const;
    QString downloadCounter() const;
    QString userName() const;
    QString userId() const;
    QString accountId() const;
    bool canComment() const;
    bool isCommentBusy() const;

    /*! Get the global instance of AppSettings. */
    AppSettings *settings() const;
    void setSettings(AppSettings *settings);

    /*! Get the global instance of NetworkManager. */
    NetworkManager *networkManager() const;

    /*! Get the global instance of NineGagApiClient. */
    NineGagApiClient *apiClient() const;

    /*! Login to 9GAG account with an email address (or user name) and password. If
        \p rememberLogin is true, the session is renewed automatically when it expires.
        If login success, loginSuccess() will emit, otherwise loginFailure() will emit. */
    Q_INVOKABLE void login(const QString &username, const QString &password, bool rememberLogin = false);

    /*! Logout from 9GAG account. */
    Q_INVOKABLE void logout();

    GagRequest *gagRequest();

    /*! Replace the interests in the section list with the ones 9GAG lists right now.
        Runs once per app session; emits sectionsUpdated() if the list changed. */
    Q_INVOKABLE void updateSections();

    /*! Ask 9GAG for tags matching \p text. Answers with tagSuggestions(); a newer call
        replaces an unanswered one. */
    Q_INVOKABLE void suggestTags(const QString &text);

    /*! Post \p text as a comment on the post \p postUrl, as a reply to \p parentId if not
        empty. Answers with commentAdded() or commentFailure(). */
    Q_INVOKABLE void addComment(const QString &postUrl, const QString &text, const QString &parentId);

    /*! Delete the own comment \p commentId. Answers with commentDeleted() or commentFailure(). */
    Q_INVOKABLE void deleteComment(const QString &postUrl, const QString &commentId);

    /*! True if a comment author with these ids is the logged in user. */
    Q_INVOKABLE bool isOwnUser(const QString &userId, const QString &accountId) const;

private:
    void initGagRequest();
    void setBusy(bool busy);
    void syncLoginState();
    void sendPendingRequests();
    void sendSectionsRequest();
    void sendSuggestRequest();

signals:
    void busyChanged();
    void downloadCounterChanged();
    void userNameChanged();

    /*! Emit when login is succeeded. */
    void loginSuccess();

    /*! Emit when login is failed, \p errorMessage contains the reason for the
        failure and should show to user. */
    void loginFailure(const QString &errorMessage);

    /*! Emit when the login ended without the user logging out (e.g. it expired).
        \p message should show to user. */
    void sessionExpired(const QString &message);

    /*! Emit when the section list was replaced with 9GAG's current interests. */
    void sectionsUpdated();

    /*! Answer to suggestTags(). \p tags is a list of {"name": ..., "count": ...}, empty if
        9GAG couldn't be asked. */
    void tagSuggestions(const QString &text, const QVariantList &tags);

    void commentBusyChanged();
    /*! Emit when a comment was posted on \p postUrl. */
    void commentAdded(const QString &postUrl);
    /*! Emit when the comment \p commentId was deleted. */
    void commentDeleted(const QString &postUrl, const QString &commentId);
    /*! Emit when posting or deleting a comment failed; \p message should show to user. */
    void commentFailure(const QString &message);

private slots:
    void onUserLoginFinished(bool success, const QString &errorMessage);
    void onUserSessionEnded(const QString &message);
    void onSessionReady();
    void onSessionFailed();
    void onSectionsReplyFinished();
    void onSuggestReplyFinished();
    void onCommentReplyFinished();

private:
    bool m_isBusy;
    AppSettings *m_settings;
    NetworkManager *m_netManager;
    NineGagApiClient *m_apiClient;
    GagRequest *m_gagRequest;

    bool m_sectionsWanted;      // updateSections() waits for a session
    bool m_sectionsUpdated;     // done for this app session
    QNetworkReply *m_sectionsReply;
    QString m_suggestText;      // text of the suggestTags() call waiting for a session
    bool m_suggestWanted;
    QNetworkReply *m_suggestReply;
    QNetworkReply *m_commentReply;
};

#endif // GAGBOOKMANAGER_H
