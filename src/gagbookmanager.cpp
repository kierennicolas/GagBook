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

#include "networkmanager.h"
#include "gagimagedownloader.h"
#include "appsettings.h"
#include "ninegagapiclient.h"
#include "ninegagapirequest.h"

GagBookManager::GagBookManager(QObject *parent) :
    QObject(parent), m_isBusy(false), m_settings(0),
    m_netManager(new NetworkManager(this)), m_apiClient(0), m_gagRequest(0)
{
    GagImageDownloader::initializeCache();
    connect(m_netManager, SIGNAL(downloadCounterChanged()), SIGNAL(downloadCounterChanged()));

    m_apiClient = new NineGagApiClient(m_netManager, this);
    connect(m_apiClient, &NineGagApiClient::userLoginFinished, this, &GagBookManager::onUserLoginFinished);
    connect(m_apiClient, &NineGagApiClient::userSessionEnded, this, &GagBookManager::onUserSessionEnded);
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
