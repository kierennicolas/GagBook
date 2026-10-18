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

#include "commentmodel.h"
#include "gagrequest.h"
#include "gagimagedownloader.h"

/*!
    \class CommentModel
    \since 1.4.0
    \brief The CommentModel class provides a two-dimensional item model.

    CommentModel subclasses QAbstractItemModel to represent CommentObject objects in a
    hierarchical list model structure for being used as a model within QML.
    This implements an editable model supporting inserting & removing comments and features the lazy
    population of the model to incrementally load the needed data.

    Internally the data is stored in the model using a pointer-based tree structure composed of
    CommentObject objects. All the top-level comments are children of an empty CommentObject
    (the rootComment) and each top-level comment may have children comments which correspond to the
    answers of a specific comment (second-level comments).

    Note: Currently CommentModel is just compatible with NineGagApiRequest and needs to be adapted if
    it should be used also with another GagRequest instance/type ((can)FetchMore() methods...).
*/

/*!
 * \brief CommentModel::CommentModel Constructs a CommentModel.
 * \param parent Pointer to the parent object.
 *
 * Note: After construction first you need to set the GagBookManager & GagUrl before being able to
 * use the CommentModel!
 */
CommentModel::CommentModel(QObject *parent)
    : QAbstractItemModel(parent), m_rootComment(new CommentObject()), m_isEmpty(true),
      m_gagUrl(QUrl()), m_fetchAmount(10), m_loadingStatus(LoadingStatus::Idle),
      m_sorting(Sorting::Hot), m_manager(0)
{
    m_roleNames[IdRole] = "id";
    m_roleNames[TimestampRole] = "timestamp";
    m_roleNames[UrlRole] = "url";
    m_roleNames[TextBodyRole] = "textBody";
    m_roleNames[TextTypeRole] = "textType";
    m_roleNames[MediaRole] = "media";
    m_roleNames[UpvotesRole] = "upvotes";
    m_roleNames[ChildCommentCountRole] = "childCommentCount";
    m_roleNames[IsOriginalPosterRole] = "isOriginalPoster";
    m_roleNames[UserRole] = "user";
    m_roleNames[ThreadIdRole] = "threadId";
}

/*!
 * \brief CommentModel::~CommentModel Destroys the CommentModel.
 */
CommentModel::~CommentModel()
{
    m_manager->gagRequest()->abortCommentsRequest();
    delete m_rootComment;
}

/*!
 * \brief CommentModel::index Returns the QModelIndex of the specified item in the model.
 *  To refer to top-level items, supply QModelIndex() as the parent index.
 * \param row The row of the item.
 * \param column The column of the item.
 * \param parent The parent of the item.
 * \return Returns the QModelIndex of the item in the model specified by the given row, column and parent index.
 */
QModelIndex CommentModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    CommentObject *parentObj = commentForIndex(parent);
    CommentObject *childObj = parentObj->child(row);

    if (childObj) {
        return createIndex(row, column, childObj);
    }
    else {
        return QModelIndex();
    }
}

/*!
 * \brief CommentModel::parent Returns the parent of the model item with the given index.
 * \param index The item for that the parent should be returned.
 * \return Returns the parent of the model item with the given index. If the item has no
 *  parent, an invalid QModelIndex is returned.
 */
QModelIndex CommentModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    CommentObject *childObj = commentForIndex(index);
    CommentObject *parentObj = childObj->parentComment();

    if (parentObj == m_rootComment) {
        return QModelIndex();
    }

    return createIndex(parentObj->row(), 0, parentObj);
}

/*!
 * \brief CommentModel::rowCount Returns the number of child items for the item that corresponds
 *  to the given model index, or the number of top-level items if an invalid index is specified.
 * \param parent The item for that the number of child items should be returned.
 * \return Returns the number of child items.
 */
int CommentModel::rowCount(const QModelIndex &parent) const
{
    CommentObject *parentObj = commentForIndex(parent);

    return parentObj->currentChildCount();
}

int CommentModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant CommentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    CommentObject *comment = commentForIndex(index);

    if (comment == 0 || comment == m_rootComment)
    {
        return QVariant();
    }

    switch (role) {
        case IdRole:
            return comment->id();
        case TimestampRole:
            return comment->timestamp();
        case UrlRole:
            return comment->permalink();
        case TextBodyRole:
            return comment->text();
        case TextTypeRole:
            return comment->textType();
        case MediaRole:
            return QVariant::fromValue(comment->media());
        case UpvotesRole:
            return comment->upvotes();
        case ChildCommentCountRole:
            return comment->totalChildCount();
        case IsOriginalPosterRole: {
            QString userId = m_rootComment->user().userId();
            if (userId.isEmpty())
                    return QVariant(false);
            else {
                if (userId == comment->user().userId())
                    return QVariant(true);
                else
                    return QVariant(false);
            }}
        case UserRole:
            return QVariant::fromValue(comment->user());
        case ThreadIdRole: {
            // replies are always added to the thread's top-level comment
            CommentObject *parent = comment->parentComment();
            return (parent == 0 || parent->isRootComment()) ? comment->id() : parent->id();
        }
        default:
            //qWarning() << "CommentModel::data(): Trying to retrieve data with the invalid role " << role << "!";
            return QVariant();
    }
}

bool CommentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    CommentObject *comment = commentForIndex(index);

    switch (role) {
        case IdRole:
            comment->setId(value.toString());
            break;
        case TimestampRole:
            comment->setTimestamp(value.toDateTime());
            break;
        case UrlRole:
            comment->setPermalink(value.toUrl());
            break;
        case TextBodyRole:
            comment->setText(value.toString());
            break;
        case TextTypeRole:
            comment->setTextType(value.value<ContentType>());
            break;
        case MediaRole:
            comment->setMedia(value.value<CommentMediaObject>());
        case UpvotesRole:
            comment->setUpvotes(value.toInt());
            break;
        case ChildCommentCountRole:
            comment->setTotalChildCount(value.toInt());
            break;
        case UserRole:
            comment->setUser(value.value<UserObject>());
        default:
            qWarning() << "CommentModel::setData(): Trying to set data with the invalid role " << role << "!";
            return false;
    }

    emit dataChanged(index, index, {role});
    return true;
}

Qt::ItemFlags CommentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return 0;
    }

    return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
}

/*!
 * \brief CommentModel::roleNames Exposes the roles so that they can be accessed via QML.
 * \return Returns the model's role names.
 */
QHash<int, QByteArray> CommentModel::roleNames() const
{
    return m_roleNames;
}

int CommentModel::currentCommentCount() const
{
    return m_rootComment->currentChildCount();
}

int CommentModel::totalCommentCount() const
{
    return m_rootComment->totalChildCount();
}

void CommentModel::setTotalCommentCount(int count)
{
    m_rootComment->setTotalChildCount(count);
}

bool CommentModel::isEmpty()
{
    return m_rootComment->currentChildCount() == 0;
}

QUrl CommentModel::gagUrl() const
{
    return m_gagUrl;
}

void CommentModel::setGagUrl(QUrl &url)
{
    if (m_gagUrl != url) {
        m_gagUrl = url;
    }
}

int CommentModel::fetchAmount() const
{
    return m_fetchAmount;
}

void CommentModel::setFetchAmount(int count)
{
    if ((m_fetchAmount != count) && (count >= 0))
        m_fetchAmount = count;
}

CommentModel::LoadingStatus CommentModel::loadingStatus() const
{
    return m_loadingStatus;
}

CommentModel::Sorting CommentModel::sorting() const
{
    return m_sorting;
}

void CommentModel::setSorting(const CommentModel::Sorting &sorting)
{
    if (m_sorting == sorting)
        return;

    m_sorting = sorting;
    emit sortingChanged(m_sorting);
}

GagBookManager *CommentModel::manager() const
{
    return m_manager;
}

void CommentModel::setManager(GagBookManager *manager)
{
    m_manager = manager;
}

void CommentModel::refresh(const QModelIndex &parent)
{
    if (m_manager == 0) {
        qWarning() << "CommentModel::refresh(): Error! GagBookManager has not been set yet!";
        return;
    }

    switch (m_loadingStatus) {
        case LoadingStatus::RefreshRequested:
            return;
        case LoadingStatus::Refreshing:
            // abort current requests
            m_manager->gagRequest()->abortCommentsRequest();
            break;
        case LoadingStatus::FetchMoreProcessing:
            // abort current requests
            m_manager->gagRequest()->abortCommentsRequest();
            break;
        default:
            break;
    }

    setLoadingStatus(LoadingStatus::RefreshRequested);

    fetchMore(parent);
}

// TODO
void CommentModel::downloadMedia(const QModelIndex &index)
{
    Q_UNUSED(index);
}

