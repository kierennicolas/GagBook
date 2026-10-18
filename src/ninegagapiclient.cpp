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

#include <QCryptographicHash>
#include <QNetworkReply>
#include <QDateTime>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <QDebug>

#include "ninegagapiclient.h"

/*!
    \class NineGagApiClient
    \since 1.3.0
    \brief The NineGagApiClient class provides the data via the undisclosed official 9GAG.com API.

    Since 1.5.0 the client also handles the 9GAG account login. The 9gag.com website (and its
    old /login and /vote endpoints) sits behind a Cloudflare challenge, so the login has to go
    through the same API the official Android app uses.
*/

// Defining API URLs:
#ifdef GAGBOOK_TEST_API_URL
const QByteArray API_URL = GAGBOOK_TEST_API_URL;     // only for tests against a mock server
#else
const QByteArray API_URL = "https://api.9gag.com";
#endif
#ifdef GAGBOOK_TEST_COMMENT_URL
const QByteArray COMMENT_URL = GAGBOOK_TEST_COMMENT_URL;  // only for tests against a mock server
#else
const QByteArray COMMENT_URL = "https://comment.9gag.com";
#endif
const QByteArray COMMENT_URL_CDN = "https://comment-cdn.9gag.com";
const QByteArray NOTIFY_URL = "https://notify.9gag.com";
const QByteArray ADMIN_URL = "https://admin.9gag.com";

// Defining API paths:
const QByteArray POSTS_PATH = "/v2/post-list";
const QByteArray GUEST_PATH = "/v2/guest-token";
const QByteArray LOGIN_PATH = "/v2/user-token";
const QByteArray SECTIONS_PATH = "/v2/group-list";
const QByteArray TAG_SEARCH_PATH = "/v2/tag-search";
const QByteArray TAG_SUGGEST_PATH = "/v2/tag-suggest";
const QByteArray COMMENT_PATH = "/v1/comment.json";
const QByteArray COMMENT_PATH_CDN = "/v1/cacheable/comment-list.json";
const QByteArray COMMENTINFO_PATH = "/v1/commentInfo.json";
const QByteArray COMMENTSDATA_PATH = "/v1/commentsData.json";
const QByteArray TOPCOMMENTS_PATH = "/v1/topComments.json";
const QByteArray COMMENT_ADD_PATH = "/v1/comment.json";     // POST form; DELETE '/v1/comment.json/<id>'
const QByteArray POST_SUBMIT_PATH = "/v2/post-submit";      // steps imageData, metaData, triggerCreation

// Defining App constants:
const QByteArray APP_ID = "com.ninegag.android.app";
const QByteArray DEVICE_TYPE = "android";
const QByteArray BUCKET_NAME = "MAIN_RELEASE";   // "__DEFAULT__";
const QByteArray COMMENT_ID_CDN = "a_dd8f2b7d304a10edaf6f29517ea0ca4100a43d1b";

// The official app (8.24.7, Oct 2026) identifies itself as '9GAG/<versionCode> (<device>;Android <n>)'
// together with 'X-Package-Version'. With Qt's default User-Agent ("Mozilla/5.0") the login got
// HTTP 403 (Oct 2026), most likely from 9GAG's firewall treating it as a bot.
const QByteArray APP_VERSION_CODE = "80240700";
const QByteArray APP_USER_AGENT = "9GAG/" + APP_VERSION_CODE + " (Jolla Sailfish OS;Android 10)";

// Characters that may stay unencoded inside a path segment (RFC 3986 'pchar'), so that
// e.g. an email address or a comma separated id list is sent the way the Android app sends it.
const QByteArray PATH_SEGMENT_SAFE_CHARS = "@,+=:!$&'()*;";

// Renew a token a bit before the server-side expiry and use a short lifetime if the
// response does not say how long the token is valid.
const qint64 TOKEN_EXPIRY_MARGIN_SECS = 60;
const qint64 FALLBACK_TOKEN_LIFETIME_SECS = 60 * 60;

// Give up on a guest-token or login request after this long.
#ifdef GAGBOOK_TEST_SESSION_TIMEOUT_MSECS
const int SESSION_TIMEOUT_MSECS = GAGBOOK_TEST_SESSION_TIMEOUT_MSECS;
#else
const int SESSION_TIMEOUT_MSECS = 30 * 1000;
#endif

// QSettings keys
const QString SETTINGS_GROUP = QStringLiteral("apiSession");
const QString KEY_DEVICE_UUID = QStringLiteral("deviceUuid");
const QString KEY_USER_TOKEN = QStringLiteral("userToken");
const QString KEY_TOKEN_EXPIRY = QStringLiteral("tokenExpiry");
const QString KEY_USER_NAME = QStringLiteral("userName");
const QString KEY_LOGIN_NAME = QStringLiteral("loginName");
const QString KEY_PASSWORD_HASH = QStringLiteral("passwordHash");
const QString KEY_USER_ID = QStringLiteral("userId");
const QString KEY_ACCOUNT_ID = QStringLiteral("accountId");
const QString KEY_COMMENT_AUTH = QStringLiteral("commentAuth");
const QString KEY_COMMENT_AUTH_EXPIRY = QStringLiteral("commentAuthExpiry");

