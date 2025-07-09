#pragma once

#ifndef CAMERAITEM_H
#define CAMERAITEM_H

#include <QtGui>

class MemberItem
{
public:
    explicit MemberItem();
    MemberItem(
        const QString name, const QString type, const bool getter, const bool setter,
        const int protectionLevel
    );
    ~MemberItem() {}
    MemberItem &operator=(const MemberItem &other);

    QString name() const;
    void setName(const QString &newName);

    QString type() const;
    void setType(const QString &newType);

    bool getter() const;
    void setGetter(bool newGetter);

    bool setter() const;
    void setSetter(bool newSetter);

    int protectionLevel() const;
    void setProtectionLevel(int newProtectionLevel);

private:
    QString m_name;
    QString m_type;
    bool m_getter;
    bool m_setter;
    int m_protectionLevel;
};

class MemberList : public QObject
{
    Q_OBJECT

public:
    explicit MemberList(QObject *parent = nullptr);

    QVector<MemberItem> items() const;

    bool setItemAt(int index, const MemberItem &item);

signals:
    void preItemAppended();
    void postItemAppended();

    void preItemRemoved(int index);
    void postItemRemoved();

public slots:
    void appendItem(
        const QString &name, const QString &type, const bool getter, const bool setter,
        const int protectionLevel
    );
    void removeItemAt(int index);
    int count();

private:
    QVector<MemberItem> m_items;
};

#endif