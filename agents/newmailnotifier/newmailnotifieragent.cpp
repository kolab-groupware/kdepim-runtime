/*
    Copyright (c) 2013 Laurent Montel <montel@kde.org>

    Copyright (c) 2010 Volker Krause <vkrause@kde.org>

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

#include "newmailnotifieragent.h"

#include "util.h"

#include "newmailnotifierattribute.h"
#include "specialnotifierjob.h"
#include "newmailnotifieradaptor.h"
#include "newmailnotifieragentsettings.h"

#include <akonadi/dbusconnectionpool.h>

#include <akonadi/agentfactory.h>
#include <akonadi/changerecorder.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/entityhiddenattribute.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/session.h>
#include <Akonadi/AttributeFactory>
#include <Akonadi/CollectionFetchScope>
#include <akonadi/kmime/specialmailcollections.h>
#include <akonadi/kmime/messagestatus.h>
#include <Akonadi/AgentManager>
#include <KLocalizedString>
#include <KMime/Message>
#include <KNotification>
#include <KNotifyConfigWidget>
#include <KIconLoader>
#include <KIcon>
#include <KConfigGroup>
#include <KLocale>

using namespace Akonadi;

NewMailNotifierAgent::NewMailNotifierAgent( const QString &id )
    : AgentBase( id ),
      mNotifierEnabled(true),
      mVerboseNotification(true),
      mBeepOnNewMails(false)
{
    KGlobal::locale()->insertCatalog( "newmailnotifieragent" );
    Akonadi::AttributeFactory::registerAttribute<NewMailNotifierAttribute>();
    new NewMailNotifierAdaptor( this );

    DBusConnectionPool::threadConnection().registerObject( QLatin1String( "/NewMailNotifierAgent" ),
                                                           this, QDBusConnection::ExportAdaptors );
    DBusConnectionPool::threadConnection().registerService( QLatin1String( "org.freedesktop.Akonadi.NewMailNotifierAgent" ) );

    connect( Akonadi::AgentManager::self(), SIGNAL(instanceStatusChanged(Akonadi::AgentInstance)),
             this, SLOT(slotInstanceStatusChanged(Akonadi::AgentInstance)) );
    connect( Akonadi::AgentManager::self(), SIGNAL(instanceRemoved(Akonadi::AgentInstance)),
             this, SLOT(slotInstanceRemoved(Akonadi::AgentInstance)) );

    changeRecorder()->setMimeTypeMonitored( KMime::Message::mimeType() );
    changeRecorder()->itemFetchScope().setCacheOnly( true );
    changeRecorder()->itemFetchScope().setFetchModificationTime( false );
    changeRecorder()->fetchCollection( true );
    changeRecorder()->setChangeRecordingEnabled( false );
    changeRecorder()->setAllMonitored(true);
    changeRecorder()->ignoreSession( Akonadi::Session::defaultSession() );
    changeRecorder()->collectionFetchScope().setAncestorRetrieval( Akonadi::CollectionFetchScope::All );
    changeRecorder()->setCollectionMonitored(Collection::root(), true);
    mTimer.setInterval( 5 * 1000 );
    connect( &mTimer, SIGNAL(timeout()), SLOT(slotShowNotifications()) );

    mNotifierEnabled = NewMailNotifierAgentSettings::enabled();
    mVerboseNotification = NewMailNotifierAgentSettings::verboseNotification();
    mBeepOnNewMails = NewMailNotifierAgentSettings::beepOnNewMails();

    if (mNotifierEnabled) {
        mTimer.setSingleShot( true );
    }
    qDebug()<<" NewMailNotifierAgent::NewMailNotifierAgent:"<<id;
}


void NewMailNotifierAgent::setEnableNotifier(bool b)
{
    if (mNotifierEnabled != b) {
        mNotifierEnabled = b;
        NewMailNotifierAgentSettings::setEnabled(b);
        if (!mNotifierEnabled) {
            clearAll();
        }
    }
}

void NewMailNotifierAgent::setVerboseMailNotification(bool b)
{
    if (mVerboseNotification != b) {
        mVerboseNotification = b;
        NewMailNotifierAgentSettings::setVerboseNotification(b);
    }
}

bool NewMailNotifierAgent::verboseMailNotification() const
{
    return mVerboseNotification;
}

void NewMailNotifierAgent::setBeepOnNewMails(bool b)
{
    if (mBeepOnNewMails != b) {
        mBeepOnNewMails = b;
        NewMailNotifierAgentSettings::setBeepOnNewMails(b);
    }
}

bool NewMailNotifierAgent::beepOnNewMails() const
{
    return mBeepOnNewMails;
}


void NewMailNotifierAgent::clearAll()
{
    mNewMails.clear();
    mInstanceNameInProgress.clear();
}

bool NewMailNotifierAgent::enabledNotifier() const
{
    return mNotifierEnabled;
}

void NewMailNotifierAgent::configure( WId /*windowId*/ )
{
    KNotifyConfigWidget::configure( 0 );
}

bool NewMailNotifierAgent::excludeSpecialCollection(const Akonadi::Collection &collection) const
{
    if ( collection.hasAttribute<Akonadi::EntityHiddenAttribute>() )
        return true;

    if ( collection.hasAttribute<NewMailNotifierAttribute>() ) {
        if (collection.attribute<NewMailNotifierAttribute>()->ignoreNewMail()) {
            return true;
        }
    }

    SpecialMailCollections::Type type = SpecialMailCollections::self()->specialCollectionType(collection);
    switch(type) {
    case SpecialMailCollections::Invalid: //Not a special collection
    case SpecialMailCollections::Inbox:
        return false;
    default:
        return true;
    }
}

