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

#include "retrievecollectionmetadatatask.h"

#include <QtCore/QDateTime>

#include <kimap/getacljob.h>
#include <kimap/getmetadatajob.h>
#include <kimap/getquotarootjob.h>
#include <kimap/myrightsjob.h>
#include <kimap/rfccodecs.h>
#include <kimap/session.h>
#include <klocale.h>

#include <akonadi/collectionquotaattribute.h>
#include <akonadi/entitydisplayattribute.h>
#include "collectionannotationsattribute.h"
#include "imapaclattribute.h"
#include "imapquotaattribute.h"
#include "noselectattribute.h"
#include "collectionmetadatahelper.h"

RetrieveCollectionMetadataTask::RetrieveCollectionMetadataTask( ResourceStateInterface::Ptr resource, QObject *parent )
  : ResourceTask( CancelIfNoSession, resource, parent ),
    m_pendingMetaDataJobs( 0 )
{
}

RetrieveCollectionMetadataTask::~RetrieveCollectionMetadataTask()
{
}

void RetrieveCollectionMetadataTask::doStart( KIMAP::Session *session )
{
  kDebug( 5327 ) << collection().remoteId();

  // Prevent fetching metadata from noselect folders.
  if ( collection().hasAttribute( "noselect" ) ) {
    NoSelectAttribute* noselect = static_cast<NoSelectAttribute*>( collection().attribute( "noselect" ) );
    if ( noselect->noSelect() ) {
      kDebug( 5327 ) << "No Select folder";
      endTaskIfNeeded();
      return;
    }
  }

  m_session = session;
  m_collection = collection();
  const QString mailBox = mailBoxForCollection( m_collection );
  const QStringList capabilities = serverCapabilities();

  m_pendingMetaDataJobs = 0;

  // First get the annotations from the mailbox if it's supported
  if ( capabilities.contains( QLatin1String("METADATA") ) || capabilities.contains( QLatin1String("ANNOTATEMORE") ) ) {
    KIMAP::GetMetaDataJob *meta = new KIMAP::GetMetaDataJob( session );
    meta->setMailBox( mailBox );
    if ( capabilities.contains( QLatin1String("METADATA") ) ) {
      meta->setServerCapability( KIMAP::MetaDataJobBase::Metadata );
      meta->addRequestedEntry( "/shared" );
      meta->setDepth( KIMAP::GetMetaDataJob::AllLevels );
    } else {
      meta->setServerCapability( KIMAP::MetaDataJobBase::Annotatemore );
      meta->addEntry( "*", "value.shared" );
    }
    connect( meta, SIGNAL(result(KJob*)), SLOT(onGetMetaDataDone(KJob*)) );
    m_pendingMetaDataJobs++;
    meta->start();
  }

  // Get the ACLs from the mailbox if it's supported
  if ( capabilities.contains( QLatin1String("ACL") ) ) {
    KIMAP::MyRightsJob *rights = new KIMAP::MyRightsJob( session );
    rights->setMailBox( mailBox );
    connect( rights, SIGNAL(result(KJob*)), SLOT(onRightsReceived(KJob*)) );
    m_pendingMetaDataJobs++;
    rights->start();
  }

  // Get the QUOTA info from the mailbox if it's supported
  if ( capabilities.contains( QLatin1String("QUOTA") ) ) {
    KIMAP::GetQuotaRootJob *quota = new KIMAP::GetQuotaRootJob( session );
    quota->setMailBox( mailBox );
    connect( quota, SIGNAL(result(KJob*)), SLOT(onQuotasReceived(KJob*)) );
    m_pendingMetaDataJobs++;
    quota->start();
  }

  // the server does not have any of the capabilities needed to get extra info, so this
  // step is done here
  if ( m_pendingMetaDataJobs == 0 ) {
    endTaskIfNeeded();
  }
}

void RetrieveCollectionMetadataTask::onGetMetaDataDone( KJob *job )
{
  m_pendingMetaDataJobs--;
  if ( job->error() ) {
    kWarning() << "Get metadata failed: " << job->errorString();
    endTaskIfNeeded();
    return; // Well, no metadata for us then...
  }

  KIMAP::GetMetaDataJob *meta = qobject_cast<KIMAP::GetMetaDataJob*>( job );
  QMap<QByteArray, QByteArray> rawAnnotations = meta->allMetaData();

  // filter out unused and annoying Cyrus annotation /vendor/cmu/cyrus-imapd/lastupdate
  // which contains the current date and time and thus constantly changes for no good
  // reason which triggers a change notification and thus a bunch of Akonadi operations
  rawAnnotations.remove( "/shared/vendor/cmu/cyrus-imapd/lastupdate" );
  rawAnnotations.remove( "/private/vendor/cmu/cyrus-imapd/lastupdate" );

  // Store the mailbox metadata
  Akonadi::CollectionAnnotationsAttribute *annotationsAttribute =
    m_collection.attribute<Akonadi::CollectionAnnotationsAttribute>( Akonadi::Collection::AddIfMissing );
  const QMap<QByteArray, QByteArray> oldAnnotations = annotationsAttribute->annotations();
  if ( oldAnnotations != rawAnnotations ) {
    annotationsAttribute->setAnnotations( rawAnnotations );
  }

  endTaskIfNeeded();
}

void RetrieveCollectionMetadataTask::onGetAclDone( KJob *job )
{
  m_pendingMetaDataJobs--;
  if ( job->error() ) {
    kWarning() << "GetACL failed: " << job->errorString();
    endTaskIfNeeded();
    return; // Well, no metadata for us then...
  }

  KIMAP::GetAclJob *acl = qobject_cast<KIMAP::GetAclJob*>( job );

  // Store the mailbox ACLs
  Akonadi::ImapAclAttribute *aclAttribute
    = m_collection.attribute<Akonadi::ImapAclAttribute>( Akonadi::Collection::AddIfMissing );
  const QMap<QByteArray, KIMAP::Acl::Rights> oldRights = aclAttribute->rights();
  if ( oldRights != acl->allRights() ) {
    aclAttribute->setRights( acl->allRights() );
  }

  endTaskIfNeeded();
}

