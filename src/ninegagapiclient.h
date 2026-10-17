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

#ifndef NINEGAGAPICLIENT_H
#define NINEGAGAPICLIENT_H

#include <QObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QNetworkRequest>

#include "networkmanager.h"

class QNetworkReply;
class QJsonObject;
class QTimer;

/*! Client for the undisclosed 9GAG mobile app API (api.9gag.com).

    The client owns the API session. A session is either a guest session (token from
    /v2/guest-token) or a user session (token from /v2/user-token). User sessions are
    stored in QSettings so they survive an app restart; the password itself is never
    stored. If the user opts in to "stay logged in", the login name and the MD5 hash
    that the API expects are stored so the session can be renewed once it expires.
 */
class NineGagApiClient : public QObject
{
    Q_OBJECT

public:
    enum SortOrder {
        Score,
        Time
    };

    enum SortDirection {
        Ascending,
        Descending
    };

    NineGagApiClient(NetworkManager *netMan, QObject *parent = 0);

    /*! True if the current token (guest or user) exists and has not expired. */
    bool sessionIsValid() const;
    /*! True if the current session is a guest session. */
    bool isGuestSession() const;
    /*! True if a user is logged in, or a stored login can renew an expired user session. */
    bool isUserLoggedIn() const;
    /*! The 9GAG user name of the logged in user, or an empty string. */
    QString userDisplayName() const;

    /*! Make sure a valid session exists. Renews an expired user session if the login was
        stored, otherwise requests a guest token. Emits sessionReady() or sessionFailed().
        Does nothing if a session request is already running. */
    void requestSession();
    /*! Log in with a 9GAG email address (or user name) and password.
        Emits userLoginFinished() and, on success, sessionReady(). */
    void loginUser(const QString &loginName, const QString &password, bool rememberLogin);
    /*! Drop the user session and any stored login. The next request uses a guest session. */
    void logout();
    /*! Mark the current token as unusable, e.g. after the API answered 401. */
    void invalidateSession();

    QNetworkReply *getHomePosts(const QString &type, const QString &lastId);
    QNetworkReply *getInterestPosts(const QString &interest, const QString &lastId);
    QNetworkReply *getTagPosts(const QString &tag, int fromIndex);
    QNetworkReply *suggestTags(const QString &text);
    QNetworkReply *getComments(const QUrl &gagUrl, const int count, const int level, QString refComment,
                               SortOrder sortOrder, SortDirection sortDirection,
                               const QString &auth = QString());
    QNetworkReply *retrieveSections();

    /*! Returns an error message if \p root is an API error response, otherwise an empty string. */
    static QString apiErrorMessage(const QJsonObject &root);

signals:
    /*! A valid session is available and requests can be sent. */
    void sessionReady();
    /*! No session could be obtained, \p errorMessage says why. */
    void sessionFailed(const QString &errorMessage);
    /*! Result of loginUser(). \p errorMessage is empty on success. */
    void userLoginFinished(bool success, const QString &errorMessage);
    /*! The user session ended without the user logging out (expired or rejected) and the
        client fell back to a guest session. \p message should be shown to the user. */
    void userSessionEnded(const QString &message);

private slots:
    void onSessionReplyFinished();
    void onSessionTimeout();

private:
    enum SessionRequestType {
        GuestRequest,   //!< /v2/guest-token
        UserRequest,    //!< /v2/user-token, started by the user from the login dialog
        RenewRequest    //!< /v2/user-token, started automatically with the stored login
    };

    typedef QList<QPair<QString, QString> > ArgList;

    // what answered a session request, for error messages and the log
    struct ResponseInfo {
        QString server;
        QString contentType;
        QString cfMitigated;
        QString cfRay;
    };

    static qint64 currentSecs();
    static QByteArray getTimestamp();
    static QByteArray createSHA1();
    static QByteArray createMD5(const QString &str);
    static QByteArray createUUID();

    QByteArray createRequestSig(const QByteArray &timestamp) const;
    QUrl apiUrl(const QByteArray &path, const ArgList &args = ArgList()) const;
    QNetworkRequest apiRequest(const QUrl &url, const QByteArray &token) const;
    QNetworkReply *request(QNetworkRequest netReq);

    bool canRenewUserSession() const;
    void startSessionRequest(SessionRequestType type);
    void finishSessionRequest(SessionRequestType type, int httpStatus, int networkError,
                              const QByteArray &body, bool timedOut, const ResponseInfo &info);
    bool abortSessionRequest();
    void setSession(const QByteArray &token, qint64 expiry, bool guest);
    void clearUserSession(bool clearStoredLogin);
    void loadStoredSession();
    void saveUserSession() const;

    NetworkManager *m_netMan;
    QByteArray m_appToken;
    QByteArray m_deviceUUID;
    qint64 m_tokenExpiry;           // seconds since epoch
    bool m_isGuestSession;
    QString m_userName;

    QNetworkReply *m_sessionReply;
    SessionRequestType m_sessionRequestType;
    QTimer *m_sessionTimer;

    // login data of the running UserRequest/RenewRequest, or the stored login
    QString m_loginName;
    QByteArray m_passwordHash;
    bool m_rememberLogin;
};

#endif // NINEGAGAPICLIENT_H
