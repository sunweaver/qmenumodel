/*
 * Copyright 2012 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Renato Araujo Oliveira Filho <renato@canonical.com>
 */

#ifndef QMENUMODEL_H
#define QMENUMODEL_H

#include <QAbstractListModel>

typedef struct _GMenuModel GMenuModel;

class QMenuModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum MenuRoles {
        Action = 0,
        Label,
        LinkSection,
        LinkSubMenu,
        Extra
    };

    ~QMenuModel();

    /* QAbstractItemModel */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QModelIndex parent (const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

protected:
    QMenuModel(GMenuModel *other=0, QObject *parent=0);
    void setMenuModel(GMenuModel *model);
    GMenuModel *menuModel() const;

private:
    GMenuModel *m_menuModel;
    quint32 m_signalChangedId;

    QVariant getStringAttribute(const QModelIndex &index, const QString &attribute) const;
    QVariant getLink(const QModelIndex &index, const QString &linkName) const;
    QVariant getExtraProperties(const QModelIndex &index) const;
    QString parseExtraPropertyName(const QString &name) const;

    static void onItemsChanged(GMenuModel *model, int position, int removed, int added, void *data);
};

#endif