bool CommentModel::canFetchMore(const QModelIndex &parent) const
{
    switch (m_loadingStatus) {
    case LoadingStatus::Idle:
        break;
    case LoadingStatus::FetchMoreFailure:
        break;
    default:
        // all other states do not allow calling fetchMore()
        return false;
    }

    CommentObject *parentComment = commentForIndex(parent);
    int fetchedChildren = parentComment->currentChildCount();
    int availableChildren = parentComment->totalChildCount();

    // for initial fetch
    if ((fetchedChildren == 0) && (parentComment == m_rootComment)) {
        return true;
    }

    // TODO: problem just for the rootComment: if using NineGagApiRequest as data source then the totalChildCount also
    // contains all replies to the comments instead of just the top-level comments!!! -> Workaround:
    if (parentComment->isRootComment()) {
        return parentComment->hasMoreTopLvlComments();
    }

    return (fetchedChildren < availableChildren);
}

void CommentModel::fetchMore(const QModelIndex &parent)
{
    if (m_manager == 0) {
        qWarning() << "CommentModel::fetchMore(): Error! GagBookManager has not been set yet!";
        return;
    }

    if (m_gagUrl.isEmpty()) {
        qWarning() << "CommentMOdel::fetchMore(): Error! GagUrl has not been set yet!";
        return;
    }

    switch (m_loadingStatus) {
        case LoadingStatus::RefreshRequested:
            setLoadingStatus(LoadingStatus::Refreshing);
            resetChildren(parent);
            break;
        case LoadingStatus::Idle:
            if (m_rootComment->currentChildCount() == 0)
                setLoadingStatus(LoadingStatus::Refreshing);
            else
                setLoadingStatus(LoadingStatus::FetchMoreProcessing);

            break;
        case LoadingStatus::FetchMoreFailure:
            setLoadingStatus(LoadingStatus::FetchMoreProcessing);
            break;
        case LoadingStatus::Refreshing:
            return;
        case LoadingStatus::FetchMoreProcessing:
            return;
        default:
            return;
    }

    CommentObject *parentComment = commentForIndex(parent);

    // TODO: problem just for the rootComment: the totalChildCount also contains all replies to the comments
    // instead of just the top-level siblings!!!
    int availableChildren = parentComment->totalChildCount();
    int currentCount = parentComment->currentChildCount();
    int remainder = availableChildren - currentCount;
    int commentsToFetch;

    // on a refresh() or on the initial CommentModel population
    if ((parentComment == m_rootComment) && (currentCount == 0)) {
        commentsToFetch = m_fetchAmount;
    }
    else {
        commentsToFetch = qMin(m_fetchAmount, remainder);
    }

    // fetch more comments relative to the last child of 'parentComment'
    QString refComment = QString();
    if (currentCount > 0) {
        // top-level comment
        if (parentComment->isRootComment())
            refComment = parentComment->child(currentCount - 1)->orderKey();
        // second-level comment (answers)
        else
            refComment = parentComment->child(currentCount - 1)->id();
    }
    // TODO implement api call for (currentCount==0 && !isTopLvlComment) to be able to refresh second-level-comments

    // TODO this is only valid for NineGagApiRequest
    QVariantList params = {
        m_gagUrl,               // gagUrl
        commentsToFetch,        // count
        2,                      // level - TODO later change this to 1
        refComment,             // refComment
        m_sorting,              // Sorting
        QString()               // auth - TODO
    };

    // To keep the variable valid it has to be assured that only one network request is performed at a time!
    m_currentParentIndex = parent;

    connect(m_manager->gagRequest(), &GagRequest::fetchCommentsSuccess, this, &CommentModel::onFetchMoreFinished,
            Qt::UniqueConnection);
    connect(m_manager->gagRequest(), &GagRequest::fetchCommentsFailure, this, &CommentModel::onFetchMoreFailure,
            Qt::UniqueConnection);

    m_manager->gagRequest()->fetchComments(params, parentComment);
}

QModelIndex CommentModel::indexForComment(CommentObject *commentObj) const
{
    if (commentObj == m_rootComment) {
        return QModelIndex();
    }

    int row = rowForComment(commentObj);

    return createIndex(row, 0, commentObj);
}

CommentObject *CommentModel::commentForIndex(const QModelIndex &index) const
{
    if (index.isValid()) {
        CommentObject *comment = static_cast<CommentObject*>(index.internalPointer());

        if (comment) {
            return comment;
        }

        qWarning() << "CommentModel::commentForIndex(): Failed to cast to a CommentObject!";
    }

    return m_rootComment;
}

