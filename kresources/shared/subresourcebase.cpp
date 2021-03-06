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

#include "subresourcebase.h"

#include <akonadi/entitydisplayattribute.h>

#include <KConfigGroup>
#include <KDebug>
#include <KUrl>

using namespace Akonadi;

SubResourceBase::SubResourceBase( const Akonadi::Collection &collection )
  : mCollection( collection ),
    mActive( true ),
    mIdArbiter( 0 )
{
}

SubResourceBase::~SubResourceBase()
{
}

void SubResourceBase::setIdArbiter( IdArbiterBase *idArbiter )
{
  mIdArbiter = idArbiter;
}

QString SubResourceBase::label() const
{
  return label( mCollection );
}

void SubResourceBase::setActive( bool active )
{
  if ( mActive != active ) {
    mActive = active;

    if ( mActive ) {
      foreach ( const Item &item, mItems ) {
        itemAdded( item );
      }
    } else {
      foreach ( const Item &item, mItems ) {
        itemRemoved( item );
      }
    }
  }
}

bool SubResourceBase::isActive() const
{
  return mActive;
}

bool SubResourceBase::isWritable() const
{
  return isWritable( mCollection );
}

QString SubResourceBase::subResourceIdentifier() const
{
  return mCollection.url().url();
}

void SubResourceBase::readConfig( const KConfigGroup &config )
{
  if ( !config.isValid() )
    return;

  const QString collectionUrl = mCollection.url().url();
  if ( !config.hasGroup( collectionUrl ) )
    return;

  KConfigGroup group( &config, collectionUrl );
  mActive = group.readEntry<bool>( QLatin1String( "Active" ), true );

  readTypeSpecificConfig( config );
}

void SubResourceBase::writeConfig( KConfigGroup &config ) const
{
  KConfigGroup group( &config, mCollection.url().url() );

  group.writeEntry( QLatin1String( "Active" ), mActive );
}

void SubResourceBase::changeCollection( const Akonadi::Collection &collection )
{
  Q_ASSERT( collection.id() == mCollection.id() );

  // first pass to subclass, then update member so subclasses can
  // check what changed
  collectionChanged( collection );

  mCollection = collection;
}

void SubResourceBase::addItem( const Akonadi::Item &item )
{
  const ItemsByItemId::const_iterator findIt = mItems.constFind( item.id() );
  if ( findIt != mItems.constEnd() ) {
    kWarning( 5650 ) << "Item id=" << item.id()
                     << ", remoteId=" << item.remoteId()
                     << ", mimeType=" << item.mimeType()
                     << "is already part of this subresource"
                     << "(id=" << mCollection.id()
                     << ", remoteId=" << mCollection.remoteId()
                     << ")";
    if ( mActive ) {
      itemChanged( item );
    }
  } else {
    if ( mActive ) {
      itemAdded( item );
    }
    mItems.insert( item.id(), item );
  }
}

void SubResourceBase::changeItem( const Akonadi::Item &item )
{
  ItemsByItemId::iterator findIt = mItems.find( item.id() );
  if ( findIt == mItems.end() ) {
    kWarning( 5650 ) << "Item id=" << item.id()
                     << ", remoteId=" << item.remoteId()
                     << ", mimeType=" << item.mimeType()
                     << "is not yet part of this subresource"
                     << "(id=" << mCollection.id()
                     << ", remoteId=" << mCollection.remoteId()
                     << ")";
    if ( mActive ) {
      itemAdded( item );
    }
    mItems.insert( item.id(), item );
  } else {
    if ( mActive ) {
      itemChanged( item );
    }
    findIt.value() = item;
  }
}

void SubResourceBase::removeItem( const Akonadi::Item &item )
{
  ItemsByItemId::iterator findIt = mItems.find( item.id() );
  if ( findIt == mItems.end() ) {
    kWarning( 5650 ) << "Item id=" << item.id()
                     << ", remoteId=" << item.remoteId()
                     << ", mimeType=" << item.mimeType()
                     << "is not part of this subresource"
                     << "(id=" << mCollection.id()
                     << ", remoteId=" << mCollection.remoteId()
                     << ")";
  } else {
    if ( mActive ) {
      itemRemoved( item );
    }
    mItems.erase( findIt );
  }
}

bool SubResourceBase::hasMappedItem( const QString &kresId ) const
{
  return mMappedItems.constFind( kresId ) != mMappedItems.constEnd();
}

Akonadi::Item SubResourceBase::mappedItem( const QString &kresId ) const
{
  return mMappedItems.value( kresId );
}

Akonadi::Collection SubResourceBase::collection() const
{
  return mCollection;
}

void SubResourceBase::readTypeSpecificConfig( const KConfigGroup &config )
{
  Q_UNUSED( config );
}

void SubResourceBase::writeTypeSpecificConfig( KConfigGroup &config ) const
{
  Q_UNUSED( config );
}

void SubResourceBase::collectionChanged( const Akonadi::Collection &collection )
{
  Q_UNUSED( collection );
}

QString SubResourceBase::label( const Akonadi::Collection &collection )
{
  return collection.displayName();
}

bool SubResourceBase::isWritable( const Akonadi::Collection &collection )
{
  Akonadi::Collection::Rights itemRights;
  itemRights |= Akonadi::Collection::CanCreateItem;
  itemRights |= Akonadi::Collection::CanChangeItem;
  itemRights |= Akonadi::Collection::CanDeleteItem;

  return ( collection.rights() & itemRights ) != 0;
}

// kate: space-indent on; indent-width 2; replace-tabs on;
