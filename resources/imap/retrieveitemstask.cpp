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

#include "retrieveitemstask.h"

#include "collectionflagsattribute.h"
#include "noselectattribute.h"
#include "uidvalidityattribute.h"
#include "uidnextattribute.h"
#include "highestmodseqattribute.h"

#include <akonadi/cachepolicy.h>
#include <akonadi/collectionstatistics.h>
#include <akonadi/kmime/messageflags.h>
#include <akonadi/kmime/messageparts.h>
#include <akonadi/agentbase.h>
#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/session.h>

#include <KDE/KDebug>
#include <KDE/KLocale>

#include <kimap/expungejob.h>
#include <kimap/fetchjob.h>
#include <kimap/selectjob.h>
#include <kimap/session.h>

#define HIGHESTMODSEQ_PROPERTY "highestModSeq"

    if (m_currentSet.isEmpty()) {
        kDebug() << "fetch complete";
        emitResult();
        return;
    }
RetrieveItemsTask::RetrieveItemsTask( ResourceStateInterface::Ptr resource, QObject *parent )
  : ResourceTask( CancelIfNoSession, resource, parent ), m_session( 0 ), m_fetchedMissingBodies( -1 ), m_fetchMissingBodies( true )
{

}

RetrieveItemsTask::~RetrieveItemsTask()
{
}

void RetrieveItemsTask::setFetchMissingItemBodies(bool enabled)
{
  m_fetchMissingBodies = enabled;
}

void RetrieveItemsTask::doStart( KIMAP::Session *session )
{
  emitPercent(0);
  // Prevent fetching items from noselect folders.
  if ( collection().hasAttribute( "noselect" ) ) {
    NoSelectAttribute* noselect = static_cast<NoSelectAttribute*>( collection().attribute( "noselect" ) );
    if ( noselect->noSelect() ) {
      kDebug( 5327 ) << "No Select folder";
      itemsRetrievalDone();
      return;
    }
  }

  m_session = session;

  const Akonadi::Collection col = collection();
  if ( m_fetchMissingBodies && col.cachePolicy()
       .localParts().contains( QLatin1String(Akonadi::MessagePart::Body) ) ) { //disconnected mode, make sure we really have the body cached

    Akonadi::Session *session = new Akonadi::Session( resourceName().toLatin1() + "_body_checker", this );
    Akonadi::ItemFetchJob *fetchJob = new Akonadi::ItemFetchJob( col, session );
    fetchJob->fetchScope().setCheckForCachedPayloadPartsOnly();
    fetchJob->fetchScope().fetchPayloadPart( Akonadi::MessagePart::Body );
    fetchJob->fetchScope().setFetchModificationTime( false );
    connect( fetchJob, SIGNAL(result(KJob*)), this, SLOT(fetchItemsWithoutBodiesDone(KJob*)) );
    connect( fetchJob, SIGNAL(result(KJob*)), session, SLOT(deleteLater()) );
  } else {
    startRetrievalTasks();
  }
}

void RetrieveItemsTask::fetchItemsWithoutBodiesDone( KJob *job )
{
  Akonadi::ItemFetchJob *fetch = static_cast<Akonadi::ItemFetchJob*>( job );
  QList<qint64> uids;
  if ( job->error() ) {
    cancelTask( job->errorString() );
    return;
  } else {
    int i = 0;
    Q_FOREACH( const Akonadi::Item &item, fetch->items() )  {
      if ( !item.cachedPayloadParts().contains( Akonadi::MessagePart::Body ) ) {
          kWarning() << "Item " << item.id() << " is missing the payload! Cached payloads: " << item.cachedPayloadParts();
          uids.append( item.remoteId().toInt() );
          i++;
      }
    }
    if ( i > 0 ) {
      kWarning() << "Number of items missing the body: " << i;
    }
  }

  onFetchItemsWithoutBodiesDone(uids);
}

void RetrieveItemsTask::onFetchItemsWithoutBodiesDone( const QList<qint64> &items )
{
  m_messageUidsMissingBody = items;
  startRetrievalTasks();
}


