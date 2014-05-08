/*
    Copyright (c) 2007 Till Adam <adam@kde.org>
    Copyright (C) 2008 Omat Holding B.V. <info@omat.nl>
    Copyright (C) 2009 Kevin Ottens <ervin@kde.org>

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

#include "imapresource.h"

#include <QHostInfo>
#include <QSettings>

#include <kdebug.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <KWindowSystem>
#include <Akonadi/CollectionModifyJob>

#include <akonadi/agentmanager.h>
#include <akonadi/attributefactory.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/changerecorder.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/itemfetchjob.h>
#include <akonadi/specialcollections.h>
#include <akonadi/session.h>
#include <akonadi/kmime/messageparts.h>

#include "collectionannotationsattribute.h"
#include "collectionflagsattribute.h"
#include "imapaclattribute.h"
#include "imapquotaattribute.h"
#include "noselectattribute.h"
#include "timestampattribute.h"
#include "uidvalidityattribute.h"
#include "uidnextattribute.h"
#include "highestmodseqattribute.h"

#include "setupserver.h"
#include "settings.h"
#include "imapaccount.h"
#include "imapidlemanager.h"
#include "resourcestate.h"
#include "subscriptiondialog.h"

#include "addcollectiontask.h"
#include "additemtask.h"
#include "changecollectiontask.h"
#include "changeitemsflagstask.h"
#include "changeitemtask.h"
#include "expungecollectiontask.h"
#include "movecollectiontask.h"
#include "moveitemstask.h"
#include "removecollectionrecursivetask.h"
#include "removeitemstask.h"
#include "retrievecollectionmetadatatask.h"
#include "retrievecollectionstask.h"
#include "retrieveitemtask.h"
#include "retrieveitemstask.h"
#include "searchtask.h"

#include "settingspasswordrequester.h"
#include "sessionpool.h"
#include "sessionuiproxy.h"

#include "resourceadaptor.h"

#ifdef MAIL_SERIALIZER_PLUGIN_STATIC

Q_IMPORT_PLUGIN(akonadi_serializer_mail)
#endif

Q_DECLARE_METATYPE(QList<qint64>)
Q_DECLARE_METATYPE(QWeakPointer<QObject>)

using namespace Akonadi;

ImapResource::ImapResource( const QString &id )
  : ResourceBase( id ),
    m_pool( new SessionPool( 2, this ) ),
    mSubscriptions( 0 ),
    m_idle( 0 )
{
  if ( name() == identifier() ) {
    const QString agentType = AgentManager::self()->instance( identifier() ).type().identifier();
    const QString agentsrcFile = KGlobal::dirs()->localxdgconfdir() + QLatin1String("akonadi/agentsrc");

    const QSettings agentsrc( agentsrcFile, QSettings::IniFormat );
    const int instanceCounter = agentsrc.value(
                                  QString::fromLatin1( "InstanceCounters/%1/InstanceCounter" ).arg( agentType ),
                                  -1 ).toInt();

    if ( instanceCounter > 0 ) {
      setName( i18n( "IMAP Account %1", instanceCounter ) );
    } else {
      setName( i18n( "IMAP Account" ) );
    }
  }

  m_pool->setPasswordRequester( new SettingsPasswordRequester( this, m_pool ) );
  m_pool->setSessionUiProxy( SessionUiProxy::Ptr( new SessionUiProxy ) );

  connect( m_pool, SIGNAL(connectDone(int,QString)),
           this, SLOT(onConnectDone(int,QString)) );
  connect( m_pool, SIGNAL(connectionLost(KIMAP::Session*)),
           this, SLOT(onConnectionLost(KIMAP::Session*)) );

  Akonadi::AttributeFactory::registerAttribute<UidValidityAttribute>();
  Akonadi::AttributeFactory::registerAttribute<UidNextAttribute>();
  Akonadi::AttributeFactory::registerAttribute<NoSelectAttribute>();
  Akonadi::AttributeFactory::registerAttribute<TimestampAttribute>();
  Akonadi::AttributeFactory::registerAttribute<HighestModSeqAttribute>();

  Akonadi::AttributeFactory::registerAttribute<CollectionAnnotationsAttribute>();
  Akonadi::AttributeFactory::registerAttribute<CollectionFlagsAttribute>();

  Akonadi::AttributeFactory::registerAttribute<ImapAclAttribute>();
  Akonadi::AttributeFactory::registerAttribute<ImapQuotaAttribute>();

  // For QMetaObject::invokeMethod()
  qRegisterMetaType<QList<qint64> >();

  changeRecorder()->fetchCollection( true );
  changeRecorder()->collectionFetchScope().setAncestorRetrieval( CollectionFetchScope::All );
  changeRecorder()->collectionFetchScope().setIncludeStatistics( true );
  changeRecorder()->itemFetchScope().fetchFullPayload( true );
  changeRecorder()->itemFetchScope().setAncestorRetrieval( ItemFetchScope::All );
  changeRecorder()->itemFetchScope().setFetchModificationTime( false );
 //(Andras) disable now, as tokoe reported problems with it and the mail filter: changeRecorder()->fetchChangedOnly( true );

  setHierarchicalRemoteIdentifiersEnabled( true );
  setItemTransactionMode( ItemSync::MultipleTransactions ); // we can recover from incomplete syncs, so we can use a faster mode
  ItemFetchScope scope( changeRecorder()->itemFetchScope() );
  scope.fetchFullPayload( false );
  scope.setAncestorRetrieval( ItemFetchScope::None );
  setItemSynchronizationFetchScope( scope );
  setDisableAutomaticItemDeliveryDone( true );
  setItemSyncBatchSize( 100 );

  connect( this, SIGNAL(reloadConfiguration()), SLOT(reconnect()) );

  Settings::self(); // make sure the D-Bus settings interface is up
  new ResourceAdaptor( this );
  setNeedsNetwork( needsNetwork() );

  // Migration issue: trash folder had ID in config, but didn't have SpecialCollections attribute, fix that.
  if (!Settings::self()->trashCollectionMigrated()) {
    const Akonadi::Collection::Id trashCollection = Settings::self()->trashCollection();
    if (trashCollection != -1) {
      Collection attributeCollection(trashCollection);
      SpecialCollections::setSpecialCollectionType("trash", attributeCollection);
    }
    Settings::self()->setTrashCollectionMigrated(true);
  }

  m_statusMessageTimer = new QTimer( this );
  m_statusMessageTimer->setSingleShot( true );
  connect( m_statusMessageTimer, SIGNAL(timeout()), SLOT(clearStatusMessage()) );
  connect( this, SIGNAL(error(QString)), SLOT(showError(QString)) );
}

ImapResource::~ImapResource()
{
  //Destroy everything that could cause callbacks immediately, otherwise the callbacks can result in a crash.

  if ( m_idle ) {
    delete m_idle;
    m_idle = 0;
  }

  Q_FOREACH (ResourceTask* task, m_taskList) {
    delete task;
  }
  m_taskList.clear();

  delete m_pool;
}

void ImapResource::aboutToQuit()
{
  //TODO the resource would ideally have to signal when it's done with logging out etc, before the destructor gets called
  if ( m_idle ) {
    m_idle->stop();
  }

  Q_FOREACH (ResourceTask* task, m_taskList) {
    task->kill();
  }

  m_pool->disconnect();
}

// -----------------------------------------------------------------------------

KDialog* ImapResource::createConfigureDialog(WId windowId)
{
  SetupServer *dlg = new SetupServer( this, windowId );
  KWindowSystem::setMainWindow( dlg, windowId );
  dlg->setWindowIcon( KIcon( QLatin1String("network-server") ) );
  connect(dlg, SIGNAL(finished(int)), this, SLOT(onConfigurationDone(int)));;
  return dlg;
}

void ImapResource::onConfigurationDone(int result)
{
  SetupServer *dlg = qobject_cast<SetupServer*>(sender());
  if (result) {
    if ( dlg->shouldClearCache() ) {
      clearCache();
    }
    Settings::self()->writeConfig();
  }
  dlg->deleteLater();
}

void ImapResource::configure( WId windowId )
{
  if ( createConfigureDialog( windowId )->exec() == QDialog::Accepted ) {
    emit configurationDialogAccepted();
    reconnect();
  } else {
    emit configurationDialogRejected();
  }
}



// ----------------------------------------------------------------------------------

void ImapResource::startConnect( const QVariant& )
{
  if ( Settings::self()->imapServer().isEmpty() ) {
    setOnline( false );
    emit status( NotConfigured, i18n( "No server configured yet." ) );
    taskDone();
    return;
  }

  m_pool->disconnect(); // reset all state, delete any old account
  ImapAccount *account = new ImapAccount;
  Settings::self()->loadAccount( account );

  const bool result = m_pool->connect( account );
  Q_ASSERT( result );
  Q_UNUSED( result );
}

int ImapResource::configureSubscription(qlonglong windowId)
{
  if (mSubscriptions)
     return 0;

  if ( !m_pool->account() )
     return -2;
  const QString password = Settings::self()->password();
  if ( password.isEmpty() )
     return -1;

  mSubscriptions = new SubscriptionDialog( 0, SubscriptionDialog::AllowToEnableSubscription );
  if(windowId) {
#ifndef Q_WS_WIN
    KWindowSystem::setMainWindow( mSubscriptions, windowId );
#else
    KWindowSystem::setMainWindow( mSubscriptions, (HWND)windowId );
#endif
  }
  mSubscriptions->setCaption( i18nc( "@title:window", "Serverside Subscription" ) );
  mSubscriptions->setWindowIcon( KIcon( QLatin1String("network-server") ) );
  mSubscriptions->connectAccount( *m_pool->account(), password );
  mSubscriptions->setSubscriptionEnabled( Settings::self()->subscriptionEnabled() );

  if ( mSubscriptions->exec() ) {
    Settings::self()->setSubscriptionEnabled( mSubscriptions->subscriptionEnabled() );
    Settings::self()->writeConfig();
    emit configurationDialogAccepted();
    reconnect();
  }
  delete mSubscriptions;

  return 0;
}

void ImapResource::onConnectDone( int errorCode, const QString &errorString )
{
  switch ( errorCode ) {
  case SessionPool::NoError:
    setOnline( true );
    taskDone();
    emit status( Idle, i18n( "Connection established." ) );

    synchronizeCollectionTree();
    break;

  case SessionPool::PasswordRequestError:
  case SessionPool::EncryptionError:
  case SessionPool::LoginFailError:
  case SessionPool::CapabilitiesTestError:
  case SessionPool::IncompatibleServerError:
    setOnline( false );
    emit status( Broken, errorString );
    cancelTask();
    return;

  case SessionPool::CouldNotConnectError:
    emit status( Idle, i18n( "Server is not available." ) );
    deferTask();
    setTemporaryOffline((m_pool->account() && m_pool->account()->timeout() > 0) ? m_pool->account()->timeout() : 300);
    return;

  case SessionPool::ReconnectNeededError:
    reconnect();
    return;

  case SessionPool::NoAvailableSessionError:
    kFatal() << "Shouldn't happen";
    return;
  }
}

void ImapResource::onConnectionLost( KIMAP::Session */*session*/ )
{
  if ( !m_pool->isConnected() ) {
    reconnect();
  }
}