// returns the index position for the given commentObj (returns -1 if commentObj is 0 or has no valid parent)
int CommentModel::rowForComment(CommentObject *commentObj) const
{
    if ((commentObj == 0) || (commentObj == m_rootComment)) {
        qWarning() << "CommentModel::rowForComment(): Received an invalid CommentObject!";
        return -1;
    }

    return commentObj->parentComment()->childIndex(commentObj);
}

// Inserts a single comment at the given row in the child items of the parent specified.
bool CommentModel::insertComment(int row, const QModelIndex &parent, CommentObject *newComment)
{
    if (newComment == nullptr)
        return false;

    CommentObject *parentComment = commentForIndex(parent);

    beginInsertRows(parent, row, row);
    parentComment->insertChild(row, newComment);
    endInsertRows();

    return true;
}

bool CommentModel::appendComment(const QModelIndex &parent, CommentObject *newComment)
{
    if (newComment == nullptr)
        return false;

    CommentObject *parentComment = commentForIndex(parent);
    int pos = parentComment->currentChildCount();

    beginInsertRows(parent, pos, pos);
    parentComment->appendChild(newComment);
    endInsertRows();

    return true;
}

bool CommentModel::appendComments(const QModelIndex &parent, QList<CommentObject *> &commentList)
{
    if (commentList.isEmpty())
        return false;

    CommentObject *parentComment = commentForIndex(parent);

    int begin = parentComment->currentChildCount();
    int end = begin + commentList.count() - 1;

    beginInsertRows(parent, begin, end);
    parentComment->appendChildren(commentList);
    endInsertRows();

    return true;
}

// Note: It is not possible to remove the root comment (i.e. if index==QModelIndex())
bool CommentModel::removeComment(const QModelIndex &index)
{
    if (!index.isValid())
        return false;

    const QModelIndex parent = index.parent();
    CommentObject *parentComment = commentForIndex(parent);
    int pos = index.row();

    beginRemoveRows(parent, pos, pos);
    parentComment->removeChild(pos);
    endRemoveRows();

    return true;
}

// pass QModelIndex() to reset the whole model
void CommentModel::resetChildren(const QModelIndex &parent)
{
    CommentObject *parentComment = commentForIndex(parent);

    if (parentComment == m_rootComment) {

        if (m_rootComment->currentChildCount() != 0) {
            beginResetModel();
            m_rootComment->removeAllChildren();
            endResetModel();

            updateIsEmpty();
        }
    }
    else {
        int childCount = parentComment->currentChildCount();

        if (childCount != 0) {
            beginRemoveRows(parent, 0, childCount - 1);
            parentComment->removeAllChildren();
            endRemoveRows();
        }
    }
}

void CommentModel::updateIsEmpty()
{
    bool value = m_rootComment->currentChildCount() == 0;

    if (m_isEmpty == value)
        return;

    m_isEmpty = value;
    isEmptyChanged(m_isEmpty);
}

void CommentModel::setLoadingStatus(CommentModel::LoadingStatus status)
{
    if (m_loadingStatus == status)
        return;

    m_loadingStatus = status;
    emit loadingStatusChanged(m_loadingStatus);
}

void CommentModel::onFetchMoreFinished(const QList<CommentObject *> &commentList/*, const QModelIndex &parentIndex*/)
{
    // TODO the response can also be empty if there were parse errors! -> Improve the error handling
    // no comments available
    if (commentList.isEmpty()) {
        updateIsEmpty();
        setLoadingStatus(LoadingStatus::Idle);
        return;
    }

    CommentObject *parentComment = commentForIndex(m_currentParentIndex);

    int begin = parentComment->currentChildCount();
    int end = begin + commentList.count() - 1;

    beginInsertRows(m_currentParentIndex, begin, end);
    parentComment->appendChildren(commentList);
    endInsertRows();

    if (parentComment == m_rootComment)
        updateIsEmpty();

    setLoadingStatus(LoadingStatus::Idle);
}

void CommentModel::onFetchMoreFailure(const QString &errorString)
{
    switch (m_loadingStatus) {
        case LoadingStatus::FetchMoreProcessing:
            setLoadingStatus(LoadingStatus::FetchMoreFailure);
            break;
        case LoadingStatus::Refreshing:
            setLoadingStatus(LoadingStatus::RefreshFailure);
            break;
        default:
            qWarning() << "CommentModel::onFetchMoreFailure(): Got an unexpected state: " << m_loadingStatus;
            break;
    }

    emit loadingFailure(errorString);
}
