/*
    This file is part of kdepim.
    Copyright (c) 2009 Kevin Krammer <kevin.krammer@gmx.at>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KRES_AKONADI_STORECOLLECTIONDIALOG_H
#define KRES_AKONADI_STORECOLLECTIONDIALOG_H

#include <akonadi/collection.h>

#include <KDialog>

namespace Akonadi {
  class CollectionView;
  class StoreCollectionFilterProxyModel;
}

class AbstractSubResourceModel;
class QLabel;
class QModelIndex;

class StoreCollectionDialog : public KDialog
{
  Q_OBJECT

  public:
    explicit StoreCollectionDialog( QWidget* parent = 0 );

    ~StoreCollectionDialog();

    void setLabelText( const QString &labelText );

    void setMimeType( const QString &mimeType );

    void setSelectedCollection( const Akonadi::Collection &collection );

    Akonadi::Collection selectedCollection() const;

    void setSubResourceModel( const AbstractSubResourceModel *subResourceModel );

  protected:
    QLabel *mLabel;
    Akonadi::StoreCollectionFilterProxyModel *mFilterModel;
    Akonadi::CollectionView *mView;

    Akonadi::Collection mSelectedCollection;

  protected Q_SLOTS:
    void collectionChanged( const Akonadi::Collection &collection );
    void collectionsInserted( const QModelIndex &parent, int start, int end );
};

#endif

// kate: space-indent on; indent-width 2; replace-tabs on;