ResourceStateInterface::Ptr ImapResource::createResourceState(const TaskArguments &args)
{
  return ResourceStateInterface::Ptr(new ResourceState(this, args));
}

// ----------------------------------------------------------------------------------

bool ImapResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  // The collection name is empty here...
  //emit status( AgentBase::Running, i18nc( "@info:status", "Retrieving item in '%1'", item.parentCollection().name() ) );

  RetrieveItemTask *task = new RetrieveItemTask( createResourceState(TaskArguments(item, parts)), this );
  task->start( m_pool );
  queueTask( task );
  return true;
}

void ImapResource::itemAdded( const Item &item, const Collection &collection )
{
  emit status( AgentBase::Running, i18nc( "@info:status", "Adding item in '%1'", collection.name() ) );

  startTask(new AddItemTask( createResourceState(TaskArguments(item, collection)), this ));
}

void ImapResource::itemChanged( const Item &item, const QSet<QByteArray> &parts )
{
  emit status( AgentBase::Running, i18nc( "@info:status", "Updating item in '%1'", item.parentCollection().name() ) );

  startTask(new ChangeItemTask( createResourceState(TaskArguments(item, parts)), this ));
}

void ImapResource::itemsFlagsChanged( const Item::List& items, const QSet< QByteArray >& addedFlags,
                                      const QSet< QByteArray >& removedFlags )
{
  emit status( AgentBase::Running, i18nc( "@info:status", "Updating items" ) );

  startTask(new ChangeItemsFlagsTask( createResourceState(TaskArguments(items, addedFlags, removedFlags)), this ));
}