void RetrieveItemsTask::startRetrievalTasks()
{
  const QString mailBox = mailBoxForCollection( collection() );

  // Now is the right time to expunge the messages marked \\Deleted from this mailbox.
  if ( isAutomaticExpungeEnabled() ) {
    if ( m_session->selectedMailBox() != mailBox ) {
      triggerPreExpungeSelect( mailBox );
    } else {
      triggerExpunge( mailBox );
    }
  } else {
    // Always select to get the stats updated
    triggerFinalSelect( mailBox );
  }
}

void RetrieveItemsTask::triggerPreExpungeSelect( const QString &mailBox )
{
  KIMAP::SelectJob *select = new KIMAP::SelectJob( m_session );
  select->setMailBox( mailBox );
  select->setCondstoreEnabled( serverCapabilities().contains( QLatin1String( "CONDSTORE" ) ) );
  connect( select, SIGNAL(result(KJob*)),
           this, SLOT(onPreExpungeSelectDone(KJob*)) );
  select->start();
}

void RetrieveItemsTask::onPreExpungeSelectDone( KJob *job )
{
  if ( job->error() ) {
    cancelTask( job->errorString() );
  } else {
    KIMAP::SelectJob *select = static_cast<KIMAP::SelectJob*>( job );
    triggerExpunge( select->mailBox() );
  }
}

void RetrieveItemsTask::triggerExpunge( const QString &mailBox )
{
  kDebug( 5327 ) << mailBox;

  KIMAP::ExpungeJob *expunge = new KIMAP::ExpungeJob( m_session );
  connect( expunge, SIGNAL(result(KJob*)),
           this, SLOT(onExpungeDone(KJob*)) );
  expunge->start();
}

void RetrieveItemsTask::onExpungeDone( KJob *job )
{
  // We can ignore the error IMO, we just had a wrong expunge so some old messages will just reappear
  // Not entirely, we at least have to handle network errors here to avoid getting stuck
  if ( job->error() && m_session->state() == KIMAP::Session::Disconnected ) {
    cancelTask( job->errorString() );
    return;
  }

  // We have to re-select the mailbox to update all the stats after the expunge
  // (the EXPUNGE command doesn't return enough for our needs)
  triggerFinalSelect( m_session->selectedMailBox() );
}

void RetrieveItemsTask::triggerFinalSelect( const QString &mailBox )
{
  KIMAP::SelectJob *select = new KIMAP::SelectJob( m_session );
  select->setMailBox( mailBox );
  select->setCondstoreEnabled( serverCapabilities().contains( QLatin1String( "CONDSTORE" ) ) );
  connect( select, SIGNAL(result(KJob*)),
           this, SLOT(onFinalSelectDone(KJob*)) );
  select->start();
}