namespace {

qint64 jsonToInt64(const QJsonValue &value)
{
    if (value.isDouble())
        return static_cast<qint64>(value.toDouble());
    if (value.isString())
        return value.toString().toLongLong();
    return 0;
}

/* Works out when a token from /v2/guest-token or /v2/user-token expires. The API sends
 * 'secondsTillExpiry' (relative) and 'tokenExpiry' (absolute, seconds since epoch). */
qint64 tokenExpiryFromResponse(const QJsonObject &data, qint64 now)
{
    const qint64 secondsLeft = jsonToInt64(data.value(QStringLiteral("secondsTillExpiry")));
    qint64 expiry = jsonToInt64(data.value(QStringLiteral("tokenExpiry")));

    if (expiry > now * 100)     // milliseconds instead of seconds
        expiry /= 1000;

    if (secondsLeft > 0)
        expiry = now + secondsLeft;     // immune to a wrong clock on the phone
    else if (expiry <= now)
        expiry = now + FALLBACK_TOKEN_LIFETIME_SECS;

    return expiry - TOKEN_EXPIRY_MARGIN_SECS;
}

} // namespace

/*!
 * \brief NineGagApiClient::NineGagApiClient Constructor.
 * \param netMan Pointer to the global NetworkManager instance.
 * \param parent The parent object.
 */
NineGagApiClient::NineGagApiClient(NetworkManager *netMan, QObject *parent) :
    QObject(parent), m_netMan(netMan), m_tokenExpiry(0), m_isGuestSession(true), m_commentAuthExpiry(0),
    m_sessionReply(0), m_sessionRequestType(GuestRequest), m_sessionTimer(new QTimer(this)),
    m_rememberLogin(false)
{
    m_sessionTimer->setSingleShot(true);
    m_sessionTimer->setInterval(SESSION_TIMEOUT_MSECS);
    connect(m_sessionTimer, &QTimer::timeout, this, &NineGagApiClient::onSessionTimeout);

    loadStoredSession();
}

qint64 NineGagApiClient::currentSecs()
{
    // QDateTime::currentSecsSinceEpoch() needs Qt 5.8, Sailfish OS ships Qt 5.6
    return QDateTime::currentMSecsSinceEpoch() / 1000;
}

/*!
 * \brief NineGagApiClient::getTimestamp Generates the current system time in milliseconds.
 * \return Returns the current (POSIX) system time in milliseconds.
 */
QByteArray NineGagApiClient::getTimestamp()
{
    return QByteArray::number(QDateTime::currentMSecsSinceEpoch());
}

/*!
 * \brief NineGagApiClient::createSHA1 Generates a random SHA-1 hash sum.
 * \return Returns the QByteArray representation formatted as a hex digit.
 */
QByteArray NineGagApiClient::createSHA1()
{
    return QCryptographicHash::hash(getTimestamp() + QUuid::createUuid().toByteArray(),
                                    QCryptographicHash::Sha1).toHex();
}

/*!
 * \brief NineGagApiClient::createMD5 Generates a MD5 hash sum for the given QString.
 * \param str The data to generate the MD5 hash sum.
 * \return  Returns a hex encoded MD5 hash for the given QString.
 */
QByteArray NineGagApiClient::createMD5(const QString &str)
{
    return QCryptographicHash::hash(str.toUtf8(), QCryptographicHash::Md5).toHex();
}

/*!
 * \brief NineGagApiClient::createUUID Generates a random UUID without '{', '}' and '-'.
 * \return Returns 32 hex digits.
 */
QByteArray NineGagApiClient::createUUID()
{
    QString uuid = QUuid::createUuid().toString();
    uuid.remove(0, 1).chop(1);
    uuid.remove(QChar('-'));
    return uuid.toUtf8();
}

/*!
 * \brief NineGagApiClient::createRequestSig Generates a specific hash sum to sign the HTTP request.
 * \param timestamp The timestamp needed for the signature generation.
 * \return Returns the generated request signature as a QByteArray representation.
 */
QByteArray NineGagApiClient::createRequestSig(const QByteArray &timestamp) const
{
    const QByteArray str = "*" + timestamp + "_._" + APP_ID + "._." + m_deviceUUID + "9GAG";
    return QCryptographicHash::hash(str, QCryptographicHash::Sha1).toHex();
}

/*!
 * \brief NineGagApiClient::apiUrl Builds an API URL. The mobile API takes its arguments as
 *  path segments ('/v2/post-list/group/1/type/hot/...') rather than as a query string.
 * \param path The API path, e.g. POSTS_PATH.
 * \param args The arguments in the order they should appear.
 */
QUrl NineGagApiClient::apiUrl(const QByteArray &path, const ArgList &args) const
{
    QByteArray encoded = API_URL + path;

    for (int i = 0; i < args.size(); ++i) {
        encoded.append('/');
        encoded.append(QUrl::toPercentEncoding(args.at(i).first));
        encoded.append('/');
        encoded.append(QUrl::toPercentEncoding(args.at(i).second, PATH_SEGMENT_SAFE_CHARS));
    }

    return QUrl::fromEncoded(encoded);
}