void RetrieveCollectionMetadataTask::onRightsReceived( KJob *job )
{
  m_pendingMetaDataJobs--;
  if ( job->error() ) {
    kWarning() << "MyRights failed: " << job->errorString();
    endTaskIfNeeded();
    return; // Well, no metadata for us then...
  }

  KIMAP::MyRightsJob *rightsJob = qobject_cast<KIMAP::MyRightsJob*>( job );

  //Default value in case we have nothing better available
  KIMAP::Acl::Rights parentRights = KIMAP::Acl::CreateMailbox | KIMAP::Acl::Create;

  //FIXME I don't think we have the parent's acl's available
  if (collection().parentCollection().attribute<Akonadi::ImapAclAttribute>()) {
    parentRights = myRights(collection().parentCollection());
  }

  const KIMAP::Acl::Rights imapRights = rightsJob->rights();

//  kDebug( 5327 ) << collection.remoteId()
//                 << "imapRights:" << imapRights
//                 << "newRights:" << newRights
//                 << "oldRights:" << collection.rights();

  const bool isNewCollection = !m_collection.hasAttribute<Akonadi::ImapAclAttribute>();
  const bool accessRevoked = CollectionMetadataHelper::applyRights(m_collection, imapRights, parentRights);
  if ( accessRevoked && !isNewCollection ) {
    // write access revoked
    const QString collectionName = m_collection.displayName();

    showInformationDialog( i18n( "<p>Your access rights to folder <b>%1</b> have been restricted, "
                                 "it will no longer be possible to add messages to this folder.</p>",
                                 collectionName ),
                           i18n( "Access rights revoked" ), QLatin1String("ShowRightsRevokedWarning") );
  }

  // Store the mailbox ACLs
  Akonadi::ImapAclAttribute *aclAttribute
    = m_collection.attribute<Akonadi::ImapAclAttribute>( Akonadi::Collection::AddIfMissing );
  const KIMAP::Acl::Rights oldRights = aclAttribute->myRights();
  if ( oldRights != imapRights ) {
    aclAttribute->setMyRights( imapRights );
  }

  //The a right is required to list acl's
  if ( imapRights & KIMAP::Acl::Admin ) {
    KIMAP::GetAclJob *acl = new KIMAP::GetAclJob( m_session );
    acl->setMailBox( mailBoxForCollection( m_collection ) );
    connect( acl, SIGNAL(result(KJob*)), SLOT(onGetAclDone(KJob*)) );
    m_pendingMetaDataJobs++;
    acl->start();
  }

  endTaskIfNeeded();
}

void RetrieveCollectionMetadataTask::onQuotasReceived( KJob *job )
{
  m_pendingMetaDataJobs--;
  if ( job->error() ) {
    kWarning() << "Quota retrieval failed: " << job->errorString();
    endTaskIfNeeded();
    return; // Well, no metadata for us then...
  }

  KIMAP::GetQuotaRootJob *quotaJob = qobject_cast<KIMAP::GetQuotaRootJob*>( job );
  const QString &mailBox = mailBoxForCollection( m_collection );

  QList<QByteArray> newRoots = quotaJob->roots();
  QList< QMap<QByteArray, qint64> > newLimits;
  QList< QMap<QByteArray, qint64> > newUsages;
  qint64 newCurrent = -1;
  qint64 newMax = -1;

  foreach ( const QByteArray &root, newRoots ) {
    newLimits << quotaJob->allLimits( root );
    newUsages << quotaJob->allUsages( root );

    const QString &decodedRoot = QString::fromUtf8( KIMAP::decodeImapFolderName( root ) );

    if ( newRoots.size() == 1 || decodedRoot == mailBox ) {
      newCurrent = newUsages.last()["STORAGE"] * 1024;
      newMax = newLimits.last()["STORAGE"] * 1024;
    }
  }

  // Store the mailbox IMAP Quotas
  Akonadi::ImapQuotaAttribute *imapQuotaAttribute
    = m_collection.attribute<Akonadi::ImapQuotaAttribute>( Akonadi::Collection::AddIfMissing );
  const QList<QByteArray> oldRoots = imapQuotaAttribute->roots();
  const QList< QMap<QByteArray, qint64> > oldLimits = imapQuotaAttribute->limits();
  const QList< QMap<QByteArray, qint64> > oldUsages = imapQuotaAttribute->usages();

  if ( oldRoots != newRoots
    || oldLimits != newLimits
    || oldUsages != newUsages ) {
    imapQuotaAttribute->setQuotas( newRoots, newLimits, newUsages );
  }

  // Store the collection Quota
  Akonadi::CollectionQuotaAttribute *quotaAttribute
    = m_collection.attribute<Akonadi::CollectionQuotaAttribute>( Akonadi::Collection::AddIfMissing );
  qint64 oldCurrent = quotaAttribute->currentValue();
  qint64 oldMax = quotaAttribute->maximumValue();

  if ( oldCurrent != newCurrent
    || oldMax != newMax ) {
    quotaAttribute->setCurrentValue( newCurrent );
    quotaAttribute->setMaximumValue( newMax );
  }

  endTaskIfNeeded();
}

void RetrieveCollectionMetadataTask::endTaskIfNeeded()
{
  if ( m_pendingMetaDataJobs <= 0 ) {
    collectionAttributesRetrieved( m_collection );
  }
}