void NewMailNotifierAgent::itemMoved( const Akonadi::Item &item, const Akonadi::Collection &collectionSource, const Akonadi::Collection &collectionDestination )
{
    if (!mNotifierEnabled)
        return;

    Akonadi::MessageStatus status;
    status.setStatusFromFlags( item.flags() );
    if ( status.isRead() || status.isSpam() || status.isIgnored() )
        return;

    if ( excludeSpecialCollection(collectionSource) ) {
        return; // outbox, sent-mail, trash, drafts or templates.
    }

    if ( mNewMails.contains( collectionSource ) ) {
        QList<Akonadi::Item::Id> idListFrom = mNewMails[ collectionSource ];
        if ( idListFrom.contains( item.id() ) ) {
            idListFrom.removeAll( item.id() );
            mNewMails[ collectionSource ] = idListFrom;
            if ( mNewMails[collectionSource].isEmpty() )
                mNewMails.remove( collectionSource );
        }
        if ( !excludeSpecialCollection(collectionDestination) ) {
            QList<Akonadi::Item::Id> idListTo = mNewMails[ collectionDestination ];
            idListTo.append( item.id() );
            mNewMails[ collectionDestination ] = idListTo;
        }
    }

}

void NewMailNotifierAgent::itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection )
{
    if (!mNotifierEnabled)
        return;

    if ( excludeSpecialCollection(collection) ) {
        return; // outbox, sent-mail, trash, drafts or templates.
    }

    Akonadi::MessageStatus status;
    status.setStatusFromFlags( item.flags() );
    if ( status.isRead() || status.isSpam() || status.isIgnored() )
        return;

    if ( !mTimer.isActive() ) {
        mTimer.start();
    }

    mNewMails[ collection ].append( item.id() );
}

void NewMailNotifierAgent::slotShowNotifications()
{
    qDebug()<<"void NewMailNotifierAgent::slotShowNotifications()";
    if (mNewMails.isEmpty())
        return;

    qDebug()<<"NewMailNotifierAgent::slotShowNotifications mNotifierEnabled"<<mNotifierEnabled;
    if (!mNotifierEnabled)
        return;

    qDebug()<<" NewMailNotifierAgent::slotShowNotifications mInstanceNameInProgress: "<<mInstanceNameInProgress;
    if (!mInstanceNameInProgress.isEmpty()) {
        //Restart timer until all is done.
        mTimer.start();
        return;
    }

    QString message;
    if (mVerboseNotification) {
        int numberOfEmail = 0;
        Akonadi::Item::Id item = -1;
        QString currentPath;
        QStringList texts;
        QHash< Akonadi::Collection, QList<Akonadi::Item::Id> >::const_iterator end(mNewMails.constEnd());
        for ( QHash< Akonadi::Collection, QList<Akonadi::Item::Id> >::const_iterator it = mNewMails.constBegin(); it != end; ++it ) {
            Akonadi::EntityDisplayAttribute *attr = it.key().attribute<Akonadi::EntityDisplayAttribute>();
            QString displayName;
            if ( attr && !attr->displayName().isEmpty() )
                displayName = attr->displayName();
            else
                displayName = it.key().name();
            texts.append( i18np( "One new email in %2", "%1 new emails in %2", it.value().count(), displayName ) );
            ++numberOfEmail;
            if (numberOfEmail == 1) {
                item = it.value().first();
                currentPath = displayName;
            }
        }
        if (numberOfEmail == 1) {
            SpecialNotifierJob *job = new SpecialNotifierJob(currentPath, item, this);
            connect(job, SIGNAL(displayNotification(QPixmap,QString)), SLOT(slotDisplayNotification(QPixmap,QString)));
            mNewMails.clear();
            return;
        } else {
            message = texts.join( QLatin1String("<br>") );
        }
    } else {
        message = i18n( "New mail arrived" );
    }

    kDebug() << message;

    slotDisplayNotification(Util::defaultPixmap(), message);

    mNewMails.clear();
}


void NewMailNotifierAgent::slotDisplayNotification(const QPixmap &pixmap, const QString &message)
{
    Util::showNotification(pixmap, message);

    if ( mBeepOnNewMails ) {
        KNotification::beep();
    }
}

void NewMailNotifierAgent::slotInstanceStatusChanged(const Akonadi::AgentInstance &instance)
{
    if (!mNotifierEnabled)
        return;

    const QString identifier(instance.identifier());
    switch(instance.status()) {
    case Akonadi::AgentInstance::Broken:
    case Akonadi::AgentInstance::Idle:
    {
        if (mInstanceNameInProgress.contains(identifier)) {
            mInstanceNameInProgress.removeAll(identifier);
        }
        break;
    }
    case Akonadi::AgentInstance::Running:
    {
        if (!Util::excludeAgentType(instance)) {
            if (!mInstanceNameInProgress.contains(identifier)) {
                mInstanceNameInProgress.append(identifier);
            }
        }
        break;
    }
    case Akonadi::AgentInstance::NotConfigured:
        break;
    }
}

void NewMailNotifierAgent::slotInstanceRemoved(const Akonadi::AgentInstance &instance)
{
    if (!mNotifierEnabled)
        return;

    const QString identifier(instance.identifier());
    if (mInstanceNameInProgress.contains(identifier)) {
        mInstanceNameInProgress.removeAll(identifier);
    }
}

void NewMailNotifierAgent::printDebug()
{
    kDebug()<<"instance in progress: "<<mInstanceNameInProgress
            <<"\n notifier enabled : "<<mNotifierEnabled
            <<"\n check in progress : "<<!mInstanceNameInProgress.isEmpty()
            <<"\n beep on new mails: "<<mBeepOnNewMails;
}

AKONADI_AGENT_MAIN( NewMailNotifierAgent )


#include "newmailnotifieragent.moc"