/*!
 * \brief NineGagApiClient::apiRequest Creates a signed API request.
 * \param url The url to which the request is directed to.
 * \param token The value of the '9GAG-9GAG_TOKEN' header.
 */
QNetworkRequest NineGagApiClient::apiRequest(const QUrl &url, const QByteArray &token) const
{
    QNetworkRequest netReq(url);
    const QByteArray timestamp = getTimestamp();

    netReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netReq.setRawHeader("9GAG-9GAG_TOKEN", token);
    netReq.setRawHeader("9GAG-TIMESTAMP", timestamp);
    netReq.setRawHeader("9GAG-APP_ID", APP_ID);
    netReq.setRawHeader("X-Package-ID", APP_ID);
    netReq.setRawHeader("9GAG-DEVICE_UUID", m_deviceUUID);
    netReq.setRawHeader("X-Device-UUID", m_deviceUUID);
    netReq.setRawHeader("9GAG-DEVICE_TYPE", DEVICE_TYPE);
    netReq.setRawHeader("9GAG-BUCKET_NAME", BUCKET_NAME);
    netReq.setRawHeader("User-Agent", APP_USER_AGENT);
    netReq.setRawHeader("X-Package-Version", APP_VERSION_CODE);
    netReq.setRawHeader("9GAG-REQUEST-SIGNATURE", createRequestSig(timestamp));

    // never answer API requests from the disk cache
    netReq.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    netReq.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);

    return netReq;
}

/*!
 * \brief NineGagApiClient::request Sends the API request with the given data.
 * \param netReq The QNetworkRequest object that should be used for the request.
 * \return Returns the pointer to the QNetworkReply object of the request.
 */
QNetworkReply *NineGagApiClient::request(QNetworkRequest netReq)
{
    return m_netMan->createGetRequest(netReq);
}

/*!
 * \brief NineGagApiClient::apiErrorMessage Reads the error message of an API response.
 * \param root The root object of the JSON response.
 * \return Returns an empty string if the response does not report an error.
 */
QString NineGagApiClient::apiErrorMessage(const QJsonObject &root)
{
    const QJsonObject meta = root.value(QStringLiteral("meta")).toObject();

    if (!meta.isEmpty()) {
        if (meta.value(QStringLiteral("status")).toString() == QLatin1String("Success"))
            return QString();

        const QString message = meta.value(QStringLiteral("errorMessage")).toString();
        return message.isEmpty() ? QStringLiteral("9GAG rejected the request.") : message;
    }

    if (root.value(QStringLiteral("status")).toString() == QLatin1String("ERROR")) {
        const QString message = root.value(QStringLiteral("error")).toString();
        return message.isEmpty() ? QStringLiteral("9GAG rejected the request.") : message;
    }

    return QString();
}

/*!
 * \brief NineGagApiClient::sessionIsValid Checks if the login/session is still valid.
 * \return Returns true if the session is still valid and a re-login is not needed.
 */
bool NineGagApiClient::sessionIsValid() const
{
    return !m_appToken.isEmpty() && currentSecs() < m_tokenExpiry;
}

/*!
 * \brief NineGagApiClient::isGuestSession Returns the current login state (logged in as guest or user).
 * \return Returns true if the API client has been logged in as guest.
 */
bool NineGagApiClient::isGuestSession() const
{
    return m_isGuestSession;
}

bool NineGagApiClient::isUserLoggedIn() const
{
    return (!m_isGuestSession && sessionIsValid()) || canRenewUserSession();
}

QString NineGagApiClient::userDisplayName() const
{
    return (m_isGuestSession && !canRenewUserSession()) ? QString() : m_userName;
}

QString NineGagApiClient::userId() const
{
    return userDisplayName().isEmpty() ? QString() : m_userId;
}

QString NineGagApiClient::accountId() const
{
    return userDisplayName().isEmpty() ? QString() : m_accountId;
}

bool NineGagApiClient::canComment() const
{
    if (m_isGuestSession || !sessionIsValid() || m_commentAuth.isEmpty())
        return false;
    return m_commentAuthExpiry == 0 || currentSecs() < m_commentAuthExpiry;
}

bool NineGagApiClient::canRenewUserSession() const
{
    return m_rememberLogin && !m_loginName.isEmpty() && !m_passwordHash.isEmpty();
}

void NineGagApiClient::requestSession()
{
    if (m_sessionReply != 0)
        return;     // the running request will emit sessionReady() or sessionFailed()

    if (canRenewUserSession()) {
        startSessionRequest(RenewRequest);
        return;
    }

    const bool userSessionExpired = !m_isGuestSession;
    if (userSessionExpired)
        clearUserSession(true);

    startSessionRequest(GuestRequest);

    // emit last, the receivers may start new requests right away
    if (userSessionExpired)
        emit userSessionEnded(QStringLiteral("Your 9GAG login has expired. Log in again from the app settings."));
}

void NineGagApiClient::loginUser(const QString &loginName, const QString &password, bool rememberLogin)
{
    abortSessionRequest();

    m_loginName = loginName.trimmed();
    m_passwordHash = createMD5(password);
    m_rememberLogin = rememberLogin;

    startSessionRequest(UserRequest);
}

