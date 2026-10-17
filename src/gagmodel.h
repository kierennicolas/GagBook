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

#ifndef GAGMODEL_H
#define GAGMODEL_H

#include <QtCore/QAbstractListModel>
#include <QtQml/QQmlParserStatus>

#include "gagobject.h"
#include "gagfeed.h"

class GagBookManager;
class GagImageDownloader;

/*! \brief The GagModel class subclasses QAbstractListModel and contains the GagObjects. This list
 * model is used within QML. */
class GagModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    /*! True if there is an active refresh request. Busy visual feedback should be shown to
        the user and the refresh option should be disabled. */
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

    /*! The current progress of the file downloads. Value between 0.0 to 1.0. */
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

    /*! The current progress of a manual file download (using downloadImage()).
        Value between 0.0 to 1.0. */
    Q_PROPERTY(qreal manualProgress READ manualProgress NOTIFY manualProgressChanged)

    /*! Set the global instance of GagBookManager. Must be set before component completed
        and can not be change afterward. */
    Q_PROPERTY(GagBookManager *manager READ manager WRITE setManager)

    /*! The selected 9GAG section to get the gags. */
    Q_PROPERTY(int selectedSection READ selectedSection WRITE setSelectedSection NOTIFY selectedSectionChanged)

    /*! The tag whose posts are shown, empty while a section is shown. \sa showTag() */
    Q_PROPERTY(QString tag READ tag NOTIFY feedChanged)

    /*! Title of what is shown, e.g. "Humor" or "#old meme". */
    Q_PROPERTY(QString feedTitle READ feedTitle NOTIFY feedChanged)
public:
    enum Roles {
        TitleRole = Qt::UserRole +1,
        IdRole,
        UrlRole,
        ImageUrlRole,
        FullImageUrlRole,
        GifImageUrlRole,
        VideoUrlRole,
        ImageSizeRole,
        VotesCountRole,
        CommentsCountRole,
        LikesRole,
        IsNSFWRole,
        IsGIFRole,
        IsVideoRole,
        IsPartialImageRole,
        SavedFileUrlRole,
        IsDownloadingRole
    };

    enum RefreshType {
        RefreshAll,
        RefreshOlder
    };

    Q_ENUM(RefreshType)

    explicit GagModel(QObject *parent = 0);

    void classBegin();
    void componentComplete();

    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

    bool isBusy() const;
    qreal progress() const;
    qreal manualProgress() const;

    GagBookManager *manager() const;
    void setManager(GagBookManager *manager);

    int selectedSection() const;
    void setSelectedSection(int selectedSection);

    QString tag() const;
    QString feedTitle() const;

    /*! Show the posts with the tag \p tag instead of the selected section. */
    Q_INVOKABLE void showTag(const QString &tag);

    /*! Refresh the gag list. */
    Q_INVOKABLE void refresh(RefreshType refreshType);
    /*! Stop and abort the refresh request. */
    Q_INVOKABLE void stopRefresh();
    /*! Download the corresponding image/file for a gag at index \p i. */
    Q_INVOKABLE void downloadImage(int i);
    /*! Change the `likes` of a gag with the \p id. */
    Q_INVOKABLE void changeLikes(const QString &id, int likes);

signals:
    void busyChanged();
    void progressChanged();
    void manualProgressChanged();
    void selectedSectionChanged();
    void feedChanged();

    /*! Emit when the refresh failed, \p errorMessage contains the reason for the
        failure and should be shown to the user. */
    void refreshFailure(const QString &errorMessage);

private slots:
    void startRequest();
    void onEndOfList();
    void onSuccess(const QList<GagObject> &gagList);
    void onFailure(const QString &errorMessage);
    void onDownloadProgress(qint64 downloaded, qint64 total);
    void onDownloadFinished();
    void onManualDownloadProgress(qint64 downloaded, qint64 total);
    void onManualDownloadFinished();
    void onSectionsReset();

private:
    GagFeed sectionFeed(int row) const;

    GagFeed m_feed;
    QString m_lastId;
    int m_fromIndex;
    int m_selectedSection;
    QString m_selectedKind;     // kind and key of the selected section, to find it again
    QString m_selectedKey;      // after the section list was updated
    QString m_tag;
    bool m_busy;
    qreal m_progress;
    qreal m_manualProgress;
    GagBookManager *m_manager;

    QHash<int, QByteArray> _roles;
    QList<GagObject> m_gagList;
    GagImageDownloader *m_imageDownloader;
    GagImageDownloader *m_manualImageDownloader;
    int m_downloadingIndex;
};

#endif // GAGMODEL_H