void ImapResource::itemsRemoved( const Akonadi::Item::List &items )
{
  const QString mailBox = ResourceStateInterface::mailBoxForCollection( items.first().parentCollection(), false );
  if ( mailBox.isEmpty() ) {
    // this item will be removed soon by its parent collection
    changeProcessed();
    return;
  }

  emit status( AgentBase::Running, i18nc( "@info:status", "Removing items" ) );

  startTask(new RemoveItemsTask( createResourceState(TaskArguments(items)), this ));
}

void ImapResource::itemsMoved( const Akonadi::Item::List &items, const Akonadi::Collection &source,
                               const Akonadi::Collection &destination )
{
  if ( items.first().parentCollection() != destination ) { // should have been set by the server
    kWarning() << "Collections don't match: destination=" << destination.id()
               << "; items parent=" << items.first().parentCollection().id()
               << "; source collection=" << source.id();
    //Q_ASSERT( false );
    //TODO: Find out why this happens
    cancelTask();
    return;
  }

  emit status( AgentBase::Running, i18nc( "@info:status", "Moving items from '%1' to '%2'", source.name(), destination.name() ) );

  startTask(new MoveItemsTask( createResourceState(TaskArguments(items, source, destination)), this ));
}



// ----------------------------------------------------------------------------------

void ImapResource::retrieveCollections()
{
  emit status( AgentBase::Running, i18nc( "@info:status", "Retrieving folders" ) );

  startTask(new RetrieveCollectionsTask( createResourceState(TaskArguments()), this ));
}

