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

#include "gagmodel.h"

#include <QtCore/QUrl>
#include <QtCore/QSize>
#include <QtCore/QSet>

#include "gagbookmanager.h"
#include "appsettings.h"
#include "networkmanager.h"
#include "gagrequest.h"
#include "gagimagedownloader.h"
#include "sectionmodel.h"

GagModel::GagModel(QObject *parent) :
    QAbstractListModel(parent), m_lastId(QString()), m_fromIndex(0),
    m_selectedSection(0), m_busy(false), m_progress(0), m_manualProgress(0), m_manager(0),
    m_gagList(QList<GagObject>()), m_imageDownloader(0), m_manualImageDownloader(0), m_downloadingIndex(-1)
{
    _roles[TitleRole] = "title";
    _roles[IdRole] = "id";
    _roles[UrlRole] = "url";
    _roles[ImageUrlRole] = "imageUrl";
    _roles[FullImageUrlRole] = "fullImageUrl";
    _roles[GifImageUrlRole] = "gifImageUrl";
    _roles[VideoUrlRole] = "videoUrl";
    _roles[ImageSizeRole] = "imageSize";
    _roles[VotesCountRole] = "votesCount";
    _roles[CommentsCountRole] = "commentsCount";
    _roles[LikesRole] = "likes";
    _roles[IsNSFWRole] = "isNSFW";
    _roles[IsGIFRole] = "isGIF";
    _roles[IsVideoRole] = "isVideo";
    _roles[IsPartialImageRole] = "isPartialImage";
    _roles[SavedFileUrlRole] = "savedFileUrl";
    _roles[IsDownloadingRole] = "isDownloading";
}

void GagModel::classBegin()
{
}

void GagModel::componentComplete()
{
    if (m_manager != 0 && m_manager->settings() != 0) {
        // keep the selection when the section list is replaced with 9GAG's current one
        connect(m_manager->settings()->sections(), &QAbstractItemModel::modelReset,
                this, &GagModel::onSectionsReset, Qt::UniqueConnection);
    }

    refresh(RefreshAll);
}

GagFeed GagModel::sectionFeed(int row) const
{
    SectionModel *sections = m_manager->settings()->sections();
    const QModelIndex index = sections->index(row, 0, QModelIndex());
    const QString kind = sections->data(index, SectionModel::KindRole).toString();
    const QString key = sections->data(index, SectionModel::UrlPathRole).toString();

    if (kind == QLatin1String("interest"))
        return GagFeed(GagFeed::Interest, key);
    return GagFeed(GagFeed::Home, key.isEmpty() ? QStringLiteral("hot") : key);
}

void GagModel::onSectionsReset()
{
    SectionModel *sections = m_manager->settings()->sections();
    int row = sections->indexOf(m_selectedKind, m_selectedKey);
    if (row < 0)
        row = 0;

    if (row != m_selectedSection) {
        m_selectedSection = row;
        emit selectedSectionChanged();
    }
    emit feedChanged();
}

QString GagModel::tag() const
{
    return m_tag;
}

QString GagModel::feedTitle() const
{
    if (!m_tag.isEmpty())
        return QStringLiteral("#") + m_tag;
    if (m_manager == 0 || m_manager->settings() == 0)
        return QString();

    SectionModel *sections = m_manager->settings()->sections();
    return sections->data(sections->index(m_selectedSection, 0, QModelIndex()),
                          SectionModel::NameRole).toString();
}

void GagModel::showTag(const QString &tag)
{
    const QString trimmed = tag.trimmed();
    if (trimmed.isEmpty())
        return;

    m_tag = trimmed;
    emit feedChanged();
    refresh(RefreshAll);
}

int GagModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_gagList.count();
}

QVariant GagModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(index.row() < m_gagList.count());

    const GagObject &gag = m_gagList.at(index.row());

    switch (role) {
    case TitleRole:
        return gag.title();
    case IdRole:
        return gag.id();
    case UrlRole:
        return gag.url();
    case ImageUrlRole:
        if (!gag.imageUrl().isLocalFile())
            return QUrl();
        return gag.imageUrl();
    case FullImageUrlRole:
        if (!gag.fullImageUrl().isLocalFile())
            return QUrl();
        return gag.fullImageUrl();
    case GifImageUrlRole:
        if (!gag.gifImageUrl().isLocalFile())
            return QUrl();
        return gag.gifImageUrl();
    case VideoUrlRole:
        if (!gag.videoUrl().isLocalFile())
            return QUrl();
        return gag.videoUrl();
    case ImageSizeRole:
        return gag.imageSize();
    case VotesCountRole:
        return gag.votesCount();
    case CommentsCountRole:
        return gag.commentsCount();
    case LikesRole:
        return gag.likes();
    case IsNSFWRole:
        return gag.isNSFW();
    case IsGIFRole:
        return gag.isGIF();
    case IsVideoRole:
        return gag.isVideo();
    case IsPartialImageRole:
        return gag.isPartialImage();
    case SavedFileUrlRole:
        if (!gag.savedFileUrl().isLocalFile())
            return QUrl();
        return gag.savedFileUrl();
    case IsDownloadingRole:
        return index.row() == m_downloadingIndex;
    default:
        qWarning("GagModel::data(): Invalid role");
        return QVariant();
    }
}