void NineGagApiClient::logout()
{
    const bool hadRequest = abortSessionRequest();

    clearUserSession(true);
    setSession(QByteArray(), 0, true);

    // someone may be waiting for the aborted request
    if (hadRequest)
        requestSession();
}

void NineGagApiClient::invalidateSession()
{
    m_tokenExpiry = 0;
}

/*!
 * \brief NineGagApiClient::startSessionRequest Sends a /v2/guest-token or /v2/user-token request.
 */
void NineGagApiClient::startSessionRequest(SessionRequestType type)
{
    if (m_sessionReply != 0) {
        if (type == GuestRequest)
            return;     // the running request serves everyone waiting for a session
        abortSessionRequest();
    }

    QNetworkRequest netReq;

    if (type == GuestRequest) {
        netReq = apiRequest(apiUrl(GUEST_PATH), QByteArray());
    }
    else {
        ArgList args;
        args << qMakePair(QStringLiteral("loginMethod"), QStringLiteral("9gag"));
        args << qMakePair(QStringLiteral("loginName"), m_loginName);
        args << qMakePair(QStringLiteral("password"), QString::fromLatin1(m_passwordHash));
        args << qMakePair(QStringLiteral("language"), QStringLiteral("en_US"));
        args << qMakePair(QStringLiteral("pushToken"), QString::fromLatin1(createSHA1()));

        // The URL contains the password hash, so it must never be logged.
        netReq = apiRequest(apiUrl(LOGIN_PATH, args), m_appToken.isEmpty() ? createSHA1() : m_appToken);
    }

    m_sessionRequestType = type;
    m_sessionReply = request(netReq);
    connect(m_sessionReply, &QNetworkReply::finished, this, &NineGagApiClient::onSessionReplyFinished);
    m_sessionTimer->start();    // Qt 5.6 has no transfer timeout of its own
}

/*!
 * \brief NineGagApiClient::abortSessionRequest Aborts a running session request without
 *  emitting any signal.
 * \return Returns true if a request was running.
 */
bool NineGagApiClient::abortSessionRequest()
{
    if (m_sessionReply == 0)
        return false;

    m_sessionTimer->stop();
    m_sessionReply->disconnect(this);
    m_sessionReply->abort();
    m_sessionReply->deleteLater();
    m_sessionReply = 0;
    return true;
}

void NineGagApiClient::onSessionReplyFinished()
{
    QNetworkReply *reply = m_sessionReply;
    m_sessionReply = 0;
    m_sessionTimer->stop();

    if (reply == 0)
        return;

    // Don't use reply->errorString(): Qt puts the request URL into it, and the URL of a
    // /v2/user-token request contains the password hash.
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const int networkError = reply->error();
    const QByteArray body = reply->readAll();

    ResponseInfo info;
    info.server = QString::fromLatin1(reply->rawHeader("Server"));
    info.contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    info.cfMitigated = QString::fromLatin1(reply->rawHeader("cf-mitigated"));
    info.cfRay = QString::fromLatin1(reply->rawHeader("CF-RAY"));
    reply->deleteLater();

    finishSessionRequest(m_sessionRequestType, httpStatus, networkError, body, false, info);
}

void NineGagApiClient::onSessionTimeout()
{
    QNetworkReply *reply = m_sessionReply;
    if (reply == 0)
        return;

    m_sessionReply = 0;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();

    finishSessionRequest(m_sessionRequestType, 0, QNetworkReply::TimeoutError, QByteArray(), true,
                         ResponseInfo());
}

/*!
 * \brief NineGagApiClient::finishSessionRequest Evaluates the answer to a session request.
 *
 * Only a JSON answer from the API that reports an error counts as a rejected login. Anything
 * else (offline, timeout, captive portal, a Cloudflare page, rate limiting, server errors) is
 * treated as temporary, so a stored login is kept.
 */
