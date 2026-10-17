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

#include "sectionmodel.h"

Section::Section(const QString &name, const QString &urlPath, const int &groupId, const QString &listType,
                 const QString &listType2, bool isSensitive)
    : m_name(name), m_urlPath(urlPath), m_groupId(groupId), m_listType(listType), m_listType2(listType2),
      m_isSensitive(isSensitive), m_kind(QStringLiteral("group"))
{
}

Section::Section(const QString &name, const QString &urlPath, const int &groupId, bool isSensitive)
    : m_name(name), m_urlPath(urlPath), m_groupId(groupId), m_listType("hot"), m_listType2("hot"),
      m_isSensitive(isSensitive), m_kind(QStringLiteral("group"))
{
}

Section::Section(const QString &name, const QString &urlPath, bool isSensitive)
    : m_name(name), m_urlPath(urlPath), m_groupId(-1), m_listType(QString()), m_listType2(QString()),
      m_isSensitive(isSensitive), m_kind(QStringLiteral("group"))
{
}

Section Section::feed(const QString &name, const QString &key, const QString &kind,
                      const QString &category, bool isSensitive)
{
    Section section(name, key, kind == QLatin1String("home") ? 1 : -1, isSensitive);
    section.m_kind = kind;
    section.m_category = category;
    return section;
}

QString Section::name() const
{
    return m_name;
}

QString Section::urlPath() const
{
    return m_urlPath;
}

int Section::groupId() const
{
    return m_groupId;
}

QString Section::listType() const
{
    return m_listType;
}

QString Section::listType2() const
{
    return m_listType2;
}

bool Section::isSensitive() const
{
    return m_isSensitive;
}

QString Section::kind() const
{
    return m_kind;
}

QString Section::category() const
{
    return m_category;
}


SectionModel::SectionModel(QObject *parent) : QAbstractListModel(parent)
{

}

void SectionModel::addSection(const Section &section)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_sections.append(section);
    endInsertRows();
}

int SectionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_sections.count();
}

QVariant SectionModel::data(const QModelIndex &index, int role) const
{
    if ((index.row() < 0) || (index.row() >= m_sections.count())) {
        return QVariant();
    }

    const Section &section = m_sections[index.row()];
    QVariant reqData;

    switch (role) {
        case NameRole:
            reqData = section.name(); break;
        case UrlPathRole:
            reqData = section.urlPath(); break;
        case GroupIdRole:
            reqData = section.groupId(); break;
        case ListTypeRole:
            reqData = section.listType(); break;
        case ListType2Role:
            reqData = section.listType2(); break;
        case IsSensitiveRole:
            reqData = section.isSensitive(); break;
        case KindRole:
            reqData = section.kind(); break;
        case CategoryRole:
            reqData = section.category(); break;
        default:
            reqData = QVariant(); break;
    }

    return reqData;
}

QHash<int, QByteArray> SectionModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[UrlPathRole] = "urlPath";
    roles[GroupIdRole] = "groupId";
    roles[ListTypeRole] = "listType";
    roles[ListType2Role] = "listType2";
    roles[IsSensitiveRole] = "isSensitive";
    roles[KindRole] = "kind";
    roles[CategoryRole] = "category";

    return roles;
}

QVariantMap SectionModel::getSection(int row)
{
    QHash<int, QByteArray> names = roleNames();
    QHashIterator<int, QByteArray> i(names);
    QVariantMap res;
    QModelIndex modelIndex = index(row, 0);

    while (i.hasNext()) {
        i.next();
        QVariant data = modelIndex.data(i.key());
        res[i.value()] = data;
    }

    return res;
}

bool SectionModel::isEmpty()
{
    if (m_sections.size() == 0)
        return true;
    else
        return false;
}

void SectionModel::setSections(const QList<Section> &sections)
{
    beginResetModel();
    m_sections = sections;
    endResetModel();
}

int SectionModel::indexOf(const QString &kind, const QString &key) const
{
    for (int i = 0; i < m_sections.size(); ++i) {
        if (m_sections.at(i).kind() == kind && m_sections.at(i).urlPath() == key)
            return i;
    }
    return -1;
}

