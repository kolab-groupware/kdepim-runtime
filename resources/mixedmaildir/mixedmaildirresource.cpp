/*  This file is part of the KDE project
    Copyright (c) 2007 Till Adam <adam@kde.org>
    Copyright (C) 2010 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.net
    Author: Kevin Krammer, krake@kdab.com

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

#include "mixedmaildirresource.h"

#include "configdialog.h"
#include "mixedmaildirstore.h"
#include "settings.h"
#include "settingsadaptor.h"

#include "filestore/collectioncreatejob.h"
#include "filestore/collectiondeletejob.h"
#include "filestore/collectionfetchjob.h"
#include "filestore/collectionmodifyjob.h"
#include "filestore/collectionmovejob.h"
#include "filestore/entitycompactchangeattribute.h"
#include "filestore/itemcreatejob.h"
#include "filestore/itemdeletejob.h"
#include "filestore/itemfetchjob.h"
#include "filestore/itemmodifyjob.h"
#include "filestore/itemmovejob.h"
#include "filestore/storecompactjob.h"

#include <akonadi/kmime/messageparts.h>
#include <akonadi/changerecorder.h>
#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/itemmodifyjob.h>
#include <akonadi/collectionfetchscope.h>

#include <kmime/kmime_message.h>

#include <KDebug>
#include <KLocale>
#include <KWindowSystem>

#include <QtCore/QDir>
#include <QtDBus/QDBusConnection>

using namespace Akonadi;

MixedMaildirResource::MixedMaildirResource( const QString &id )
    : ResourceBase( id ), mStore( new MixedMaildirStore() )
{
  new SettingsAdaptor( Settings::self() );
  QDBusConnection::sessionBus().registerObject( QLatin1String( "/Settings" ),
                              Settings::self(), QDBusConnection::ExportAdaptors );
  connect( this, SIGNAL( reloadConfiguration() ), SLOT( ensureSaneConfiguration() ) );
  connect( this, SIGNAL( reloadConfiguration() ), SLOT( ensureDirExists() ) );

  // We need to enable this here, otherwise we neither get the remote ID of the
  // parent collection when a collection changes, nor the full item when an item
  // is added.
  changeRecorder()->fetchCollection( true );
  changeRecorder()->itemFetchScope().fetchFullPayload( true );
  changeRecorder()->itemFetchScope().setAncestorRetrieval( ItemFetchScope::All );
  changeRecorder()->collectionFetchScope().setAncestorRetrieval( CollectionFetchScope::All );

  setHierarchicalRemoteIdentifiersEnabled( true );

  if ( ensureSaneConfiguration() ) {
    mStore->setPath( Settings::self()->path() );
  }
}

MixedMaildirResource::~MixedMaildirResource()
{
  delete mStore;
}

void MixedMaildirResource::aboutToQuit()
{
  // The settings may not have been saved if e.g. they have been modified via
  // DBus instead of the config dialog.
  Settings::self()->writeConfig();
}

void MixedMaildirResource::configure( WId windowId )
{
  ConfigDialog dlg;
  if ( windowId ) {
    KWindowSystem::setMainWindow( &dlg, windowId );
  }
  if ( dlg.exec() ) {
    mStore->setPath( Settings::self()->path() );
    emit configurationDialogAccepted();
  } else {
    emit configurationDialogRejected();
  }

  ensureDirExists();
  synchronizeCollectionTree();
}

void MixedMaildirResource::itemAdded( const Akonadi::Item & item, const Akonadi::Collection& collection )
{
  if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  FileStore::ItemCreateJob *job = mStore->createItem( item, collection );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( itemAddedResult( KJob* ) ) );
}

void MixedMaildirResource::itemChanged( const Akonadi::Item& item, const QSet<QByteArray>& parts )
{
  if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  if ( Settings::self()->readOnly() ) {
    changeProcessed();
    return;
  }

  Q_UNUSED( parts );
//   const bool hasPayload = parts.contains( Item::FullPayload ) || parts.contains( MessagePart::Body );
//   Q_ASSERT( !hasPayload || item.hasPayload<KMime::Message::Ptr>() );

  FileStore::ItemModifyJob *job = mStore->modifyItem( item );
  job->setIgnorePayload( !item.hasPayload<KMime::Message::Ptr>() );
  job->setProperty( "originalRemoteId", item.remoteId() );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( itemChangedResult( KJob* ) ) );
}

void MixedMaildirResource::itemMoved( const Item &item, const Collection &source, const Collection &destination )
{
  if ( source == destination ) {
    changeProcessed();
    return;
  }

  if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  Item moveItem = item;
  moveItem.setParentCollection( source );

  FileStore::ItemMoveJob *job = mStore->moveItem( moveItem, destination );
  job->setProperty( "originalRemoteId", moveItem.remoteId() );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( itemMovedResult( KJob* ) ) );
}

void MixedMaildirResource::itemRemoved(const Akonadi::Item & item)
{
  if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  FileStore::ItemDeleteJob *job = mStore->deleteItem( item );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( itemRemovedResult( KJob* ) ) );
}

void MixedMaildirResource::retrieveCollections()
{
  if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  FileStore::CollectionFetchJob *job = mStore->fetchCollections( mStore->topLevelCollection(), FileStore::CollectionFetchJob::Recursive );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( retrieveCollectionsResult( KJob* ) ) );
}

void MixedMaildirResource::retrieveItems( const Akonadi::Collection & col )
{
  if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  FileStore::ItemFetchJob *job = mStore->fetchItems( col );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( retrieveItemsResult( KJob* ) ) );
}

bool MixedMaildirResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( parts );

  FileStore::ItemFetchJob *job = mStore->fetchItem( item );
  if ( parts.contains( Item::FullPayload ) ) {
    job->fetchScope().fetchFullPayload( true );
  } else {
    Q_FOREACH( const QByteArray &part, parts ) {
      job->fetchScope().fetchPayloadPart( part, true );
    }
  }
  connect( job, SIGNAL( result( KJob* ) ), SLOT( retrieveItemResult( KJob* ) ) );

  return true;
}

void MixedMaildirResource::collectionAdded(const Collection & collection, const Collection &parent)
{
  if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  FileStore::CollectionCreateJob *job = mStore->createCollection( collection, parent );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( collectionAddedResult( KJob* ) ) );
}

void MixedMaildirResource::collectionChanged(const Collection & collection)
{
  if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  FileStore::CollectionModifyJob *job = mStore->modifyCollection( collection );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( collectionChangedResult( KJob* ) ) );
}

void MixedMaildirResource::collectionChanged(const Collection & collection, const QSet<QByteArray> &changedAttributes )
{
  if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  Q_UNUSED( changedAttributes );

  FileStore::CollectionModifyJob *job = mStore->modifyCollection( collection );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( collectionChangedResult( KJob* ) ) );
}

void MixedMaildirResource::collectionMoved( const Collection &collection, const Collection &source, const Collection &dest )
{
  //kDebug( KDE_DEFAULT_DEBUG_AREA ) << collection << source << dest;

  if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  if ( collection.parentCollection() == Collection::root() ) {
    const QString message = i18nc( "@info:status", "Cannot move root maildir folder '%1'." ,collection.remoteId() );
    kError() << message;
    cancelTask( message );
    return;
  }

  if ( source == dest ) { // should not happen, but who knows...
    changeProcessed();
    return;
  }

  Collection moveCollection = collection;
  moveCollection.setParentCollection( source );

  FileStore::CollectionMoveJob *job = mStore->moveCollection( moveCollection, dest );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( collectionMovedResult( KJob* ) ) );
}

void MixedMaildirResource::collectionRemoved( const Akonadi::Collection &collection )
{
   if ( !ensureSaneConfiguration() ) {
    const QString message = i18nc( "@info:status", "Unusable configuration." );
    kError() << message;
    cancelTask( message );
    return;
  }

  if ( collection.parentCollection() == Collection::root() ) {
    emit error( i18n("Cannot delete top-level maildir folder '%1'.", Settings::self()->path() ) );
    changeProcessed();
    return;
  }

  FileStore::CollectionDeleteJob *job = mStore->deleteCollection( collection );
  connect( job, SIGNAL( result( KJob* ) ), SLOT( collectionRemovedResult( KJob* ) ) );
}

void MixedMaildirResource::ensureDirExists()
{
  QDir dir( Settings::self()->path() );
  if ( !dir.exists() ) {
    if ( !dir.mkpath( Settings::self()->path() ) ) {
      const QString message = i18nc( "@info:status", "Unable to create maildir '%1'.", Settings::self()->path() );
      kError() << message;
      status( Broken, message );
    }
  }
}

bool MixedMaildirResource::ensureSaneConfiguration()
{
  if ( Settings::self()->path().isEmpty() ) {
    const QString message = i18nc( "@info:status", "No usable storage location configured.");
    kError() << message;
    status( Broken, message );
    return false;
  }
  return true;
}

void MixedMaildirResource::retrieveCollectionsResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::CollectionFetchJob *fetchJob = qobject_cast<FileStore::CollectionFetchJob*>( job );
  Q_ASSERT( fetchJob != 0 );

  Collection::List collections;
  collections << mStore->topLevelCollection();
  collections << fetchJob->collections();
  collectionsRetrieved( collections );
}

void MixedMaildirResource::retrieveItemsResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::ItemFetchJob *fetchJob = qobject_cast<FileStore::ItemFetchJob*>( job );
  Q_ASSERT( fetchJob != 0 );

  itemsRetrieved( fetchJob->items() );
}

void MixedMaildirResource::retrieveItemResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::ItemFetchJob *fetchJob = qobject_cast<FileStore::ItemFetchJob*>( job );
  Q_ASSERT( fetchJob != 0 );
  Q_ASSERT( !fetchJob->items().isEmpty() );

  itemRetrieved( fetchJob->items()[ 0 ] );
}

void MixedMaildirResource::itemAddedResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::ItemCreateJob *itemJob = qobject_cast<FileStore::ItemCreateJob*>( job );
  Q_ASSERT( itemJob != 0 );

  changeCommitted( itemJob->item() );
}

void MixedMaildirResource::itemChangedResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::ItemModifyJob *itemJob = qobject_cast<FileStore::ItemModifyJob*>( job );
  Q_ASSERT( itemJob != 0 );

  changeCommitted( itemJob->item() );

  const QString remoteId = itemJob->property( "originalRemoteId" ).value<QString>();

  // only schedule compact if the modifed item is from an MBox, i.e. has a numerical
  // remoteId (Maildir items have strings with non-numeral characters)
  // the store can modify Maildir items but MBox requires append new + delete old
  bool ok = false;
  remoteId.toULongLong( &ok );
  if ( ok ) {
    scheduleCustomTask( this, "compactStore", QVariant() );
  }
}

void MixedMaildirResource::itemMovedResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::ItemMoveJob *itemJob = qobject_cast<FileStore::ItemMoveJob*>( job );
  Q_ASSERT( itemJob != 0 );

  changeCommitted( itemJob->item() );

  const QString remoteId = itemJob->property( "originalRemoteId" ).value<QString>();

  // only schedule compact if the delete item is from an MBox, i.e. has a numerical
  // remoteId (Maildir items have strings with non-numeral characters)
  bool ok = false;
  remoteId.toULongLong( &ok );
  if ( ok ) {
    scheduleCustomTask( this, "compactStore", QVariant() );
  }
}

void MixedMaildirResource::itemRemovedResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::ItemDeleteJob *itemJob = qobject_cast<FileStore::ItemDeleteJob*>( job );
  Q_ASSERT( itemJob != 0 );

  changeCommitted( itemJob->item() );

  const QString remoteId = itemJob->item().remoteId();

  // only schedule compact if the delete item is from an MBox, i.e. has a numerical
  // remoteId (Maildir items have strings with non-numeral characters)
  bool ok = false;
  remoteId.toULongLong( &ok );
  if ( ok ) {
    scheduleCustomTask( this, "compactStore", QVariant() );
  }
}

void MixedMaildirResource::collectionAddedResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::CollectionCreateJob *colJob = qobject_cast<FileStore::CollectionCreateJob*>( job );
  Q_ASSERT( colJob != 0 );

  changeCommitted( colJob->collection() );
}

void MixedMaildirResource::collectionChangedResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::CollectionModifyJob *colJob = qobject_cast<FileStore::CollectionModifyJob*>( job );
  Q_ASSERT( colJob != 0 );

  changeCommitted( colJob->collection() );
}

void MixedMaildirResource::collectionMovedResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::CollectionMoveJob *colJob = qobject_cast<FileStore::CollectionMoveJob*>( job );
  Q_ASSERT( colJob != 0 );

  changeCommitted( colJob->collection() );
}

void MixedMaildirResource::collectionRemovedResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::CollectionDeleteJob *colJob = qobject_cast<FileStore::CollectionDeleteJob*>( job );
  Q_ASSERT( colJob != 0 );

  changeCommitted( colJob->collection() );
}

void MixedMaildirResource::compactStore( const QVariant &arg )
{
  Q_UNUSED( arg );

  FileStore::StoreCompactJob *job = mStore->compactStore();
  connect( job, SIGNAL( result( KJob* ) ), SLOT( compactStoreResult( KJob* ) ) );
}

void MixedMaildirResource::compactStoreResult( KJob *job )
{
  if ( job->error() != 0 ) {
    kError() << job->errorString();
    status( Broken, job->errorString() );
    cancelTask( job->errorString() );
    return;
  }

  FileStore::StoreCompactJob *compactJob = qobject_cast<FileStore::StoreCompactJob*>( job );
  Q_ASSERT( compactJob != 0 );

  const Item::List items = compactJob->changedItems();
  kDebug( KDE_DEFAULT_DEBUG_AREA ) << "Compacting store resulted in" << items.count() << "changed items";

  // TODO this has to be done asynchronous
  Q_FOREACH( const Item &item, items ) {
    ItemFetchJob *fetchJob = new ItemFetchJob( item );
    if ( !fetchJob->exec() ) {
      kError() << "Fetch for item" << item.remoteId() << "parentCollection" << item.parentCollection() << "failed:" << fetchJob->errorString();
      continue;
    }

    if ( fetchJob->items().isEmpty() ) {
      kError() << "Fetch for item" << item.remoteId() << "parentCollection" << item.parentCollection() << "failed:" << fetchJob->errorString();
      continue;
    }

    Item changedItem = fetchJob->items()[ 0 ];
    changedItem.setRemoteId( item.attribute<FileStore::EntityCompactChangeAttribute>()->remoteId() );
    changedItem.removeAttribute<FileStore::EntityCompactChangeAttribute>();

    ItemModifyJob *modifyJob = new ItemModifyJob( changedItem );
    if ( !modifyJob->exec() ) {
      kError() << "Modify for item" << changedItem.remoteId() << "parentCollection" << item.parentCollection() << "failed:" << fetchJob->errorString();
      continue;
    }
  }

  taskDone();
}

#include "mixedmaildirresource.moc"

AKONADI_RESOURCE_MAIN( MixedMaildirResource )

// kate: space-indent on; indent-width 2; replace-tabs on;