void RetrieveItemsTask::onFinalSelectDone( KJob *job )
{
  if ( job->error() ) {
    cancelTask( job->errorString() );
    return;
  }

  KIMAP::SelectJob *select = qobject_cast<KIMAP::SelectJob*>( job );

  const QString mailBox = select->mailBox();
  const int messageCount = select->messageCount();
  const qint64 uidValidity = select->uidValidity();
  const qint64 nextUid = select->nextUid();
  quint64 highestModSeq = select->highestModSequence();
  const QList<QByteArray> flags = select->permanentFlags();

  //The select job retrieves highestmodset whenever it's available, but in case of no CONDSTORE support we ignore it
  if( !serverCapabilities().contains( QLatin1String( "CONDSTORE" )) ) {
    highestModSeq = 0;
  }

  // uidvalidity can change between sessions, we don't want to refetch
  // folders in that case. Keep track of what is processed and what not.
  static QStringList processed;
  bool firstTime = false;
  if ( processed.indexOf( mailBox ) == -1 ) {
    firstTime = true;
    processed.append( mailBox );
  }

  Akonadi::Collection col = collection();
  bool modifyNeeded = false;

  // Get the current uid validity value and store it
  int oldUidValidity = 0;
  if ( !col.hasAttribute( "uidvalidity" ) ) {
    UidValidityAttribute* currentUidValidity  = new UidValidityAttribute( uidValidity );
    col.addAttribute( currentUidValidity );
    modifyNeeded = true;
  } else {
    UidValidityAttribute* currentUidValidity =
      static_cast<UidValidityAttribute*>( col.attribute( "uidvalidity" ) );
    oldUidValidity = currentUidValidity->uidValidity();
    if ( oldUidValidity != uidValidity ) {
      currentUidValidity->setUidValidity( uidValidity );
      modifyNeeded = true;
    }
  }

  // Get the current uid next value and store it
  int oldNextUid = 0;
  if ( !col.hasAttribute( "uidnext" ) ) {
    UidNextAttribute* currentNextUid  = new UidNextAttribute( nextUid );
    col.addAttribute( currentNextUid );
    modifyNeeded = true;
  } else {
    UidNextAttribute* currentNextUid =
      static_cast<UidNextAttribute*>( col.attribute( "uidnext" ) );
    oldNextUid = currentNextUid->uidNext();
    if ( oldNextUid != nextUid ) {
      currentNextUid->setUidNext( nextUid );
      modifyNeeded = true;
    }
  }

  // Store the mailbox flags
  if ( !col.hasAttribute( "collectionflags" ) ) {
    Akonadi::CollectionFlagsAttribute *flagsAttribute  = new Akonadi::CollectionFlagsAttribute( flags );
    col.addAttribute( flagsAttribute );
    modifyNeeded = true;
  } else {
    Akonadi::CollectionFlagsAttribute *flagsAttribute =
      static_cast<Akonadi::CollectionFlagsAttribute*>( col.attribute( "collectionflags" ) );
    const QList<QByteArray> oldFlags = flagsAttribute->flags();
    if ( oldFlags != flags ) {
      flagsAttribute->setFlags( flags );
      modifyNeeded = true;
    }
  }

  quint64 oldHighestModSeq = 0;
  if ( highestModSeq > 0 ) {
    if ( !col.hasAttribute( "highestmodseq" ) ) {
      HighestModSeqAttribute *attr = new HighestModSeqAttribute( highestModSeq );
      col.addAttribute( attr );
      modifyNeeded = true;
    } else {
      HighestModSeqAttribute *attr = col.attribute<HighestModSeqAttribute>();
      if ( attr->highestModSequence() < highestModSeq ) {
        oldHighestModSeq = attr->highestModSequence();
        attr->setHighestModSeq( highestModSeq );
        modifyNeeded = true;
      } else if ( attr->highestModSequence() == highestModSeq ) {
        oldHighestModSeq = attr->highestModSequence();
      } else if ( attr->highestModSequence() > highestModSeq ) {
        // This situation should not happen. If it does, update the highestModSeq
        // attribute, but rather do a full sync
        attr->setHighestModSeq( highestModSeq );
        modifyNeeded = true;
      }
    }
  }

  if ( modifyNeeded )
    applyCollectionChanges( col );

  KIMAP::FetchJob::FetchScope scope;
  scope.parts.clear();
  scope.mode = KIMAP::FetchJob::FetchScope::FullHeaders;

  if ( col.cachePolicy()
       .localParts().contains( QLatin1String(Akonadi::MessagePart::Body) ) ) {
    scope.mode = KIMAP::FetchJob::FetchScope::Full;
  }

  const qint64 realMessageCount = col.statistics().count();
  m_fetchedMissingBodies = -1;

  // First check the uidvalidity, if this has changed, it means the folder
  // has been deleted and recreated. So we wipe out the messages and
  // retrieve all.
  if ( oldUidValidity != uidValidity && !firstTime
    && oldUidValidity != 0 && messageCount > 0 ) {
    kDebug( 5327 ) << "UIDVALIDITY check failed (" << oldUidValidity << "|"
                   << uidValidity << ") refetching " << mailBox;

    KIMAP::FetchJob *fetch = new KIMAP::FetchJob( m_session );
    fetch->setSequenceSet( KIMAP::ImapSet( 1, messageCount ) );
    fetch->setScope( scope );
    connect( fetch, SIGNAL(headersReceived(QString,QMap<qint64,qint64>,QMap<qint64,qint64>,QMap<qint64,KIMAP::MessageFlags>,QMap<qint64,KIMAP::MessagePtr>)),
           this, SLOT(onHeadersReceived(QString,QMap<qint64,qint64>,QMap<qint64,qint64>,QMap<qint64,KIMAP::MessageFlags>,QMap<qint64,KIMAP::MessagePtr>)) );
    connect( fetch, SIGNAL(result(KJob*)),
           this, SLOT(onHeadersFetchDone(KJob*)) );
    fetch->start();
  } else if ( messageCount > realMessageCount && messageCount > 0 ) {
    // The amount on the server is bigger than that we have in the cache
    // that probably means that there is new mail. Fetch missing.
    kDebug( 5327 ) << "Fetch missing: " << messageCount << " But: " << realMessageCount;

    KIMAP::FetchJob *fetch = new KIMAP::FetchJob( m_session );
    fetch->setSequenceSet( KIMAP::ImapSet( realMessageCount+1, messageCount ) );
    fetch->setScope( scope );
    fetch->setProperty( HIGHESTMODSEQ_PROPERTY, oldHighestModSeq );
    connect( fetch, SIGNAL(headersReceived(QString,QMap<qint64,qint64>,QMap<qint64,qint64>,QMap<qint64,KIMAP::MessageFlags>,QMap<qint64,KIMAP::MessagePtr>)),
           this, SLOT(onHeadersReceived(QString,QMap<qint64,qint64>,QMap<qint64,qint64>,QMap<qint64,KIMAP::MessageFlags>,QMap<qint64,KIMAP::MessagePtr>)) );
    connect( fetch, SIGNAL(result(KJob*)),
           this, SLOT(onHeadersFetchDone(KJob*)) );
    fetch->start();
  } else if ( messageCount == realMessageCount && oldNextUid != nextUid
           && oldNextUid != 0 && !firstTime && messageCount > 0 ) {
    // amount is right but uidnext is different.... something happened
    // behind our back...
    kDebug( 5327 ) << "UIDNEXT check failed, refetching mailbox";

    qint64 startIndex = 1;
    // one scenario we can recover from is that an equal amount of mails has been deleted and added while we were not looking
    // the amount has to be less or equal to (nextUid - oldNextUid) due to strictly ascending UIDs
    // so, we just have to reload the last (nextUid - oldNextUid) mails if the uidnext values seem sane
    if ( oldNextUid < nextUid && oldNextUid != 0 && !firstTime )
      startIndex = qMax( 1ll, messageCount - ( nextUid - oldNextUid ) );

    Q_ASSERT( startIndex >= 1 );
    Q_ASSERT( startIndex <= messageCount );

    KIMAP::FetchJob *fetch = new KIMAP::FetchJob( m_session );
    fetch->setSequenceSet( KIMAP::ImapSet( startIndex, messageCount ) );
    fetch->setScope( scope );
    connect( fetch, SIGNAL(headersReceived(QString,QMap<qint64,qint64>,QMap<qint64,qint64>,QMap<qint64,KIMAP::MessageFlags>,QMap<qint64,KIMAP::MessagePtr>)),
           this, SLOT(onHeadersReceived(QString,QMap<qint64,qint64>,QMap<qint64,qint64>,QMap<qint64,KIMAP::MessageFlags>,QMap<qint64,KIMAP::MessagePtr>)) );
    connect( fetch, SIGNAL(result(KJob*)),
           this, SLOT(onHeadersFetchDone(KJob*)) );
    fetch->start();
  } else if (!m_messageUidsMissingBody.isEmpty() ) {
    m_fetchedMissingBodies = 0;
    //fetch missing uids
    KIMAP::FetchJob *fetch = new KIMAP::FetchJob( m_session );
    KIMAP::ImapSet imapSet;
    imapSet.add( m_messageUidsMissingBody );
    fetch->setSequenceSet( imapSet );
    fetch->setScope( scope );
    fetch->setUidBased( true );
    // Do a full flags sync if some messages were removed, otherwise do just an incremental update
    fetch->setProperty( HIGHESTMODSEQ_PROPERTY, ( messageCount < realMessageCount ) ? 0 : oldHighestModSeq );
    connect( fetch, SIGNAL(headersReceived(QString,QMap<qint64,qint64>,QMap<qint64,qint64>,QMap<qint64,KIMAP::MessageFlags>,QMap<qint64,KIMAP::MessagePtr>)),
             this, SLOT(onHeadersReceived(QString,QMap<qint64,qint64>,QMap<qint64,qint64>,QMap<qint64,KIMAP::MessageFlags>,QMap<qint64,KIMAP::MessagePtr>)) );
    connect( fetch, SIGNAL(result(KJob*)),
             this, SLOT(onHeadersFetchDone(KJob*)) );
    fetch->start();
  } else if ( messageCount > 0 ) {
    if ( messageCount < realMessageCount ) {
        // Some messages were removed, list all flags to find out which messages
        // are missing
        kDebug( 5327 ) << ( realMessageCount - messageCount ) << "messages were removed from maildir";
    } else {
        kDebug( 5327 ) << "All fine, asking for changed flags looking for changes";
    }
    listFlagsForImapSet( KIMAP::ImapSet( 1, messageCount ), ( messageCount < realMessageCount ) ? 0 : oldHighestModSeq );
  } else {
    kDebug( 5327 ) << "No messages present so we are done";
    itemsRetrievalDone();
  }
}