void NineGagApiClient::finishSessionRequest(SessionRequestType type, int httpStatus, int networkError,
                                            const QByteArray &body, bool timedOut, const ResponseInfo &info)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    const bool isJson = (parseError.error == QJsonParseError::NoError && doc.isObject());
    const QJsonObject root = doc.object();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QByteArray token = data.value(QStringLiteral("userToken")).toString().toUtf8();
    const QString apiError = isJson ? apiErrorMessage(root) : QString();

    const bool success = isJson && apiError.isEmpty() && httpStatus >= 200 && httpStatus < 400
            && !token.isEmpty();
    const bool rejected = !success && isJson && !apiError.isEmpty()
            && httpStatus != 429 && httpStatus < 500;

    QString message;
    if (success)
        message = QString();
    else if (rejected)
        message = apiError;
    else if (timedOut)
        message = QStringLiteral("9GAG didn't answer in time. Check your connection and try again.");
    else if (httpStatus == 0)
        message = QStringLiteral("Couldn't connect to 9GAG (network error %1). "
                                 "Check your connection and try again.").arg(networkError);
    else if (httpStatus == 429)
        message = QStringLiteral("9GAG is limiting requests right now. Try again in a few minutes.");
    else if (httpStatus >= 500)
        message = QStringLiteral("9GAG's server has a problem (HTTP %1). Try again later.").arg(httpStatus);
    else if (!isJson && (!info.cfMitigated.isEmpty()
                         || info.server.contains(QLatin1String("cloudflare"), Qt::CaseInsensitive)))
        message = QStringLiteral("9GAG's firewall blocked the request (HTTP %1%2).")
                .arg(httpStatus)
                .arg(info.cfMitigated.isEmpty() ? QString() : QStringLiteral(", ") + info.cfMitigated);
    else
        message = QStringLiteral("9GAG sent an unexpected answer (HTTP %1). Try again later.").arg(httpStatus);

    if (!success) {
        // The body never contains the request URL in practice, but redact the hash anyway.
        QByteArray snippet = body.left(300);
        if (!m_passwordHash.isEmpty())
            snippet.replace(m_passwordHash, "<password-hash>");
        if (!m_loginName.isEmpty())
            snippet.replace(m_loginName.toUtf8(), "<login-name>");
        snippet.replace('\n', ' ').replace('\r', ' ');

        qWarning() << "NineGagApiClient: session request failed, type" << type << "HTTP" << httpStatus
                   << "network error" << networkError << "-" << message;
        qWarning() << "NineGagApiClient: answer: server" << info.server << "content-type" << info.contentType
                   << "cf-mitigated" << info.cfMitigated << "cf-ray" << info.cfRay
                   << "body" << snippet;
    }

    switch (type) {
    case GuestRequest:
        if (success) {
            setSession(token, tokenExpiryFromResponse(data, currentSecs()), true);
            emit sessionReady();
        } else {
            emit sessionFailed(message);
        }
        break;

    case UserRequest:
    case RenewRequest:
        if (success) {
            const QJsonObject user = data.value(QStringLiteral("user")).toObject();
            QString userName = user.value(QStringLiteral("loginName")).toString();
            if (userName.isEmpty())
                userName = user.value(QStringLiteral("fullName")).toString();
            if (userName.isEmpty())
                userName = m_loginName;

            m_userName = userName;
            m_userId = user.value(QStringLiteral("userId")).toVariant().toString();
            m_accountId = user.value(QStringLiteral("accountId")).toVariant().toString();

            // the key the comment service wants as 'auth' (commentAuth: type, authHash, expiryTs)
            const QJsonObject commentAuth = data.value(QStringLiteral("commentAuth")).toObject();
            m_commentAuth = commentAuth.value(QStringLiteral("authHash")).toString().toUtf8();
            m_commentAuthExpiry = jsonToInt64(commentAuth.value(QStringLiteral("expiryTs")));
            if (m_commentAuthExpiry > currentSecs() * 100)     // milliseconds
                m_commentAuthExpiry /= 1000;

            setSession(token, tokenExpiryFromResponse(data, currentSecs()), false);
            saveUserSession();

            if (type == UserRequest)
                emit userLoginFinished(true, QString());
            emit sessionReady();
        }
        else if (type == UserRequest) {
            // keep any stored login from before, but don't keep the failed one
            loadStoredSession();
            emit userLoginFinished(false, message);

            if (!sessionIsValid())
                requestSession();
        }
        else if (!rejected) {
            // keep the stored login, the problem is probably temporary
            emit sessionFailed(message);
        }
        else {
            // the stored login was rejected (password changed, account gone ...)
            clearUserSession(true);
            startSessionRequest(GuestRequest);
            emit userSessionEnded(QStringLiteral("9GAG didn't accept your saved login (%1). "
                                                 "Log in again from the app settings.").arg(message));
        }
        break;
    }
}

void NineGagApiClient::setSession(const QByteArray &token, qint64 expiry, bool guest)
{
    m_appToken = token;
    m_tokenExpiry = expiry;
    m_isGuestSession = guest;
}

/*!
 * \brief NineGagApiClient::clearUserSession Forgets the user session.
 * \param clearStoredLogin Also forget the stored login name and password hash.
 */
void NineGagApiClient::clearUserSession(bool clearStoredLogin)
{
    if (!m_isGuestSession)
        setSession(QByteArray(), 0, true);

    m_userName.clear();
    m_userId.clear();
    m_accountId.clear();
    m_commentAuth.clear();
    m_commentAuthExpiry = 0;

    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    settings.remove(KEY_USER_TOKEN);
    settings.remove(KEY_TOKEN_EXPIRY);
    settings.remove(KEY_USER_NAME);
    settings.remove(KEY_USER_ID);
    settings.remove(KEY_ACCOUNT_ID);
    settings.remove(KEY_COMMENT_AUTH);
    settings.remove(KEY_COMMENT_AUTH_EXPIRY);

    if (clearStoredLogin) {
        m_loginName.clear();
        m_passwordHash.clear();
        m_rememberLogin = false;
        settings.remove(KEY_LOGIN_NAME);
        settings.remove(KEY_PASSWORD_HASH);
    }
}

/*!
 * \brief NineGagApiClient::loadStoredSession Restores the device id, a still valid user session
 *  and the stored login from QSettings.
 */