QHash<int, QByteArray> GagModel::roleNames() const
{
    return _roles;
}

Qt::ItemFlags GagModel::flags(const QModelIndex &index) const
{
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

bool GagModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    bool success = false;
    QVector<int> roles;
    roles << role;

    if (index.isValid() && index.row() < m_gagList.count())
    {
        switch (role) {
            case Roles::SavedFileUrlRole:
                m_gagList[index.row()].setSavedFileUrl(value.toUrl());
                success = true;
                break;
        }
    }

    if (success)
        emit dataChanged(index, index, roles);

    return success;
}

bool GagModel::isBusy() const
{
    return m_busy;
}

qreal GagModel::progress() const
{
    return m_progress;
}

qreal GagModel::manualProgress() const
{
    return m_manualProgress;
}

GagBookManager *GagModel::manager() const
{
    return m_manager;
}

void GagModel::setManager(GagBookManager *manager)
{
    m_manager = manager;
}

int GagModel::selectedSection() const
{
    return m_selectedSection;
}

void GagModel::setSelectedSection(int selectedSection)
{
    const bool tagShown = !m_tag.isEmpty();
    m_tag.clear();      // choosing a section leaves the tag

    if (m_selectedSection != selectedSection) {
        m_selectedSection = selectedSection;
        emit selectedSectionChanged();
        emit feedChanged();
    } else if (tagShown) {
        emit feedChanged();
    }
}

void GagModel::refresh(RefreshType refreshType)
{
    if (m_busy != true) {
        m_busy = true;
        emit busyChanged();
    }

    if (m_progress != 0) {
        m_progress = 0;
        emit progressChanged();
    }

    if (m_imageDownloader != 0) {
        m_imageDownloader->disconnect();
        m_imageDownloader->deleteLater();
        m_imageDownloader = 0;
    }

    if (m_manager == 0) {
        qWarning("GagModel::refresh(): Error! GagBookManager has not been set yet!");
        return;
    }

    if (refreshType == RefreshAll) {

        if (!m_tag.isEmpty()) {
            m_feed = GagFeed(GagFeed::Tag, m_tag);
        } else {
            m_feed = sectionFeed(m_selectedSection);

            SectionModel *sections = m_manager->settings()->sections();
            const QModelIndex index = sections->index(m_selectedSection, 0, QModelIndex());
            m_selectedKind = sections->data(index, SectionModel::KindRole).toString();
            m_selectedKey = sections->data(index, SectionModel::UrlPathRole).toString();
        }

        m_lastId = QString();
        m_fromIndex = 0;

        connect(m_manager->gagRequest(), SIGNAL(readyToRequestGags()), this, SLOT(startRequest()),
                Qt::UniqueConnection);
    }

    // Connect the results before the request starts: getting a session can already fail,
    // and that failure has to stop the busy state too.
    GagRequest *gagReq = m_manager->gagRequest();
    connect(gagReq, SIGNAL(fetchGagsSuccess(QList<GagObject>)), this, SLOT(onSuccess(QList<GagObject>)),
            Qt::UniqueConnection);
    connect(gagReq, SIGNAL(fetchGagsFailure(QString)), this, SLOT(onFailure(QString)), Qt::UniqueConnection);
    connect(gagReq, &GagRequest::reachedEndOfList, this, &GagModel::onEndOfList, Qt::UniqueConnection);

    if (!m_gagList.isEmpty()) {
        if (refreshType == RefreshAll) {
            beginRemoveRows(QModelIndex(), 0, m_gagList.count() - 1);
            m_gagList.clear();
            endRemoveRows();
        } else {
            // tags page by position, the others by the ids of the last posts
            m_fromIndex = m_gagList.count();

            m_lastId = "";

            const int end = m_gagList.size();
            const int start = (end < 9 ? 0 : end - 9);
            QString id;

            for (int i = start; i < end; i++) {
                id = m_gagList.at(i).id();
                m_lastId.append(id);

                if (i < (end - 1))
                    m_lastId.append(",");
            }
        }
    }

    m_manager->gagRequest()->initiateGagsRequest();
}

void GagModel::stopRefresh()
{
    if (m_imageDownloader != 0)
        m_imageDownloader->stop();

    if (m_busy != false) {
        m_busy = false;
        emit busyChanged();
    }
}