void ImapResource::triggerCollectionExtraInfoJobs( const QVariant &collectionVariant )
{
  const Collection collection( collectionVariant.value<Collection>() );
  emit status( AgentBase::Running, i18nc( "@info:status", "Retrieving extra folder information for '%1'", collection.name() ) );

  //The collection that we received is potentially outdated.
  //Using it would overwrite attributes with old values.
  //FIXME: because this is async and not part of the resourcetask, it can't be killed. ResourceBase should just provide an up-to date copy of the collection.
  Akonadi::CollectionFetchJob *fetchJob = new Akonadi::CollectionFetchJob(collection, CollectionFetchJob::Base, this);
  fetchJob->fetchScope().setAncestorRetrieval( CollectionFetchScope::All );
  fetchJob->fetchScope().setIncludeStatistics( true );
  connect(fetchJob, SIGNAL(result(KJob*)), this, SLOT(onMetadataCollectionFetchDone(KJob*)));
}

void ImapResource::onMetadataCollectionFetchDone(KJob *job)
{
  if (job->error()) {
    kWarning() << "Failed to retrieve collection before RetrieveCollectionMetadataTask " << job->errorString();
    cancelTask(i18n("Failed to collection metadata."));
    return;
  }

  Akonadi::CollectionFetchJob *fetchJob = static_cast<Akonadi::CollectionFetchJob*>(job);
  Q_ASSERT(fetchJob->collections().size() == 1);

  startTask(new RetrieveCollectionMetadataTask( createResourceState(TaskArguments(fetchJob->collections().first())), this ));
}