void NineGagApiClient::loadStoredSession()
{
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);

    // Keep the device id stable, a user token may be bound to it.
    m_deviceUUID = settings.value(KEY_DEVICE_UUID).toByteArray();
    if (m_deviceUUID.size() != 32) {
        m_deviceUUID = createUUID();
        settings.setValue(KEY_DEVICE_UUID, m_deviceUUID);
    }

    m_loginName = settings.value(KEY_LOGIN_NAME).toString();
    m_passwordHash = settings.value(KEY_PASSWORD_HASH).toByteArray();
    m_rememberLogin = !m_loginName.isEmpty() && !m_passwordHash.isEmpty();

    const QByteArray token = settings.value(KEY_USER_TOKEN).toByteArray();
    const qint64 expiry = settings.value(KEY_TOKEN_EXPIRY).toLongLong();

    if (token.isEmpty())
        return;

    if (expiry > currentSecs() || m_rememberLogin) {
        m_userName = settings.value(KEY_USER_NAME).toString();
        m_userId = settings.value(KEY_USER_ID).toString();
        m_accountId = settings.value(KEY_ACCOUNT_ID).toString();
        m_commentAuth = settings.value(KEY_COMMENT_AUTH).toByteArray();
        m_commentAuthExpiry = settings.value(KEY_COMMENT_AUTH_EXPIRY).toLongLong();

        if (expiry > currentSecs())
            setSession(token, expiry, false);
        // else: expired, but it can be renewed with the stored login
    }
    else {
        // expired and can't be renewed
        settings.remove(KEY_USER_TOKEN);
        settings.remove(KEY_TOKEN_EXPIRY);
        settings.remove(KEY_USER_NAME);
    }
}

void NineGagApiClient::saveUserSession() const
{
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    settings.setValue(KEY_USER_TOKEN, m_appToken);
    settings.setValue(KEY_TOKEN_EXPIRY, m_tokenExpiry);
    settings.setValue(KEY_USER_NAME, m_userName);
    settings.setValue(KEY_USER_ID, m_userId);
    settings.setValue(KEY_ACCOUNT_ID, m_accountId);
    settings.setValue(KEY_COMMENT_AUTH, m_commentAuth);
    settings.setValue(KEY_COMMENT_AUTH_EXPIRY, m_commentAuthExpiry);

    if (m_rememberLogin) {
        settings.setValue(KEY_LOGIN_NAME, m_loginName);
        settings.setValue(KEY_PASSWORD_HASH, m_passwordHash);
    } else {
        settings.remove(KEY_LOGIN_NAME);
        settings.remove(KEY_PASSWORD_HASH);
    }
}

/*!
 * \brief appendPageArgs Adds the paging arguments every post list takes.
 */
static void appendPageArgs(QList<QPair<QString, QString> > &args, const QString &lastId)
{
    // count of posts (GagModel builds the next page from the last 9 ids)
    args << qMakePair(QStringLiteral("itemCount"), QStringLiteral("9"));

    // ToDo: disabled album and video posts until support is added | "animated,photo,video,article"
    args << qMakePair(QStringLiteral("entryTypes"), QStringLiteral("animated,photo"));

    if (!lastId.isEmpty())
        args << qMakePair(QStringLiteral("olderThan"), lastId);
}

/*!
 * \brief NineGagApiClient::getHomePosts Requests a page of one of the home feeds.
 *  Be aware that first a session has to be requested to access the servers.
 * \param type 'hot', 'trending' or 'fresh'.
 * \param lastId The ids of the last posts already shown (next page), or empty.
 */
QNetworkReply *NineGagApiClient::getHomePosts(const QString &type, const QString &lastId)
{
    ArgList args;
    args << qMakePair(QStringLiteral("group"), QStringLiteral("1"));
    args << qMakePair(QStringLiteral("type"), type);
    appendPageArgs(args, lastId);

    return this->request(apiRequest(apiUrl(POSTS_PATH, args), m_appToken));
}

/*!
 * \brief NineGagApiClient::getInterestPosts Requests a page of a 9GAG interest
 *  ('/v2/post-list/interest/humor/type/hot/...', confirmed Oct 2026).
 * \param interest The interest's list type, e.g. 'humor' or 'girlnsfw'.
 * \param lastId The ids of the last posts already shown (next page), or empty.
 */
QNetworkReply *NineGagApiClient::getInterestPosts(const QString &interest, const QString &lastId)
{
    ArgList args;
    args << qMakePair(QStringLiteral("interest"), interest);
    args << qMakePair(QStringLiteral("type"), QStringLiteral("hot"));
    appendPageArgs(args, lastId);

    return this->request(apiRequest(apiUrl(POSTS_PATH, args), m_appToken));
}

/*!
 * \brief NineGagApiClient::getTagPosts Requests a page of posts with a tag
 *  ('/v2/tag-search/query/cat/fromIndex/0/...', confirmed Oct 2026).
 * \param tag The tag as 9GAG shows it, e.g. 'old meme'.
 * \param fromIndex The number of posts already loaded.
 */