void SectionModel::setDefaultSections()
{
    // The home feeds, then the interests 9GAG listed in /v2/group-list in October 2026.
    // GagBookManager::updateSections() replaces the interests with the live list.
    QList<Section> sections;
    sections << Section::feed("Hot", "hot", "home", "Feeds", false)
             << Section::feed("Trending", "trending", "home", "Feeds", false)
             << Section::feed("Fresh", "fresh", "home", "Feeds", false);

    struct { const char *name; const char *key; bool sensitive; } interests[] = {
        { "Animals & Pets", "animals", false }, { "Anime & Manga", "anime", false },
        { "Anime NSFW", "animensfw", true }, { "Cat", "cats", false }, { "Comic", "comic", false },
        { "Cosplay", "cosplay", false }, { "Crypto", "crypto", false }, { "Food & Drinks", "food", false },
        { "Gaming", "games", false }, { "Girl", "girlsfw", true }, { "Girl NSFW", "girlnsfw", true },
        { "Guy NSFW", "guys", true }, { "Humor", "humor", false }, { "Latest News", "news", false },
        { "Lifestyle", "lifestyle", false }, { "Memes", "memes", false },
        { "Motor Vehicles", "motorvehicles", false }, { "Movies & TV", "movies", false },
        { "Music", "music", false }, { "Old Meme", "oldmeme", false }, { "Politics", "politics", false },
        { "Random", "random", false }, { "Relationship & Dating", "relationship", false },
        { "Science & Tech", "science", false }, { "Sports", "sports", false },
        { "Superhero", "superhero", false }, { "Wholesome", "wholesome", false },
        { "Woah", "woah", false }, { "WTF", "wtf", false }
    };
    for (size_t i = 0; i < sizeof(interests) / sizeof(interests[0]); ++i)
        sections << Section::feed(QString::fromUtf8(interests[i].name), interests[i].key, "interest",
                                  "Interests", interests[i].sensitive);

    struct { const char *name; const char *key; } locals[] = {
        { "Brazil", "brazil" }, { "Canada", "canada" }, { "France", "france" }, { "Germany", "germany" },
        { "Hong Kong", "hongkong" }, { "India", "india" }, { "Indonesia", "indonesia" },
        { "Mexico", "mexico" }, { "Netherlands", "netherlands" }, { "Philippines", "philippines" },
        { "United Kingdom", "unitedkingdom" }, { "USA", "usa" }
    };
    for (size_t i = 0; i < sizeof(locals) / sizeof(locals[0]); ++i)
        sections << Section::feed(QString::fromUtf8(locals[i].name), locals[i].key, "interest", "Local", false);

    setSections(sections);
}

void SectionModel::save(QSettings *settings, const QString &key) const
{
    settings->beginWriteArray(key, m_sections.size());

    for (int i = 0; i < m_sections.size(); ++i) {
        settings->setArrayIndex(i);
        settings->setValue("name", m_sections.at(i).name());
        settings->setValue("urlPath", m_sections.at(i).urlPath());
        settings->setValue("groupId", m_sections.at(i).groupId());
        //settings->setValue("listType", m_sections.at(i).listType());
        //settings->setValue("listType2", m_sections.at(i).listType2());
        settings->setValue("isSensitive", m_sections.at(i).isSensitive());
        settings->setValue("kind", m_sections.at(i).kind());
        settings->setValue("category", m_sections.at(i).category());
    }

    settings->endArray();
}

void SectionModel::restore(QSettings *settings, const QString &key)
{
    beginResetModel();
    m_sections.clear();
    int size = settings->beginReadArray(key);
    QVariant name, urlPath, groupId, isSensitive;

    for (int i = 0; i < size; ++i) {
        settings->setArrayIndex(i);
        name = settings->value("name");
        urlPath = settings->value("urlPath");
        groupId = settings->value("groupId");
        isSensitive = settings->value("isSensitive");

        const QString kind = settings->value("kind").toString();

        if (kind.isEmpty()) {
            // a section list from before 1.6.0
            m_sections.append(Section(name.toString(), urlPath.toString(), groupId.toInt(), isSensitive.toBool()));
        } else {
            m_sections.append(Section::feed(name.toString(), urlPath.toString(), kind,
                                            settings->value("category").toString(), isSensitive.toBool()));
        }
    }

    settings->endArray();
    endResetModel();
}