void RetrieveItemsTask::listFlagsForImapSet( const KIMAP::ImapSet& set, qint64 highestModSeq )
{
  KIMAP::FetchJob::FetchScope scope;
  scope.parts.clear();
  scope.mode = KIMAP::FetchJob::FetchScope::Flags;
  if(serverCapabilities().contains( QLatin1String( "CONDSTORE" ))) {
      scope.changedSince = highestModSeq;
  }

  KIMAP::FetchJob* fetch = new KIMAP::FetchJob( m_session );
  fetch->setSequenceSet( set );
  fetch->setScope( scope );
  connect( fetch, SIGNAL(headersReceived(QString,QMap<qint64,qint64>,QMap<qint64,qint64>,QMap<qint64,KIMAP::MessageFlags>,QMap<qint64,KIMAP::MessagePtr>)),
           this, SLOT(onFlagsReceived(QString,QMap<qint64,qint64>,QMap<qint64,qint64>,QMap<qint64,KIMAP::MessageFlags>,QMap<qint64,KIMAP::MessagePtr>)) );
  connect( fetch, SIGNAL(result(KJob*)),
           this, SLOT(onFlagsFetchDone(KJob*)) );
  fetch->start();
}

void RetrieveItemsTask::onHeadersReceived( const QString &mailBox, const QMap<qint64, qint64> &uids,
                                           const QMap<qint64, qint64> &sizes,
                                           const QMap<qint64, KIMAP::MessageFlags> &flags,
                                           const QMap<qint64, KIMAP::MessagePtr> &messages )
{
  Akonadi::Item::List addedItems;

  foreach ( qint64 number, uids.keys() ) { //krazy:exclude=foreach
    Akonadi::Item i;
    i.setRemoteId( QString::number( uids[number] ) );
    i.setMimeType( KMime::Message::mimeType() );
    i.setPayload( KMime::Message::Ptr( messages[number] ) );
    i.setSize( sizes[number] );

    // update status flags
    if ( KMime::isSigned( messages[number].get() ) )
      i.setFlag( Akonadi::MessageFlags::Signed );
    if ( KMime::isEncrypted( messages[number].get() ) )
      i.setFlag( Akonadi::MessageFlags::Encrypted );
    if ( KMime::isInvitation( messages[number].get() ) )
      i.setFlag( Akonadi::MessageFlags::HasInvitation );
    if ( KMime::hasAttachment( messages[number].get() ) )
      i.setFlag( Akonadi::MessageFlags::HasAttachment );

    const QList<QByteArray> akonadiFlags = toAkonadiFlags( flags[number] );
    foreach ( const QByteArray &flag, akonadiFlags ) {
      i.setFlag( flag );
    }
    //kDebug( 5327 ) << "Flags: " << i.flags();
    addedItems << i;
  }

  const qint64 highestModSeq = extractHighestModSeq( static_cast<KJob*>( sender() ) );
  if ( highestModSeq == 0 ) {
    itemsRetrieved( addedItems );
  } else {
    itemsRetrievedIncremental( addedItems, Akonadi::Item::List() );
  }

  //m_fetchedMissingBodies is -1 if we fetch for other reason, but missing bodies
  if ( m_fetchedMissingBodies != -1 ) {
    m_fetchedMissingBodies += addedItems.count();
    emit status(Akonadi::AgentBase::Running,
                i18nc( "@info:status", "Fetching missing mail bodies in %3: %1/%2", m_fetchedMissingBodies, m_messageUidsMissingBody.count(), mailBox));
  }
}