QNetworkReply *NineGagApiClient::getTagPosts(const QString &tag, int fromIndex)
{
    ArgList args;
    args << qMakePair(QStringLiteral("query"), tag.trimmed());
    args << qMakePair(QStringLiteral("fromIndex"), QString::number(fromIndex));
    args << qMakePair(QStringLiteral("itemCount"), QStringLiteral("9"));
    args << qMakePair(QStringLiteral("entryTypes"), QStringLiteral("animated,photo"));
    args << qMakePair(QStringLiteral("offset"), QStringLiteral("10"));
    args << qMakePair(QStringLiteral("sort"), QStringLiteral("asc"));

    return this->request(apiRequest(apiUrl(TAG_SEARCH_PATH, args), m_appToken));
}

/*!
 * \brief NineGagApiClient::suggestTags Requests tag suggestions for typed text
 *  ('/v2/tag-suggest/query/ca', answers data.tags[{value, count, highlighted}]).
 */
QNetworkReply *NineGagApiClient::suggestTags(const QString &text)
{
    ArgList args;
    args << qMakePair(QStringLiteral("query"), text.trimmed());

    return this->request(apiRequest(apiUrl(TAG_SUGGEST_PATH, args), m_appToken));
}

/*!
 * \brief NineGagApiClient::getComments Retrieves the comment data for a given post.
 * \param gagUrl The URL of the post for that the comments should be fetched.
 * \param count The number of comments that should be loaded.
 * \param level
 * \param refComment The commentId/orderKey of a comment to load the comments beneath that comment.
 *  A given orderKey value loads top-level comments and a commentId value loads second-level comments.
 * \param sortOrder Note that for SortOrder::Time the posts are ordered by the timestamp-value and
 *  not by the elapsed time!
 * \param sortDirection
 * \param auth
 * \return Returns the retrieved comment data.
 */
QNetworkReply *NineGagApiClient::getComments(const QUrl &gagUrl, const int count, const int level,
                                             QString refComment, SortOrder sortOrder,
                                             SortDirection sortDirection, const QString &auth)
{
    QUrl reqUrl(COMMENT_URL_CDN + COMMENT_PATH_CDN);
    QUrlQuery query;

    query.addQueryItem("appId", COMMENT_ID_CDN);
    query.addQueryItem("url", QUrl::toPercentEncoding(gagUrl.toString()));

    // Set sort order
    switch (sortOrder) {
        default:
            qWarning("NineGagApiClient::getComments(): Using invalid sort order, "
                     "default value will be used!");
            // fall through
        case SortOrder::Score:
            query.addQueryItem("order", "score");
            break;
        case SortOrder::Time:
            query.addQueryItem("order", "ts");
            break;
    }

    // Set sort direction
    switch (sortDirection) {
        default:
        qWarning("NineGagApiClient::getComments(): Using invalid sort direction, "
                 "default value will be used!");
            // fall through
        case SortDirection::Descending:
            query.addQueryItem("direction", "desc");
            break;
        case SortDirection::Ascending:
            query.addQueryItem("direction", "asc");
            break;
    }

    // set refComment
    if (!refComment.isEmpty()) {
        // 'score_*'-value (orderKey) loads further top-level comments
        // 'c_*'-value (commentId) loads further second-level comments
        query.addQueryItem("ref", refComment);
        query.addQueryItem("refCommentId", refComment);
    }

    query.addQueryItem("count", QString::number(count));
    query.addQueryItem("level", QString::number(level));
    query.addQueryItem("auth", (auth.isEmpty() ? "null" : auth));

    reqUrl.setQuery(query.query());

    QNetworkRequest netReq;
    netReq.setUrl(reqUrl);
    netReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netReq.setRawHeader("appId", COMMENT_ID_CDN);
    netReq.setRawHeader("X-Package-ID", APP_ID);
    netReq.setRawHeader("X-Device-UUID", m_deviceUUID);

    return this->request(netReq);
}

/*!
 * \brief NineGagApiClient::retrieveSections Requests the lists of 9GAG sections ('groups'),
 *  interests ('interests') and local interests ('localInterests').
 */
QNetworkReply *NineGagApiClient::retrieveSections()
{
    ArgList args;
    args << qMakePair(QStringLiteral("entryTypes"), QStringLiteral("animated,photo,video,article"));

    return this->request(apiRequest(apiUrl(SECTIONS_PATH, args), m_appToken));
}

/*!
 * \brief NineGagApiClient::formBody Encodes \p args as application/x-www-form-urlencoded.
 *  Every reserved character is percent-encoded, so a '+' or '&' in a comment stays as typed.
 */
QByteArray NineGagApiClient::formBody(const ArgList &args)
{
    QByteArray body;
    for (int i = 0; i < args.size(); ++i) {
        if (i > 0)
            body.append('&');
        body.append(QUrl::toPercentEncoding(args.at(i).first));
        body.append('=');
        body.append(QUrl::toPercentEncoding(args.at(i).second));
    }
    return body;
}

/*!
 * \brief NineGagApiClient::commentRequest A request to comment.9gag.com. The comment service
 *  takes the app identity headers but no request signature (9GAG app 8.24.7).
 */
