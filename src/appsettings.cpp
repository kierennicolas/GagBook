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

#include "appsettings.h"

#include <QtCore/QSettings>

// version of the stored section list, see readSettings()
static const int SECTIONS_VERSION = 2;

AppSettings::AppSettings(QObject *parent) :
    QObject(parent), m_settings(new QSettings(this)), m_sections(new SectionModel(this))
{
    readSettings();
}

void AppSettings::readSettings()
{
    QString version = m_settings->value("version", "0.0").toString();

    // reset deprecated (or missing) config file
    if (!m_settings->contains("version") || (version != QString("0.2"))) {
        setDefaultSettings();
        return;
    }

    m_loggedIn = m_settings->value("loggedIn", false).toBool();
    m_whiteTheme = m_settings->value("whiteTheme", false).toBool();
    m_source = static_cast<Source>(m_settings->value("source", 0).toInt());
    m_scrollWithVolumeKeys = m_settings->value("scrollWithVolumeKeys", false).toBool();
    m_sections->restore(m_settings, "sections");

    // 1.6.0 replaced the 2019 sections with 9GAG's interests
    if (m_sections->isEmpty() || m_settings->value("sectionsVersion").toInt() < SECTIONS_VERSION) {
        m_sections->setDefaultSections();
        saveSections();
    }
}

void AppSettings::saveSections()
{
    m_settings->remove("sections");
    m_sections->save(m_settings, "sections");
    m_settings->setValue("sectionsVersion", SECTIONS_VERSION);
}

void AppSettings::setDefaultSettings()
{
    // Reset everything except the API session kept by NineGagApiClient (device id, login).
    // That group is written before AppSettings exists, so clear() would wipe the device id.
    foreach (const QString &key, m_settings->childKeys())
        m_settings->remove(key);
    foreach (const QString &group, m_settings->childGroups()) {
        if (group != QLatin1String("apiSession"))
            m_settings->remove(group);
    }

    m_settings->setValue("version", "0.2");

    m_loggedIn = false;
    m_settings->setValue("loggedIn", m_loggedIn);

    m_whiteTheme = false;

    m_source = Source::NineGagApiSource;
    m_settings->setValue("source", static_cast<int>(m_source));

    m_scrollWithVolumeKeys = false;
    m_settings->setValue("scrollWithVolumeKeys", m_scrollWithVolumeKeys);

    m_sections->setDefaultSections();
    saveSections();

    m_settings->sync();
}

bool AppSettings::isLoggedIn() const
{
    return m_loggedIn;
}

void AppSettings::setLoggedIn(bool isLoggedIn)
{
    if (m_loggedIn != isLoggedIn) {
        m_loggedIn = isLoggedIn;
        m_settings->setValue("loggedIn", m_loggedIn);
        emit loggedInChanged();
    }
}

bool AppSettings::isWhiteTheme() const
{
    return m_whiteTheme;
}

void AppSettings::setWhiteTheme(bool whiteTheme)
{
    if (m_whiteTheme != whiteTheme) {
        m_whiteTheme = whiteTheme;
        m_settings->setValue("whiteTheme", m_whiteTheme);
        emit whiteThemeChanged();
    }
}

AppSettings::Source AppSettings::source() const
{
    return m_source;
}

void AppSettings::setSource(Source source)
{
    if (m_source != source) {
        m_source = source;
        m_settings->setValue("source", static_cast<int>(m_source));
        emit sourceChanged();
    }
}

bool AppSettings::scrollWithVolumeKeys() const
{
    return m_scrollWithVolumeKeys;
}

void AppSettings::setScrollWithVolumeKeys(bool scrollWithVolumeKeys)
{
    if (m_scrollWithVolumeKeys != scrollWithVolumeKeys) {
        m_scrollWithVolumeKeys = scrollWithVolumeKeys;
        m_settings->setValue("scrollWithVolumeKeys", m_scrollWithVolumeKeys);
        emit scrollWithVolumeKeysChanged();
    }
}

SectionModel *AppSettings::sections() const
{
    return m_sections;
}

void AppSettings::setSections(const SectionModel *sections)
{
    Q_UNUSED(sections);
//    if (m_sections != sections) {
//        m_sections = sections;
//        m_settings->setValue("sections", m_sections);
//        emit sectionsChanged();
//    }
}