void GagModel::downloadImage(int i)
{
    if (m_manualImageDownloader != 0) {
        m_manualImageDownloader->disconnect();
        m_manualImageDownloader->deleteLater();
        m_manualImageDownloader = 0;

        if (m_downloadingIndex != -1) {
            QModelIndex modelIndex = index(m_downloadingIndex);
            m_downloadingIndex = -1;
            emit dataChanged(modelIndex, modelIndex);
        }
    }

    m_downloadingIndex = i;
    emit dataChanged(index(i), index(i));

    if (m_manualProgress != 0) {
        m_manualProgress = 0;
        emit manualProgressChanged();
    }

    QList<GagObject> gags;
    gags.append(m_gagList.at(i));

    m_manualImageDownloader = new GagImageDownloader(manager()->networkManager(), this);
    m_manualImageDownloader->setGagList(gags);
    m_manualImageDownloader->setDownloadVideo(gags.first().isVideo());
    m_manualImageDownloader->setDownloadGIF(gags.first().isGIF());
    m_manualImageDownloader->setDownloadPartialImage(gags.first().isPartialImage());
    connect(m_manualImageDownloader, SIGNAL(downloadProgress(qint64,qint64)),
            SLOT(onManualDownloadProgress(qint64,qint64)));
    connect(m_manualImageDownloader, SIGNAL(finished()), SLOT(onManualDownloadFinished()));
    m_manualImageDownloader->start();
}

void GagModel::changeLikes(const QString &id, int likes)
{
    for (int i = 0; i < m_gagList.count(); ++i) {
        GagObject gag = m_gagList.at(i);
        if (gag.id() == id) {
            int oldLikes = gag.likes();
            gag.setLikes(likes);
            gag.setVotesCount(gag.votesCount() + (likes - oldLikes));
            emit dataChanged(index(i), index(i));
            break;
        }
    }
}

void GagModel::startRequest()
{
    m_manager->gagRequest()->fetchGags(m_feed, m_lastId, m_fromIndex);
}

void GagModel::onEndOfList()
{
    emit refreshFailure("Reached end of the list. There are no further posts available");

    if (m_busy != false) {
        m_busy = false;
        emit busyChanged();
    }
}

void GagModel::onSuccess(const QList<GagObject> &gagList)
{
    // a next page can repeat posts (e.g. tag search while new posts arrive), skip those
    QSet<QString> shownIds;
    foreach (const GagObject &gag, m_gagList)
        shownIds.insert(gag.id());

    QList<GagObject> newGags;
    foreach (const GagObject &gag, gagList) {
        if (!shownIds.contains(gag.id())) {
            shownIds.insert(gag.id());
            newGags.append(gag);
        }
    }

    if (newGags.isEmpty()) {
        onEndOfList();
        return;
    }

    m_imageDownloader = new GagImageDownloader(manager()->networkManager(), this);
    m_imageDownloader->setGagList(newGags);
    m_imageDownloader->setDownloadGIF(false);
    connect(m_imageDownloader, SIGNAL(downloadProgress(qint64,qint64)), this,
            SLOT(onDownloadProgress(qint64,qint64)), Qt::UniqueConnection);
    connect(m_imageDownloader, SIGNAL(finished()), this,
            SLOT(onDownloadFinished()), Qt::UniqueConnection);
    m_imageDownloader->start();
}

void GagModel::onFailure(const QString &errorMessage)
{
    emit refreshFailure(errorMessage);

    if (m_busy != false) {
        m_busy = false;
        emit busyChanged();
    }
}

void GagModel::onDownloadProgress(qint64 downloaded, qint64 total)
{
    qreal progress;
    if (total > 0)
        progress = qreal(downloaded) / qreal(total);
    else
        progress = 1;

    if (m_progress != progress) {
        m_progress = progress;
        emit progressChanged();
    }
}

void GagModel::onDownloadFinished()
{
    const QList<GagObject> gagList = m_imageDownloader->gagList();
    beginInsertRows(QModelIndex(), m_gagList.count(), m_gagList.count() + gagList.count() - 1);
    m_gagList.reserve(m_gagList.count() + gagList.count());
    m_gagList.append(gagList);
    endInsertRows();

    if (m_busy != false) {
        m_busy = false;
        emit busyChanged();
    }

    Q_ASSERT(m_imageDownloader != 0);
    m_imageDownloader->disconnect();
    m_imageDownloader->deleteLater();
    m_imageDownloader = 0;
}

void GagModel::onManualDownloadProgress(qint64 downloaded, qint64 total)
{
    qreal progress;
    if (total > 0)
        progress = qreal(downloaded) / qreal(total);
    else
        progress = 1;

    if (m_manualProgress != progress) {
        m_manualProgress = progress;
        emit manualProgressChanged();
    }
}

void GagModel::onManualDownloadFinished()
{
    QModelIndex modelIndex = index(m_downloadingIndex);
    m_downloadingIndex = -1;
    emit dataChanged(modelIndex, modelIndex);

    m_manualImageDownloader->deleteLater();
    m_manualImageDownloader = 0;
}
