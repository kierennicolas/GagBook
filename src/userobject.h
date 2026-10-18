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

#ifndef USEROBJECT_H
#define USEROBJECT_H

#include <QUrl>
#include <QObject>
#include <QExplicitlySharedDataPointer>

class UserObjectData;

class UserObject
{
    Q_GADGET

public:
    explicit UserObject();
    explicit UserObject(const QString &userId);
    UserObject(const UserObject &obj);
    UserObject &operator=(const UserObject &obj);
    ~UserObject();

    Q_INVOKABLE QString name() const;
    void setName(const QString &name);

    Q_INVOKABLE QString userId() const;
    void setUserId(const QString &id);

    /*! The account id; the comment service sends it next to the user id. */
    Q_INVOKABLE QString accountId() const;
    void setAccountId(const QString &id);

    Q_INVOKABLE QString emojiStatus() const;
    void setEmojiStatus(const QString &status);

    Q_INVOKABLE QUrl avatarUrl() const;
    void setAvatarUrl(const QUrl &url);

    Q_INVOKABLE bool isProUser() const;
    void setIsProUser(bool value);

    Q_INVOKABLE bool isStaffUser() const;
    void setIsStaffUser(bool value);

private:
    QExplicitlySharedDataPointer<UserObjectData> data;
};

Q_DECLARE_METATYPE(UserObject)

#endif // USEROBJECT_H