void ImapResource::retrieveItems( const Collection &col )
{
  scheduleCustomTask( this, "triggerCollectionExtraInfoJobs", QVariant::fromValue( col ), ResourceBase::Append );

  //The collection that we receive was fetched when the task was scheduled, it is therefore possible that it is outdated.
  //We refetch the collection since we rely on up-to-date annotations.
  //FIXME: because this is async and not part of the resourcetask, it can't be killed. ResourceBase should just provide an up-to date copy of the collection.
  Akonadi::CollectionFetchJob *fetchJob = new Akonadi::CollectionFetchJob(col, CollectionFetchJob::Base, this);
  fetchJob->fetchScope().setAncestorRetrieval( CollectionFetchScope::All );
  fetchJob->fetchScope().setIncludeStatistics( true );
  connect(fetchJob, SIGNAL(result(KJob*)), this, SLOT(onItemRetrievalCollectionFetchDone(KJob*)));
}

void ImapResource::onItemRetrievalCollectionFetchDone(KJob *job)
{
  if (job->error()) {
    kWarning() << "Failed to retrieve collection before RetrieveItemsTask: " << job->errorString();
    cancelTask(i18n("Failed to retrieve items."));
    return;
  }

  Akonadi::CollectionFetchJob *fetchJob = static_cast<Akonadi::CollectionFetchJob*>(job);
  Q_ASSERT(fetchJob->collections().size() == 1);

  setItemStreamingEnabled( true );

  RetrieveItemsTask *task = new RetrieveItemsTask( createResourceState(TaskArguments(fetchJob->collections().first())), this);
  connect(task, SIGNAL(status(int,QString)), SIGNAL(status(int,QString)));
  connect(this, SIGNAL(retrieveNextItemSyncBatch(int)), task, SLOT(onReadyForNextBatch(int)));
  startTask(task);
}

void ImapResource::collectionAdded( const Collection & collection, const Collection &parent )
{
  emit status( AgentBase::Running, i18nc( "@info:status", "Creating folder '%1'", collection.name() ) );
  startTask(new AddCollectionTask( createResourceState(TaskArguments(collection, parent)), this ));
}

void ImapResource::collectionChanged( const Collection &collection, const QSet<QByteArray> &parts )
{
  emit status( AgentBase::Running, i18nc( "@info:status", "Updating folder '%1'", collection.name() ) );
  startTask(new ChangeCollectionTask( createResourceState(TaskArguments(collection, parts)), this ));
}

void ImapResource::collectionRemoved( const Collection &collection )
{
  //TODO Move this to the task
  const QString mailBox = ResourceStateInterface::mailBoxForCollection( collection, false );
  if ( mailBox.isEmpty() ) {
    // this collection will be removed soon by its parent collection
    changeProcessed();
    return;
  }
  emit status( AgentBase::Running, i18nc( "@info:status", "Removing folder '%1'", collection.name() ) );

  startTask(new RemoveCollectionRecursiveTask( createResourceState(TaskArguments(collection)), this ));
}

void ImapResource::collectionMoved( const Akonadi::Collection &collection, const Akonadi::Collection &source,
                                    const Akonadi::Collection &destination )
{
  emit status( AgentBase::Running, i18nc( "@info:status", "Moving folder '%1' from '%2' to '%3'",
        collection.name(), source.name(), destination.name() ) );
  startTask(new MoveCollectionTask( createResourceState(TaskArguments(collection, source, destination)), this ));
}



void ImapResource::addSearch(const QString& query, const QString& queryLanguage, const Collection& resultCollection)
{
}

void ImapResource::removeSearch(const Collection& resultCollection)
{
}

