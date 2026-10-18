/*
 * Copyright (C) 2019 Alexander Seibel.
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

#ifndef COMMENTMODEL_H
#define COMMENTMODEL_H

#include <QDebug>
#include <QObject>
#include <QAbstractItemModel>

#include "commentobject.h"
#include "gagbookmanager.h"

class CommentModel : public QAbstractItemModel
{
    Q_OBJECT

    Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY isEmptyChanged)

    Q_PROPERTY(QUrl gagUrl READ gagUrl WRITE setGagUrl)

    Q_PROPERTY(LoadingStatus loadingStatus READ loadingStatus WRITE setLoadingStatus NOTIFY loadingStatusChanged)

    Q_PROPERTY(Sorting sorting READ sorting WRITE setSorting NOTIFY sortingChanged)

    /*! Set the global instance of GagBookManager. Must be set before component completed
        and can not be changed afterwards. */
    Q_PROPERTY(GagBookManager *manager READ manager WRITE setManager)

public:
    enum Roles {
        IdRole = Qt::UserRole +1,
        TimestampRole,
        UrlRole,
        TextBodyRole,
        TextTypeRole,
        MediaRole,
        UpvotesRole,
        ChildCommentCountRole,
        IsOriginalPosterRole,
        UserRole,
        ThreadIdRole    //!< id of the top-level comment of the thread, the parent for replies
    };

    enum LoadingStatus {
        Idle,
        FetchMoreProcessing,
        FetchMoreFailure,
        RefreshRequested,
        Refreshing,
        RefreshFailure
    };

    enum Sorting {
        Hot,
        Fresh
    };

    Q_ENUM(LoadingStatus)
    Q_ENUM(Sorting)

    explicit CommentModel(QObject *parent = 0);
    ~CommentModel();

    /* Reimplementations needed for a read-only model */
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;

    /* Reimplementations to enable editing in the model */
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE int currentCommentCount() const;

    // TODO make it a Q_PROPERTY?
    Q_INVOKABLE int totalCommentCount() const;
    void setTotalCommentCount(int count);

    bool isEmpty();

    QUrl gagUrl() const;
    void setGagUrl(QUrl &url);

    int fetchAmount() const;
    void setFetchAmount(int count);

    LoadingStatus loadingStatus() const;

    Sorting sorting() const;
    void setSorting(const Sorting &sorting);

    GagBookManager *manager() const;
    void setManager(GagBookManager *manager);

    Q_INVOKABLE void refresh(const QModelIndex &parent = QModelIndex());

    Q_INVOKABLE void downloadMedia(const QModelIndex &index);

signals:
    void isEmptyChanged(bool value);
    void loadingStatusChanged(LoadingStatus status);
    void sortingChanged(Sorting sorting);
    void loadingFailure(QString errorMsg);

protected:
    /* Reimplementations for a lazy population of the model data */
    bool canFetchMore(const QModelIndex &parent) const;
    void fetchMore(const QModelIndex &parent);

    /* Helper methods */

    QModelIndex indexForComment(CommentObject *commentObj) const;
    CommentObject *commentForIndex(const QModelIndex &index) const;
    int rowForComment(CommentObject *commentObj) const;

    bool insertComment(int row, const QModelIndex &parent, CommentObject *newComment);
    bool appendComment(const QModelIndex &parent, CommentObject *newComment);
    bool appendComments(const QModelIndex &parent, QList<CommentObject *> &commentList);
    bool removeComment(const QModelIndex &index);
    void resetChildren(const QModelIndex &parent);

    void updateIsEmpty();

protected slots:
    void setLoadingStatus(LoadingStatus status);
    void onFetchMoreFinished(const QList<CommentObject *> &commentList);
    void onFetchMoreFailure(const QString &errorString);

private:
    CommentObject *m_rootComment;
    bool m_isEmpty;
    QUrl m_gagUrl;
    int m_fetchAmount;
    QHash<int, QByteArray> m_roleNames;
    LoadingStatus m_loadingStatus;
    Sorting m_sorting;

    QModelIndex m_currentParentIndex;

    GagBookManager *m_manager;
};

#endif // COMMENTMODEL_H
