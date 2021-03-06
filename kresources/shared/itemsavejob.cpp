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

#include "itemsavejob.h"

#include "itemsavecontext.h"

#include <akonadi/itemcreatejob.h>
#include <akonadi/itemdeletejob.h>
#include <akonadi/itemmodifyjob.h>

#include <KDebug>

using namespace Akonadi;

ItemSaveJob::ItemSaveJob( const ItemSaveContext &saveContext )
{
  foreach ( const ItemAddContext &addContext, saveContext.addedItems ) {
    kDebug( 5650 ) << "CreateJob for Item (mimeType=" << addContext.item.mimeType()
                   << "), collection (id=" << addContext.collection.id()
                   << ", remoteId=" << addContext.collection.remoteId()
                   << ")";
    (void)new ItemCreateJob( addContext.item, addContext.collection, this );
  }

  foreach ( const Item &item, saveContext.changedItems ) {
    kDebug( 5650 ) << "ModifyJob for Item (id=" << item.id()
                   << ", remoteId=" << item.remoteId()
                   << ", mimeType=" << item.mimeType()
                   << ")";
    (void)new ItemModifyJob( item, this );
  }

  foreach ( const Item &item, saveContext.removedItems ) {
    kDebug( 5650 ) << "DeleteJob for Item (id=" << item.id()
                   << ", remoteId=" << item.remoteId()
                   << ", mimeType=" << item.mimeType()
                   << ")";
    (void)new ItemDeleteJob( item, this );
  }
}

// kate: space-indent on; indent-width 2; replace-tabs on;