void ImapResource::search( const QString &query, const Collection &collection )
{
  QVariantMap arg;
  arg[QLatin1String("query")] = query;
  arg[QLatin1String("collection")] = QVariant::fromValue( collection );
  scheduleCustomTask( this, "doSearch", arg );
}

void ImapResource::doSearch( const QVariant &arg )
{
  const QVariantMap map = arg.toMap();
  const QString query = map[QLatin1String("query")].toString();
  const Collection collection = map[QLatin1String("collection")].value<Collection>();

  emit status( AgentBase::Running, i18nc( "@info:status", "Searching..." ) );
  startTask(new SearchTask( createResourceState(TaskArguments(collection)), query, this ));
}


// -----

// ----------------------------------------------------------------------------------

void ImapResource::scheduleConnectionAttempt()
{
  // block all other tasks, until we are connected
  scheduleCustomTask( this, "startConnect", QVariant(), ResourceBase::Prepend );
}

void ImapResource::doSetOnline(bool online)
{
#ifndef IMAPRESOURCE_NO_SOLID
  kDebug() << "online=" << online;
#endif
  if ( !online ) {
    Q_FOREACH(ResourceTask* task, m_taskList) {
      task->kill();
      delete task;
    }
    m_taskList.clear();
    m_pool->cancelPasswordRequests();
    if (m_pool->isConnected()) {
        m_pool->disconnect();
    }
    if (m_idle) {
      m_idle->stop();
      delete m_idle;
      m_idle = 0;
    }
    Settings::self()->clearCachedPassword();
  } else if ( online && !m_pool->isConnected() ) {
    scheduleConnectionAttempt();
  }
  ResourceBase::doSetOnline( online );
}

QChar ImapResource::separatorCharacter() const
{
    return m_separatorCharacter;
}

void ImapResource::setSeparatorCharacter( const QChar &separator )
{
    m_separatorCharacter = separator;
}

bool ImapResource::needsNetwork() const
{
  const QString hostName = Settings::self()->imapServer().section( QLatin1Char(':'), 0, 0 );
  // ### is there a better way to do this?
  if ( hostName == QLatin1String( "127.0.0.1" ) ||
       hostName == QLatin1String( "localhost" ) ||
       hostName == QHostInfo::localHostName() ) {
    return false;
  }
  return true;
}

void ImapResource::reconnect()
{
  setNeedsNetwork( needsNetwork() );
  setOnline( false ); // we are not connected initially
  setOnline( true );
}



// ----------------------------------------------------------------------------------

void ImapResource::startIdleIfNeeded()
{
  if ( !m_idle ) {
    startIdle();
  }
}

void ImapResource::startIdle()
{
  delete m_idle;
  m_idle = 0;

  if ( !m_pool->serverCapabilities().contains( QLatin1String("IDLE") ) )
    return;

  //Without password we don't even have to try
  if (Settings::self()->password().isEmpty()) {
    return;
  }

  const QStringList ridPath = Settings::self()->idleRidPath();
  if ( ridPath.size() < 2 )
    return;

  Collection c, p;
  p.setParentCollection( Collection::root() );
  for ( int i = ridPath.size() - 1; i > 0; --i ) {
    p.setRemoteId( ridPath.at( i ) );
    c.setParentCollection( p );
    p = c;
  }
  c.setRemoteId( ridPath.first() );

  Akonadi::CollectionFetchScope scope;
  scope.setResource( identifier() );
  scope.setAncestorRetrieval( Akonadi::CollectionFetchScope::All );

  Akonadi::CollectionFetchJob *fetch
    = new Akonadi::CollectionFetchJob( c, Akonadi::CollectionFetchJob::Base, this );
  fetch->setFetchScope( scope );

  connect( fetch, SIGNAL(result(KJob*)),
           this, SLOT(onIdleCollectionFetchDone(KJob*)) );
}

