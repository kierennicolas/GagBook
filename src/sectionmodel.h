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

#ifndef SECTIONMODEL_H
#define SECTIONMODEL_H

#include <QAbstractListModel>
#include <QSettings>
#include <QList>

/*!
 * \brief The Section class represents a 9GAG section.
 */
class Section
{
public:
    /*!
     * \brief Section Constructor for a 9GAG section object.
     * \param name The name of the 9GAG section in plaintext, which will be shown to the user.
     * \param urlPath The 9GAG section string, as shown in the url of a browser session (e.g. 'hot').
     * \param groupId The id to select between the different 9GAG sections/groups (only for NineGagApiSource).
     * \param listType Currently unsupported.
     * \param listType2 Currently unsupported.
     * \param isSensitive Determines if a post/gag contains sensitive content (NSFW/NFK).
     */
    Section(const QString &name, const QString &urlPath, const int &groupId, const QString &listType,
            const QString &listType2, bool isSensitive);

    /*!
     * \brief Section Overloaded constructor.
     */
    Section(const QString &name, const QString &urlPath, const int &groupId, bool isSensitive);

    /*!
     * \brief Section Overloaded constructor.
     */
    Section(const QString &name, const QString &urlPath, bool isSensitive);

    /*!
     * \brief Section Constructor for a home feed ('hot', 'trending', 'fresh') or a 9GAG interest.
     * \param name The name shown to the user.
     * \param key For a home feed its type, for an interest its list type (e.g. 'humor').
     * \param kind "home" or "interest".
     * \param category The heading the section is listed under, e.g. "Interests".
     * \param isSensitive True for interests with sensitive content (NSFW).
     */
    static Section feed(const QString &name, const QString &key, const QString &kind,
                        const QString &category, bool isSensitive);

    QString name() const;
    QString urlPath() const;
    int groupId() const;
    QString listType() const;
    QString listType2() const;
    bool isSensitive() const;
    QString kind() const;
    QString category() const;

private:
    QString m_name;
    QString m_urlPath;
    int m_groupId;
    QString m_listType;
    QString m_listType2;
    bool m_isSensitive;
    QString m_kind;
    QString m_category;
};

/*!
 * \brief The SectionModel class subclasses QAbstractListModel to represent the 9GAG sections in a list model,
 *        which is also used as a model within QML.
 */
class SectionModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /*!
     * \brief The SectionRoles enum defines custom roles.
     */
    enum SectionRoles {
        NameRole = Qt::UserRole + 1,
        UrlPathRole,
        GroupIdRole,
        ListTypeRole,
        ListType2Role,
        IsSensitiveRole,
        KindRole,
        CategoryRole
    };

    /*!
     * \brief SectionModel Constructor.
     * \param parent Pointer to the parent object.
     */
    explicit SectionModel(QObject *parent = 0);

    /*!
     * \brief addSection Appends a Section object to the list.
     * \param section The Section object which should be appended to the list.
     */
    void addSection(const Section &section);

    /*!
     * \brief rowCount Returns the total number of items in the model.
     * \param parent Reference to the QModelIndex parent object.
     * \return Returns the total number of items in the list.
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    /*!
     * \brief data Returns the data stored under the given role for the item referred to by the index.
     * \param index The index of the Section item in the list.
     * \param role The requested role of the Section item. \sa SectionRoles
     * \return Returns the requested data or an invalid QVariant for an invalid index.
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    /*!
     * \brief getSection A handy getter to retrieve the Section object's data at a specific row easily,
     *        with the benefit to be able to use the roleNames on this object.
     * \param row The row at which the corresponding Section object is located.
     * \return Returns the data of the corresponding Section object.
     */
    Q_INVOKABLE QVariantMap getSection(int row);

    /*!
     * \brief isEmpty Checks if the Section model is empty.
     * \return Returns true if the the Section list is empty.
     */
    bool isEmpty();

    /*!
     * \brief setSections Replaces all sections, e.g. with the interests 9GAG currently lists.
     */
    void setSections(const QList<Section> &sections);

    /*!
     * \brief indexOf Returns the row of the section with \p kind and \p key, or -1.
     */
    Q_INVOKABLE int indexOf(const QString &kind, const QString &key) const;

    /*!
     * \brief setDefaultSections Resets the data model to the default sections.
     */
    void setDefaultSections();

    /*!
     * \brief save Stores the current data model in the config file.
     * \param settings Pointer to the QSettings object.
     * \param key The name for the settings value.
     */
    void save(QSettings *settings, const QString &key) const;

    /*!
     * \brief restore Restores and loads the data model from the config file.
     * \param settings Pointer to the QSettings object.
     * \param key The settings value name from which the data model should be restored.
     */
    void restore(QSettings *settings, const QString &key);

protected:
    /*!
     * \brief roleNames Overrides the default function to expose the custom roles to QML.
     * \return Returns the model's custom role names.
     */
    QHash<int, QByteArray> roleNames() const;

private:
    QList<Section> m_sections;
};

#endif // SECTIONMODEL_H