QNetworkRequest NineGagApiClient::commentRequest(const QUrl &url) const
{
    QNetworkRequest netReq(url);
    netReq.setRawHeader("X-Package-ID", APP_ID);
    netReq.setRawHeader("X-Package-Version", APP_VERSION_CODE);
    netReq.setRawHeader("X-Device-UUID", m_deviceUUID);
    netReq.setRawHeader("User-Agent", APP_USER_AGENT);
    netReq.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    netReq.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    return netReq;
}

QNetworkReply *NineGagApiClient::addComment(const QString &postUrl, const QString &text, const QString &parentId)
{
    // the fields the app's AddCommentTask sends
    ArgList args;
    args << qMakePair(QStringLiteral("auth"), QString::fromLatin1(m_commentAuth));
    args << qMakePair(QStringLiteral("appId"), QString::fromLatin1(COMMENT_ID_CDN));
    args << qMakePair(QStringLiteral("url"), postUrl);
    args << qMakePair(QStringLiteral("text"), text);
    args << qMakePair(QStringLiteral("isAnonymous"), QString());
    if (!parentId.isEmpty())
        args << qMakePair(QStringLiteral("parent"), parentId);
    args << qMakePair(QStringLiteral("follow"), QStringLiteral("0"));
    args << qMakePair(QStringLiteral("replyThreadOnly"), QStringLiteral("0"));

    QNetworkRequest netReq = commentRequest(QUrl(QString::fromLatin1(COMMENT_URL + COMMENT_ADD_PATH)));
    netReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    return m_netMan->createPostRequest(netReq, formBody(args));
}

QNetworkReply *NineGagApiClient::deleteComment(const QString &postUrl, const QString &commentId)
{
    // the app sends these as headers on the DELETE request
    QNetworkRequest netReq = commentRequest(QUrl(QString::fromLatin1(COMMENT_URL + COMMENT_ADD_PATH) + "/"
                                                 + QString::fromLatin1(QUrl::toPercentEncoding(commentId))));
    netReq.setRawHeader("auth", m_commentAuth);
    netReq.setRawHeader("appId", COMMENT_ID_CDN);
    netReq.setRawHeader("url", postUrl.toUtf8());
    return m_netMan->createDeleteRequest(netReq);
}

QString NineGagApiClient::newUploadId() const
{
    return m_userId + "_" + QString::number(QDateTime::currentMSecsSinceEpoch());
}

/*!
 * \brief NineGagApiClient::postSubmitForm Sends one form step to /v2/post-submit.
 */
QNetworkReply *NineGagApiClient::postSubmitForm(const ArgList &args)
{
    QNetworkRequest netReq = apiRequest(QUrl::fromEncoded(API_URL + POST_SUBMIT_PATH), m_appToken);
    netReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    return m_netMan->createPostRequest(netReq, formBody(args));
}

QNetworkReply *NineGagApiClient::uploadImage(const QString &uploadId, const QString &filePath)
{
    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        return 0;
    }

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    QByteArray contentType = "image/jpeg";
    if (suffix == QLatin1String("png"))
        contentType = "image/png";
    else if (suffix == QLatin1String("gif"))
        contentType = "image/gif";
    else if (suffix == QLatin1String("webp"))
        contentType = "image/webp";

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QString("form-data; name=\"imageData\"; filename=\"upload.%1\"")
                        .arg(suffix.isEmpty() ? QStringLiteral("jpg") : suffix));
    imagePart.setBodyDevice(file);
    file->setParent(multiPart);     // deleted with the multipart
    multiPart->append(imagePart);

    const QPair<QString, QString> fields[] = {
        qMakePair(QStringLiteral("step"), QStringLiteral("imageData")),
        qMakePair(QStringLiteral("uploadId"), uploadId)
    };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QString("form-data; name=\"%1\"").arg(fields[i].first));
        part.setBody(fields[i].second.toUtf8());
        multiPart->append(part);
    }

    QNetworkRequest netReq = apiRequest(QUrl::fromEncoded(API_URL + POST_SUBMIT_PATH), m_appToken);
    netReq.setHeader(QNetworkRequest::ContentTypeHeader, QVariant());   // set by QHttpMultiPart
    return m_netMan->createPostRequest(netReq, multiPart);
}

QNetworkReply *NineGagApiClient::uploadMeta(const QString &uploadId, const QString &title,
                                            const QStringList &tags, bool nsfw)
{
    ArgList args;
    args << qMakePair(QStringLiteral("step"), QStringLiteral("metaData"));
    args << qMakePair(QStringLiteral("uploadId"), uploadId);
    args << qMakePair(QStringLiteral("moderate"), QStringLiteral("1"));
    args << qMakePair(QStringLiteral("title"), title);
    args << qMakePair(QStringLiteral("isNSFW"), nsfw ? QStringLiteral("1") : QStringLiteral("0"));
    args << qMakePair(QStringLiteral("tags"), tags.join(QStringLiteral(",")));
    return postSubmitForm(args);
}

QNetworkReply *NineGagApiClient::triggerPostCreation(const QString &uploadId)
{
    ArgList args;
    args << qMakePair(QStringLiteral("step"), QStringLiteral("triggerCreation"));
    args << qMakePair(QStringLiteral("uploadId"), uploadId);
    args << qMakePair(QStringLiteral("moderate"), QStringLiteral("1"));
    return postSubmitForm(args);
}
