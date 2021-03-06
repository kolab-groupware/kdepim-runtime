/*
    Copyright (c) 2010 Klarälvdalens Datakonsult AB,
                       a KDAB Group company <info@kdab.com>
    Author: Kevin Ottens <kevin@kdab.com>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/

#include "addcollectiontask.h"

#include "collectionannotationsattribute.h"

#include <KDE/KDebug>
#include <KDE/KLocale>

#include <kimap/createjob.h>
#include <kimap/session.h>
#include <kimap/setmetadatajob.h>
#include <kimap/subscribejob.h>

#include <akonadi/collectiondeletejob.h>

AddCollectionTask::AddCollectionTask( ResourceStateInterface::Ptr resource, QObject *parent )
  : ResourceTask( DeferIfNoSession, resource, parent ), m_pendingJobs( 0 ), m_session(0)
{
}

AddCollectionTask::~AddCollectionTask()
{
}

void AddCollectionTask::doStart( KIMAP::Session *session )
{
  if ( parentCollection().remoteId().isEmpty() ) {
    kWarning() << "Parent collection has no remote id, aborting." << collection().name() << parentCollection().name();
    emitError( i18n( "Cannot add IMAP folder '%1' for a non-existing parent folder '%2'.",
                    collection().name(),
                    parentCollection().name() ) );
    changeProcessed();
    return;
  }

  const QChar separator = separatorCharacter();
  m_pendingJobs = 0;
  m_session = session;
  m_collection = collection();
  m_collection.setName( m_collection.name().replace( separator, QString() ) );
  m_collection.setRemoteId( separator + m_collection.name() );

  QString newMailBox = mailBoxForCollection( parentCollection() );

  if ( !newMailBox.isEmpty() )
    newMailBox += separator;

  newMailBox += m_collection.name();

  kDebug( 5327 ) << "New folder: " << newMailBox;

  KIMAP::CreateJob *job = new KIMAP::CreateJob( session );
  job->setMailBox( newMailBox );

  connect( job, SIGNAL(result(KJob*)),
           this, SLOT(onCreateDone(KJob*)) );

  job->start();
}


void AddCollectionTask::onCreateDone( KJob *job )
{
  if ( job->error() ) {
    kWarning() << "Failed to create folder on server: " << job->errorString();
    emitError( i18n( "Failed to create the folder '%1' on the IMAP server. ",
                      m_collection.name() ) );
    cancelTask( job->errorString() );
  } else {
    // Automatically subscribe to newly created mailbox
    KIMAP::CreateJob *create = static_cast<KIMAP::CreateJob*>( job );

    KIMAP::SubscribeJob *subscribe = new KIMAP::SubscribeJob( create->session() );
    subscribe->setMailBox( create->mailBox() );

    connect( subscribe, SIGNAL(result(KJob*)),
             this, SLOT(onSubscribeDone(KJob*)) );

    subscribe->start();
  }
}

void AddCollectionTask::onSubscribeDone( KJob *job )
{
  if ( job->error() && isSubscriptionEnabled() ) {
    kWarning() << "Failed to subscribe to the new folder: " << job->errorString();
    emitWarning( i18n( "Failed to subscribe to the folder '%1' on the IMAP server. "
                       "It will disappear on next sync. Use the subscription dialog to overcome that",
                       m_collection.name() ) );
  }

  const Akonadi::CollectionAnnotationsAttribute *attribute = m_collection.attribute<Akonadi::CollectionAnnotationsAttribute>();
  if ( !attribute || !serverSupportsAnnotations() ) {
    // we are finished
    changeCommitted( m_collection );
    synchronizeCollectionTree();
    return;
  }

  const QMap<QByteArray, QByteArray> annotations = attribute->annotations();

  foreach ( const QByteArray &entry, annotations.keys() ) { //krazy:exclude=foreach
    KIMAP::SetMetaDataJob *job = new KIMAP::SetMetaDataJob( m_session );
    if ( serverCapabilities().contains( QLatin1String("METADATA") ) ) {
      job->setServerCapability( KIMAP::MetaDataJobBase::Metadata );
    } else {
      job->setServerCapability( KIMAP::MetaDataJobBase::Annotatemore );
    }
    job->setMailBox( mailBoxForCollection( m_collection ) );

    if ( !entry.startsWith( "/shared" ) && !entry.startsWith( "/private" ) ) {
        //Support for legacy annotations that don't include the prefix
      job->addMetaData( QByteArray("/shared") + entry, annotations[entry] );
    } else {
      job->addMetaData( entry, annotations[entry] );
    }

    connect( job, SIGNAL(result(KJob*)),
             this, SLOT(onSetMetaDataDone(KJob*)) );

    m_pendingJobs++;

    job->start();
  }
}

void AddCollectionTask::onSetMetaDataDone( KJob *job )
{
  if ( job->error() ) {
    kWarning() << "Failed to write annotations: " << job->errorString();
    emitWarning( i18n( "Failed to write some annotations for '%1' on the IMAP server. %2",
                       collection().name(), job->errorText() ) );
  }

  m_pendingJobs--;

  if ( m_pendingJobs == 0 )
    changeCommitted( m_collection );
}