void RetrieveItemsTask::onHeadersFetchDone( KJob *job )
{
  if ( job->error() ) {
      cancelTask( job->errorString() );
      m_fetchedMissingBodies = -1;
      return;
  }

  const qint64 highestModSeq = extractHighestModSeq( job );
  if ( highestModSeq > 0 ) {
    // Calling itemsRetrievalDone() before previous call to itemsRetrievedIncremental()
    // behaves like if we called itemsRetrieved(Items::List()), so make sure
    // Akonadi knows we did incremental fetch that came up with no changes
    itemsRetrievedIncremental( Akonadi::Item::List(), Akonadi::Item::List() );
  }

  KIMAP::FetchJob *fetch = static_cast<KIMAP::FetchJob*>( job );
  KIMAP::ImapSet alreadyFetched = fetch->sequenceSet();

  // If this is the first fetch of a folder, skip getting flags, we
  // already have them all from the previous full fetch. This is not
  // just an optimization, as incremental retrieval assumes nothing
  // will be listed twice.
  if ( m_fetchedMissingBodies == -1 && alreadyFetched.intervals().first().begin() <= 1 ) {
    itemsRetrievalDone();
    return;
  }

  // Fetch flags of all items that were not fetched by the fetchJob. After
  // that /all/ items in the folder are synced.
  KIMAP::ImapSet::Id end = 0;
  if ( m_fetchedMissingBodies == -1) {
      end = alreadyFetched.intervals().first().begin() - 1;
  }
  KIMAP::ImapSet set( 1, end );
  listFlagsForImapSet( set, highestModSeq );
}