void ImapResource::onIdleCollectionFetchDone( KJob *job )
{
  if (job->error()) {
    kWarning() << "CollectionFetch for idling failed."
               << "error=" << job->error()
               << ", errorString=" << job->errorString();
    return;
  }
  Akonadi::CollectionFetchJob *fetch = static_cast<Akonadi::CollectionFetchJob*>(job);
  //Can be empty if collection is not subscribed locally
  if (!fetch->collections().isEmpty()) {
    m_idle = new ImapIdleManager( createResourceState(TaskArguments(fetch->collections().first())), m_pool, this );
  }
}



// ----------------------------------------------------------------------------------

void ImapResource::requestManualExpunge( qint64 collectionId )
{
  if ( !Settings::self()->automaticExpungeEnabled() ) {
    Collection collection( collectionId );

    Akonadi::CollectionFetchScope scope;
    scope.setResource( identifier() );
    scope.setAncestorRetrieval( Akonadi::CollectionFetchScope::All );

    Akonadi::CollectionFetchJob *fetch
      = new Akonadi::CollectionFetchJob( collection,
                                         Akonadi::CollectionFetchJob::Base,
                                         this );
    fetch->setFetchScope( scope );

    connect( fetch, SIGNAL(result(KJob*)),
             this, SLOT(onExpungeCollectionFetchDone(KJob*)) );
  }
}

void ImapResource::onExpungeCollectionFetchDone( KJob *job )
{
  if ( job->error() == 0 ) {
    Akonadi::CollectionFetchJob *fetch = static_cast<Akonadi::CollectionFetchJob*>( job );
    Akonadi::Collection collection = fetch->collections().first();

    scheduleCustomTask( this, "triggerCollectionExpunge",
                        QVariant::fromValue( collection ) );

  } else {
    kWarning() << "CollectionFetch for expunge failed."
               << "error=" << job->error()
               << ", errorString=" << job->errorString();
  }
}

void ImapResource::triggerCollectionExpunge( const QVariant &collectionVariant )
{
  const Collection collection = collectionVariant.value<Collection>();

  ExpungeCollectionTask *task = new ExpungeCollectionTask( createResourceState(TaskArguments(collection)), this );
  task->start( m_pool );
  queueTask( task );
}



// ----------------------------------------------------------------------------------

void ImapResource::abortActivity()
{
  if ( !m_taskList.isEmpty() ) {
    m_pool->disconnect( SessionPool::CloseSession );
    scheduleConnectionAttempt();
  }
}

void ImapResource::queueTask( ResourceTask *task )
{
  connect( task, SIGNAL(destroyed(QObject*)),
           this, SLOT(taskDestroyed(QObject*)) );
  m_taskList << task;
}

void ImapResource::startTask( ResourceTask* task )
{
  task->start(m_pool);
  queueTask(task);
}

void ImapResource::taskDestroyed( QObject *task )
{
  m_taskList.removeAll( static_cast<ResourceTask *>( task ) );
}


QStringList ImapResource::serverCapabilities() const
{
  return m_pool->serverCapabilities();
}

QString ImapResource::dumpResourceToString() const
{
  QString ret;
  Q_FOREACH(ResourceTask* task, m_taskList) {
    if (!ret.isEmpty())
      ret += QLatin1String(", ");
    ret += QLatin1String(task->metaObject()->className());
  }
  return QLatin1String("IMAP tasks: ") + ret;
}

void ImapResource::showError( const QString &message )
{
  emit status( Akonadi::AgentBase::Idle, message );
  m_statusMessageTimer->start( 1000*10 );
}

void ImapResource::clearStatusMessage()
{
  emit status( Akonadi::AgentBase::Idle, QString() );
}

void ImapResource::modifyCollection(const Collection &col)
{
    Akonadi::CollectionModifyJob *modJob = new Akonadi::CollectionModifyJob(col, this);
    connect(modJob, SIGNAL(result(KJob*)), this, SLOT(onCollectionModifyDone(KJob*)));
}

void ImapResource::onCollectionModifyDone(KJob* job)
{
    if (job->error()) {
        kWarning() << "Failed to modify collection: " << job->errorString();
    }
}

