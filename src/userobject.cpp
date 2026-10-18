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

#include "userobject.h"

class UserObjectData : public QSharedData
{
public:
    UserObjectData() : name(QString()), userId(QString()), emojiStatus(QString()), avatarUrl(QUrl()),
        isProUser(false), isStaffUser(false)
    {

    }

    ~UserObjectData()
    {

    }

    QString name;
    QString userId;
    QString accountId;
    QString emojiStatus;
    QUrl avatarUrl;
    bool isProUser;
    bool isStaffUser;

private:
    Q_DISABLE_COPY(UserObjectData)
};

UserObject::UserObject() : data(new UserObjectData)
{

}

UserObject::UserObject(const QString &userId)
{
    UserObjectData *obj = new UserObjectData();
    obj->userId = userId;
    data = obj;
}

UserObject::UserObject(const UserObject &obj) : data(obj.data)
{

}

UserObject &UserObject::operator=(const UserObject &obj)
{
    if (this != &obj)
        data.operator=(obj.data);

    return *this;
}

UserObject::~UserObject()
{

}

QString UserObject::name() const
{
    return data->name;
}

void UserObject::setName(const QString &name)
{
    data->name = name;
}

QString UserObject::userId() const
{
    return data->userId;
}

void UserObject::setUserId(const QString &id)
{
    data->userId = id;
}

QString UserObject::accountId() const
{
    return data->accountId;
}

void UserObject::setAccountId(const QString &id)
{
    data->accountId = id;
}

QString UserObject::emojiStatus() const
{
    return data->emojiStatus;
}

void UserObject::setEmojiStatus(const QString &status)
{
    data->emojiStatus = status;
}

QUrl UserObject::avatarUrl() const
{
    return data->avatarUrl;
}

void UserObject::setAvatarUrl(const QUrl &url)
{
    data->avatarUrl = url;
}

bool UserObject::isProUser() const
{
    return data->isProUser;
}

void UserObject::setIsProUser(bool value)
{
    data->isProUser = value;
}

bool UserObject::isStaffUser() const
{
    return data->isStaffUser;
}

void UserObject::setIsStaffUser(bool value)
{
    data->isStaffUser = value;
}
