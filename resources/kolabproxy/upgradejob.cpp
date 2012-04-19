/*
 *  Copyright (c) 2012 Christian Mollekopf <mollekopf@kolabsys.com>
 * 
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 * 
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 * 
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */


#include "upgradejob.h"
#include "kolabdefs.h"
#include "kolabhandler.h"
#include <collectionannotationsattribute.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/itemcreatejob.h>
#include <akonadi/itemmodifyjob.h>
#include <klocalizedstring.h>

using namespace Akonadi;

#define IMAP_COLLECTION "IMAP_COLLECTION"
#define FOLDER_TYPE "FOLDER_TYPE"

UpgradeJob::UpgradeJob(Kolab::Version targetVersion, const Akonadi::AgentInstance& instance, QObject* parent)
    : Akonadi::Job(parent),
    m_targetVersion(targetVersion),
    m_agentInstance(instance)
{
    kDebug() << targetVersion;
}

void UpgradeJob::doStart()
{
    kDebug();
    //Get all subdirectories of kolab resource
    CollectionFetchJob *job = new CollectionFetchJob( Collection::root(), CollectionFetchJob::Recursive, this );
    job->fetchScope().setResource( m_agentInstance.identifier() );
    connect( job, SIGNAL(result(KJob*)), this, SLOT(collectionFetchResult(KJob*)) );
}

void UpgradeJob::collectionFetchResult(KJob* job)
{
    kDebug();
    if ( job->error() ) {
        kDebug() << job->errorString();
        return; // Akonadi::Job propagates that automatically
    }
    
    // We only want to upgrade our items, not the ones in shared folders.
    // So we search for our inbox first.
    Collection defaultParent;
    foreach ( const Collection &col, static_cast<CollectionFetchJob*>( job )->collections() ) {
        EntityDisplayAttribute* attr = 0;
        if ( (attr = col.attribute<EntityDisplayAttribute>()) ) {
            if ( attr->iconName() == QLatin1String( "mail-folder-inbox" ) ) {
                defaultParent = col;
            }
        }
    }
    
    if ( !defaultParent.isValid() ) {
        kWarning() << "No inbox found!";
        setError( Job::Unknown );
        setErrorText( i18n( "No inbox found!" ) );
        emitResult();
        return;
    }
        
    foreach ( const Collection &col, static_cast<CollectionFetchJob*>( job )->collections() ) {  
        if (col.parent() != defaultParent.id()) {
            kDebug() << "skipping toplevel folder";
            continue;
        }
        KolabV2::FolderType folderType = KolabV2::Mail;
        CollectionAnnotationsAttribute *attr = 0;
        if ( (attr = col.attribute<CollectionAnnotationsAttribute>()) ) {
            folderType = KolabV2::folderTypeFromString( attr->annotations().value( KOLAB_FOLDER_TYPE_ANNOTATION ) );
        }
        if (folderType == KolabV2::Mail) {
            kWarning() << "Wrong folder annotation (this should never happen, the annotation is probably not available)";
        }
        
        kDebug() << "upgrading " << col.id();
        
        ItemFetchJob *itemFetchJob = new ItemFetchJob(col, this);
        itemFetchJob->fetchScope().fetchFullPayload(true);
        itemFetchJob->fetchScope().setCacheOnly(false);
        itemFetchJob->setProperty( IMAP_COLLECTION, QVariant::fromValue( col ) );
        itemFetchJob->setProperty( FOLDER_TYPE, QVariant::fromValue( static_cast<int>(folderType) ) );
        connect( itemFetchJob, SIGNAL(result(KJob*)), this, SLOT(itemFetchResult(KJob*)) );
    }
}

void UpgradeJob::itemFetchResult(KJob* job)
{
    if ( job->error() ) {
        kDebug() << job->errorString();
        return; // Akonadi::Job propagates that automatically
    }
    ItemFetchJob *j = static_cast<ItemFetchJob*>( job );
    if (j->items().isEmpty()) {
        qWarning() << "no items fetched ";
        return;
    }
    
    const Collection imapCollection = j->property(IMAP_COLLECTION).value<Akonadi::Collection>();
    if (!imapCollection.isValid()) {
        qWarning() << "invalid imap collection";
        return;
    }
    
//     foreach(const Akonadi::Item &imapItem, j->items()) {
//         if (!imapItem.hasPayload<KMime::Message::Ptr>()) {
//             kWarning() << "Payload is not a MessagePtr!";
//             continue;
//         }
//         const KMime::Message::Ptr payload = imapItem.payload<KMime::Message::Ptr>();
//         KCalCore::Incidence::Ptr incidencePtr = Kolab::KolabObjectReader(payload).getIncidence();
//         if (!incidencePtr) {
//             kWarning() << "Failed to read incidence.";
//             continue;
//         }
//         
//         imapItem.setMimeType( "message/rfc822" );
//         const KMime::Message::Ptr &message = incidenceToMime(incidencePtr);
//         imapItem.setPayload(message);
//         new ItemModifyJob(imapItem, this);
//         
//     }
    
    KolabV2::FolderType folderType = static_cast<KolabV2::FolderType>(j->property(FOLDER_TYPE).toInt());
    KolabHandler::Ptr handler = KolabHandler::createHandler(folderType, imapCollection);
        
    if (!handler) {
        qWarning() << "invalid handler";
        return;
    }
    handler->setKolabFormatVersion(m_targetVersion);
    
    foreach(Akonadi::Item imapItem, j->items()) {
        if (!imapItem.isValid()) {
            qWarning() << "invalid item";
            continue;
        }
        kDebug() << "updating item " << imapItem.id();
        const Akonadi::Item::List &translatedItems = handler->translateItems(Akonadi::Item::List() << imapItem);
        if (translatedItems.size() != 1) {
            qWarning() << "failed to translateItems" << translatedItems.size();
            continue;
        }
        handler->toKolabFormat( translatedItems.first(), imapItem );
        ItemModifyJob *modJob = new ItemModifyJob(imapItem, this);
        connect(modJob, SIGNAL(result(KJob*)), this, SLOT(itemModifyResult(KJob*)) );
    }
}

void UpgradeJob::itemModifyResult(KJob* job)
{
    if ( job->error() ) {
        kDebug() << job->errorString();
        return; // Akonadi::Job propagates that automatically
    }
    kDebug() << "modjob done";
}
