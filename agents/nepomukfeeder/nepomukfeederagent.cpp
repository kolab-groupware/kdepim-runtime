/*
    Copyright (c) 2007 Tobias Koenig <tokoe@kde.org>
                  2008 Sebastian Trueg <trueg@kde.org>
                  2009 Volker Krause <vkrause@kde.org>
                  2011 Christian Mollekopf <chrigi_1@fastmail.fm>

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

#include "nepomukfeederagent.h"
#include <aneo.h>

#include <akonadi/changerecorder.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/entityhiddenattribute.h>
#include <akonadi/indexpolicyattribute.h>
#include <akonadi/item.h>
#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/entityhiddenattribute.h>

#include <dms-copy/simpleresource.h>
#include <dms-copy/simpleresourcegraph.h>
#include <dms-copy/datamanagement.h>
#include <nepomuk/resourcemanager.h>

#include <KLocale>
#include <KUrl>
#include <KProcess>
#include <KStandardDirs>
#include <KIdleTime>
#include <KConfigGroup>

#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/RDF>

#include <QtCore/QTimer>
#include <nepomukfeederutils.h>
#include "pluginloader.h"
#include "nepomukhelpers.h"

namespace Akonadi {

static inline bool indexingDisabled( const Collection &collection )
{
  if ( collection.hasAttribute<EntityHiddenAttribute>() )
    return true;

  IndexPolicyAttribute *indexPolicy = collection.attribute<IndexPolicyAttribute>();
  if ( indexPolicy && !indexPolicy->indexingEnabled() )
    return true;

  return false;
}

NepomukFeederAgent::NepomukFeederAgent(const QString& id) :
  AgentBase(id),
  mIndexCompatLevel( 2 ),
  mNepomukStartupAttempted( false ),
  mInitialUpdateDone( false ),
  mSelfTestPassed( false ),
  mSystemIsIdle( false ),
  mIdleDetectionDisabled( false )
{
  KGlobal::locale()->insertCatalog( "akonadi_nepomukfeeder" ); //TODO do we really need this?

  // initialize Nepomuk
  Nepomuk::ResourceManager::instance()->init();

  changeRecorder()->fetchCollection( true );
  changeRecorder()->itemFetchScope().setAncestorRetrieval( ItemFetchScope::Parent );
  changeRecorder()->setAllMonitored(true);
  changeRecorder()->itemFetchScope().fetchFullPayload();
  changeRecorder()->itemFetchScope().setCacheOnly( true );

  disableIdleDetection( false ); //Could be removed if not needed anymore

  mNepomukStartupTimeout.setInterval( 300 * 1000 );
  mNepomukStartupTimeout.setSingleShot( true );
  connect( &mNepomukStartupTimeout, SIGNAL(timeout()), SLOT(selfTest()) );
  connect( Nepomuk::ResourceManager::instance(), SIGNAL(nepomukSystemStarted()), SLOT(selfTest()) );
  connect( Nepomuk::ResourceManager::instance(), SIGNAL(nepomukSystemStopped()), SLOT(selfTest()) );
  connect( this, SIGNAL(reloadConfiguration()), SLOT(selfTest()) );
  connect( this, SIGNAL(fullyIndexed()), this, SLOT(slotFullyIndexed()) );

  connect( KIdleTime::instance(), SIGNAL(timeoutReached(int)), SLOT(systemIdle()) );
  connect( KIdleTime::instance(), SIGNAL(resumingFromIdle()), SLOT(systemResumed()) );
  KIdleTime::instance()->addIdleTimeout( 10 * 1000 );

  checkOnline();
  QTimer::singleShot( 0, this, SLOT(selfTest()) );

  mQueue.setItemFetchScope(changeRecorder()->itemFetchScope());

  connect(&mQueue, SIGNAL(progress(int)), SIGNAL(percent(int)));
  connect(&mQueue, SIGNAL(idle(QString)), this, SLOT(idle(QString)));
  connect(&mQueue, SIGNAL(running(QString)), this, SLOT(running(QString)));
  connect(&mQueue, SIGNAL(fullyIndexed()), this, SIGNAL(fullyIndexed()));
}

NepomukFeederAgent::~NepomukFeederAgent()
{

}

void NepomukFeederAgent::itemAdded(const Akonadi::Item& item, const Akonadi::Collection& collection)
{
  //kDebug() << item.id();
  if ( indexingDisabled( collection ) )
    return;

  Q_ASSERT( item.parentCollection() == collection);
  mQueue.addItem( item );
}

void NepomukFeederAgent::itemChanged(const Akonadi::Item& item, const QSet< QByteArray >& partIdentifiers)
{
  //kDebug() << item.id();
  Q_UNUSED( partIdentifiers );
  if ( indexingDisabled( item.parentCollection() ) )
    return;
  // TODO: check part identfiers if anything interesting changed at all
  mQueue.addItem( item );
}

void NepomukFeederAgent::itemRemoved(const Akonadi::Item& item)
{
  //kDebug() << item.url();
  Nepomuk::removeResources(QList <QUrl>() << item.url());
}

void NepomukFeederAgent::collectionAdded(const Akonadi::Collection& collection, const Akonadi::Collection& parent)
{
  Q_UNUSED( parent );
  if ( indexingDisabled( collection ) )
    return;
  NepomukHelpers::addCollectionToNepomuk(collection);
}

void NepomukFeederAgent::collectionChanged(const Akonadi::Collection& collection, const QSet< QByteArray >& partIdentifiers)
{
  Q_UNUSED( partIdentifiers );
  if ( indexingDisabled( collection ) )
    return;
  NepomukHelpers::addCollectionToNepomuk(collection);
}

void NepomukFeederAgent::collectionRemoved(const Akonadi::Collection& collection)
{
  Nepomuk::removeResources(QList <QUrl>() << collection.url());
}

void NepomukFeederAgent::updateAll()
{
  mQueue.setReindexing(true);
  CollectionFetchJob *collectionFetch = new CollectionFetchJob( Collection::root(), CollectionFetchJob::Recursive, this );
  connect( collectionFetch, SIGNAL(collectionsReceived(Akonadi::Collection::List)), SLOT(collectionsReceived(Akonadi::Collection::List)) );
}

void NepomukFeederAgent::collectionsReceived(const Akonadi::Collection::List& collections)
{
  foreach( const Collection &collection, collections ) {
    if ( indexingDisabled( collection ) )
      continue;
    mQueue.addCollection( collection );
  }
}

void NepomukFeederAgent::selfTest()
{
  QStringList errorMessages;
  mSelfTestPassed = false;

  // check if we have been disabled explicitly
  {
    KConfig config( "akonadi_nepomuk_feederrc" );
    KConfigGroup cfgGrp( &config, identifier() );
    if ( !cfgGrp.readEntry( "Enabled", true ) ) {
      checkOnline();
      emit status( Broken, i18n( "Indexing has been disabled by you." ) );
      return;
    }
  }

  // if Nepomuk is not running, try to start it
  if ( !mNepomukStartupAttempted && !Nepomuk::ResourceManager::instance()->initialized() ) {
    KProcess process;
    const QString nepomukserver = KStandardDirs::findExe( QLatin1String("nepomukserver") );
    if ( process.startDetached( nepomukserver ) == 0 ) {
      errorMessages.append( i18n( "Unable to start the Nepomuk server." ) );
    } else {
      mNepomukStartupAttempted = true;
      mNepomukStartupTimeout.start();
      // wait for Nepomuk to start
      checkOnline();
      emit status( Broken, i18n( "Waiting for the Nepomuk server to start..." ) );
      return;
    }
  }

  if ( !Nepomuk::ResourceManager::instance()->initialized() ) {
    if ( mNepomukStartupAttempted && mNepomukStartupTimeout.isActive() ) {
      // still waiting for Nepomuk to start
      setOnline( false );
      emit status( Broken, i18n( "Waiting for the Nepomuk server to start..." ) );
      return;
    } else {
      errorMessages.append( i18n( "Nepomuk is not running." ) );
    }
  }

  if ( errorMessages.isEmpty() ) {
    mSelfTestPassed = true;
    mNepomukStartupAttempted = false; // everything worked, we can try again if the server goes down later
    mNepomukStartupTimeout.stop();
    checkOnline();
    if ( !mInitialUpdateDone && needsReIndexing() ) {
      mInitialUpdateDone = true;
      QTimer::singleShot( 0, this, SLOT(updateAll()) );
    } else {
      emit status( Idle, i18n( "Ready to index data." ) );
    }
    return;
  }

  checkOnline();
  emit status( Broken, i18n( "Nepomuk is not operational: %1", errorMessages.join( " " ) ) );
}

void NepomukFeederAgent::disableIdleDetection( bool value )
{
  mIdleDetectionDisabled = value;
}

bool NepomukFeederAgent::needsReIndexing() const
{
  const KConfigGroup grp( componentData().config(), "InitialIndexing" );
  return mIndexCompatLevel > grp.readEntry( "IndexCompatLevel", 0 );
}

void NepomukFeederAgent::slotFullyIndexed()
{
  KConfigGroup grp( componentData().config(), "InitialIndexing" );
  grp.writeEntry( "IndexCompatLevel", mIndexCompatLevel );
  grp.sync();
}

void NepomukFeederAgent::doSetOnline(bool online)
{
  changeRecorder()->setChangeRecordingEnabled( !online );
  Akonadi::AgentBase::doSetOnline( online );
}

void NepomukFeederAgent::checkOnline()
{
  if ( mIdleDetectionDisabled )
    setOnline( mSelfTestPassed );
  else
    setOnline( mSelfTestPassed && mSystemIsIdle );

  mQueue.setOnline(isOnline());
  if ( isOnline() && !mQueue.isEmpty() ) {
    if ( mQueue.currentCollection().isValid() )
      emit status( AgentBase::Running, i18n( "Indexing collection '%1'...", mQueue.currentCollection().name() ) );
    else
      emit status( AgentBase::Running, i18n( "Indexing recent changes..." ) );
  }
}

void NepomukFeederAgent::systemIdle()
{
  if ( mIdleDetectionDisabled )
    return;

  emit status( Idle, i18n( "System idle, ready to index data." ) );
  mSystemIsIdle = true;
  KIdleTime::instance()->catchNextResumeEvent();
  checkOnline();
}

void NepomukFeederAgent::systemResumed()
{
  if ( mIdleDetectionDisabled )
    return;

  emit status( Idle, i18n( "System busy, indexing suspended." ) );
  mSystemIsIdle = false;
  checkOnline();
}

void NepomukFeederAgent::idle(const QString &string) 
{
  emit status( AgentBase::Idle, string );
}

void NepomukFeederAgent::running(const QString &string) 
{
  emit status( AgentBase::Running, string );
}

}

AKONADI_AGENT_MAIN( Akonadi::NepomukFeederAgent )


#include "nepomukfeederagent.moc"