void RetrieveItemsTask::onFlagsReceived( const QString &mailBox, const QMap<qint64, qint64> &uids,
                                         const QMap<qint64, qint64> &sizes,
                                         const QMap<qint64, KIMAP::MessageFlags> &flags,
                                         const QMap<qint64, KIMAP::MessagePtr> &messages )
{
  Q_UNUSED( mailBox );
  Q_UNUSED( sizes );
  Q_UNUSED( messages );

  Akonadi::Item::List changedItems;

  foreach ( qint64 number, uids.keys() ) { //krazy:exclude=foreach
    Akonadi::Item i;
    i.setRemoteId( QString::number( uids[number] ) );
    i.setMimeType( KMime::Message::mimeType() );
    i.setFlags( Akonadi::Item::Flags::fromList( toAkonadiFlags( flags[number] ) ) );

    //kDebug( 5327 ) << "Flags: " << i.flags();
    changedItems << i;
  }

  if ( !changedItems.isEmpty() ) {
    KIMAP::FetchJob *fetch = static_cast<KIMAP::FetchJob*>( sender() );
    // When changedsince is invalid, we do a full-sync. In that case use itemsRetrieved()
    // so that we correctly update removed moessages
    if ( fetch->scope().changedSince == 0 ) {
        itemsRetrieved( changedItems );
    } else {
        itemsRetrievedIncremental( changedItems, Akonadi::Item::List() );
    }
  }
}

void RetrieveItemsTask::onFlagsFetchDone( KJob *job )
{
  if ( job->error() ) {
    cancelTask( job->errorString() );
  } else {
    KIMAP::FetchJob *fetch = static_cast<KIMAP::FetchJob*>( job );
    if ( fetch->scope().changedSince > 0 ) {
        // In case there were no changed flags, make sure Akonadi knows that there
        // were no changes
        itemsRetrievedIncremental( Akonadi::Item::List(), Akonadi::Item::List() );
    }
    itemsRetrievalDone();
  }
}

qint64 RetrieveItemsTask::extractHighestModSeq( KJob *job ) const
{
    qint64 highestModSeq = 0;
    const QVariant v = job->property( HIGHESTMODSEQ_PROPERTY );
    if ( v.isValid() ) {
        bool ok = false;
        highestModSeq = v.toLongLong( &ok );
        if ( !ok ) {
            return 0;
        }
    }

    return highestModSeq;
}



