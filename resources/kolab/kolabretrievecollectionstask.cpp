/*
    Copyright (c) 2010 Klarälvdalens Datakonsult AB,
                       a KDAB Group company <info@kdab.com>
    Author: Kevin Ottens <kevin@kdab.com>
    Copyright (c) 2014 Christian Mollekopf <mollekopf@kolabsys.com>

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

#include "kolabretrievecollectionstask.h"
#include "kolabhelpers.h"
#include "tracer.h"

#include <noselectattribute.h>
#include <noinferiorsattribute.h>
#include <collectionannotationsattribute.h>
#include <collectionmetadatahelper.h>
#include <imapaclattribute.h>
#include <kimap/getmetadatajob.h>
#include <kimap/myrightsjob.h>

#include <akonadi/cachepolicy.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/kmime/messageparts.h>
#include <akonadi/collectionidentificationattribute.h>
#include <akonadi/calendar/blockalarmsattribute.h>

#include <kmime/kmime_message.h>

#include <KDE/KDebug>
#include <KDE/KLocale>


bool isNamespaceFolder(const QString &path, const QList<KIMAP::MailBoxDescriptor> &namespaces, bool matchCompletePath = false)
{
    Q_FOREACH (const KIMAP::MailBoxDescriptor &desc, namespaces) {
        if (path.startsWith(desc.name.left(desc.name.size() - 1))) { //Namespace ends with path separator and pathPart doesn't
            if (!matchCompletePath || path.size() - desc.name.size() <= 1) {      //We want to match only for the complete path
                return true;
            }
        }
    }
    return false;
}

RetrieveMetadataJob::RetrieveMetadataJob(KIMAP::Session *session, const QStringList &mailboxes, const QStringList &serverCapabilities, const QSet<QByteArray> &requestedMetadata, const QString &separator, const QList<KIMAP::MailBoxDescriptor> &sharedNamespace, const QList<KIMAP::MailBoxDescriptor> &userNamespace, QObject *parent)
    : KJob(parent)
    , mJobs(0)
    , mRequestedMetadata(requestedMetadata)
    , mServerCapabilities(serverCapabilities)
    , mMailboxes(mailboxes)
    , mSession(session)
    , mSeparator(separator)
    , mSharedNamespace(sharedNamespace)
    , mUserNamespace(userNamespace)
{

}

void RetrieveMetadataJob::start()
{
    Trace();
    //Fill the map with empty entires so we set the mimetype to mail if no metadata is retrieved
    Q_FOREACH (const QString &mailbox, mMailboxes) {
        mMetadata.insert(mailbox, QMap<QByteArray, QByteArray>());
    }

    if ( mServerCapabilities.contains( QLatin1String("METADATA") ) || mServerCapabilities.contains( QLatin1String("ANNOTATEMORE") ) ) {
        QSet<QString> toplevelMailboxes;
        Q_FOREACH (const QString &mailbox, mMailboxes) {
            const QStringList parts = mailbox.split(mSeparator);
            if (!parts.isEmpty()) {
                if (isNamespaceFolder(mailbox, mUserNamespace) && parts.length() >= 2) {
                    // Other Users can be too big to request with a single command so we request Other Users/<user>/*
                    toplevelMailboxes << parts.at(0) + mSeparator + parts.at(1) + mSeparator;
                } else if (!isNamespaceFolder(mailbox, mSharedNamespace)) {
                    toplevelMailboxes << parts.first();
                }
            }
        }
        Q_FOREACH (const KIMAP::MailBoxDescriptor &desc, mSharedNamespace) {
            toplevelMailboxes << desc.name;
        }
        //TODO perhaps exclude the shared and other users namespaces by listing only toplevel (with %), and then only getting metadata of the toplevel folders.
        Q_FOREACH (const QString &mailbox, toplevelMailboxes) {
            {
                KIMAP::GetMetaDataJob *meta = new KIMAP::GetMetaDataJob(mSession);
                meta->setMailBox(mailbox + QLatin1String("*"));
                if ( mServerCapabilities.contains( QLatin1String("METADATA") ) ) {
                    meta->setServerCapability( KIMAP::MetaDataJobBase::Metadata );
                } else {
                    meta->setServerCapability( KIMAP::MetaDataJobBase::Annotatemore );
                }
                meta->setDepth( KIMAP::GetMetaDataJob::AllLevels );
                Q_FOREACH (const QByteArray &requestedEntry, mRequestedMetadata) {
                    meta->addRequestedEntry(requestedEntry);
                }
                connect( meta, SIGNAL(result(KJob*)), SLOT(onGetMetaDataDone(KJob*)) );
                mJobs++;
                meta->start();
            }
        }
    }


    // Get the ACLs from the mailbox if it's supported
    if ( mServerCapabilities.contains( QLatin1String("ACL") ) ) {

        Q_FOREACH (const QString &mailbox, mMailboxes) {
            // "Shared Folders" is not a valid mailbox, so we have to skip the ACL request for this folder
            if (isNamespaceFolder(mailbox, mSharedNamespace, true)) {
                continue;
            }
            KIMAP::MyRightsJob *rights = new KIMAP::MyRightsJob( mSession );
            rights->setMailBox(mailbox);
            connect( rights, SIGNAL(result(KJob*)), SLOT(onRightsReceived(KJob*)) );
            mJobs++;
            rights->start();
        }
    }
    checkDone();
}

void RetrieveMetadataJob::onGetMetaDataDone( KJob *job )
{
    mJobs--;
    KIMAP::GetMetaDataJob *meta = static_cast<KIMAP::GetMetaDataJob*>( job );
    if ( job->error() ) {
        kDebug() << "No metadata for for mailbox: " << meta->mailBox();
        if (!isNamespaceFolder(meta->mailBox(), mSharedNamespace)) {
            kWarning() << "Get metadata failed: " << job->errorString();
            //We ignore the error to avoid failing the complete sync. We can run into this when trying to retrieve rights for non-existing mailboxes.
        }
        checkDone();
        return;
    }

    const QHash<QString, QMap<QByteArray, QByteArray> > metadata = meta->allMetaDataForMailboxes();
    Q_FOREACH (const QString &folder, metadata.keys()) {
        mMetadata.insert(folder, metadata.value(folder));
    }
    checkDone();
}

void RetrieveMetadataJob::onRightsReceived( KJob *job )
{
    mJobs--;
    KIMAP::MyRightsJob *rights = static_cast<KIMAP::MyRightsJob*>(job);
    if ( job->error() ) {
        kDebug() << "No rights for mailbox: " << rights->mailBox();
        if (!isNamespaceFolder(rights->mailBox(), mSharedNamespace)) {
            kWarning() << "MyRights failed: " << job->errorString();
            //We ignore the error to avoid failing the complete sync. We can run into this when trying to retrieve rights for non-existing mailboxes.
        }
        checkDone();
        return;
    }

    const KIMAP::Acl::Rights imapRights = rights->rights();
    mRights.insert(rights->mailBox(), imapRights);
    checkDone();
}

void RetrieveMetadataJob::checkDone()
{
    if (!mJobs) {
        Trace() << "done";
        kDebug() << "done";
        emitResult();
    }
}

KolabRetrieveCollectionsTask::KolabRetrieveCollectionsTask(ResourceStateInterface::Ptr resource, QObject* parent)
    : ResourceTask(CancelIfNoSession, resource, parent)
    , mJobs(0)
    , cContentMimeTypes("CONTENTMIMETYPES")
    , cAccessRights("AccessRights")
    , cImapAcl("imapacl")
    , cCollectionAnnotations("collectionannotations")
    , cDefaultKeepLocalChanges(QSet<QByteArray>() << cContentMimeTypes << cAccessRights << cImapAcl << cCollectionAnnotations)
    , cDefaultMimeTypes(QStringList() << Akonadi::Collection::mimeType() << QLatin1String("application/x-kolab-objects"))
    , cCollectionOnlyContentMimeTypes(QStringList() << Akonadi::Collection::mimeType())
{
    mRequestedMetadata << "/shared/vendor/kolab/folder-type";
    mRequestedMetadata << "/private/vendor/kolab/folder-type";
}

KolabRetrieveCollectionsTask::~KolabRetrieveCollectionsTask()
{

}

void KolabRetrieveCollectionsTask::doStart(KIMAP::Session *session)
{
    Trace();
    kDebug() << "Starting collection retrieval";
    mTime.start();
    mSession = session;

    Akonadi::Collection root;
    root.setName(resourceName());
    root.setRemoteId(rootRemoteId());
    root.setContentMimeTypes(QStringList(Akonadi::Collection::mimeType()));
    root.setParentCollection(Akonadi::Collection::root());
    root.addAttribute(new NoSelectAttribute(true));
    root.attribute<Akonadi::EntityDisplayAttribute>(Akonadi::Collection::AddIfMissing)->setIconName(QLatin1String("kolab"));

    Akonadi::CachePolicy policy;
    policy.setInheritFromParent(false);
    policy.setSyncOnDemand(true);

    QStringList localParts;
    localParts << QLatin1String(Akonadi::MessagePart::Envelope)
                << QLatin1String(Akonadi::MessagePart::Header);
    int cacheTimeout = 60;

    if (isDisconnectedModeEnabled()) {
        // For disconnected mode we also cache the body
        // and we keep all data indifinitely
        localParts << QLatin1String(Akonadi::MessagePart::Body);
        cacheTimeout = -1;
    }

    policy.setLocalParts(localParts);
    policy.setCacheTimeout(cacheTimeout);
    policy.setIntervalCheckTime(intervalCheckTime());

    root.setCachePolicy(policy);

    mMailCollections.insert(QString(), root);

    Trace() << "subscription enabled: " << isSubscriptionEnabled();
    //jobs are serialized by the session
    if (isSubscriptionEnabled()) {
        KIMAP::ListJob *fullListJob = new KIMAP::ListJob(session);
        fullListJob->setOption(KIMAP::ListJob::NoOption);
        fullListJob->setQueriedNamespaces(serverNamespaces());
        connect( fullListJob, SIGNAL(mailBoxesReceived(QList<KIMAP::MailBoxDescriptor>,QList<QList<QByteArray> >)),
                this, SLOT(onFullMailBoxesReceived(QList<KIMAP::MailBoxDescriptor>,QList<QList<QByteArray> >)) );
        connect( fullListJob, SIGNAL(result(KJob*)), SLOT(onFullMailBoxesReceiveDone(KJob*)));
        mJobs++;
        fullListJob->start();
    }

    KIMAP::ListJob *listJob = new KIMAP::ListJob(session);
    listJob->setOption(KIMAP::ListJob::IncludeUnsubscribed);
    listJob->setQueriedNamespaces(serverNamespaces());
    connect(listJob, SIGNAL(mailBoxesReceived(QList<KIMAP::MailBoxDescriptor>,QList<QList<QByteArray> >)),
            this, SLOT(onMailBoxesReceived(QList<KIMAP::MailBoxDescriptor>,QList<QList<QByteArray> >)));
    connect(listJob, SIGNAL(result(KJob*)), SLOT(onMailBoxesReceiveDone(KJob*)));
    mJobs++;
    listJob->start();
}

void KolabRetrieveCollectionsTask::onMailBoxesReceived(const QList< KIMAP::MailBoxDescriptor > &descriptors,
                                                   const QList< QList<QByteArray> > &flags)
{
    for (int i=0; i<descriptors.size(); ++i) {
        const KIMAP::MailBoxDescriptor descriptor = descriptors[i];
        createCollection(descriptor.name, flags.at(i), !isSubscriptionEnabled() || mSubscribedMailboxes.contains(descriptor.name));
    }
    checkDone();
}

Akonadi::Collection KolabRetrieveCollectionsTask::getOrCreateParent(const QString &path)
{
    if (mMailCollections.contains(path)) {
        return mMailCollections.value(path);
    }
    //create a dummy collection
    const QString separator = separatorCharacter();
    const QStringList pathParts = path.split(separator);
    const QString pathPart = pathParts.last();
    Akonadi::Collection c;
    c.setName( pathPart );
    c.setRemoteId( separator + pathPart );
    const QStringList parentPath = pathParts.mid(0, pathParts.size() - 1);
    const Akonadi::Collection parentCollection = getOrCreateParent(parentPath.join(separator));
    c.setParentCollection(parentCollection);

    c.addAttribute(new NoSelectAttribute(true));
    c.setContentMimeTypes(QStringList() << Akonadi::Collection::mimeType());
    c.setRights(Akonadi::Collection::ReadOnly);
    c.setEnabled(false);
    setAttributes(c, pathParts, path);

    mMailCollections.insert(path, c);
    return c;
}

void KolabRetrieveCollectionsTask::setAttributes(Akonadi::Collection &c, const QStringList &pathParts, const QString &path)
{

    CollectionIdentificationAttribute *attr = c.attribute<CollectionIdentificationAttribute>(Akonadi::Collection::AddIfMissing);
    attr->setIdentifier(path.toLatin1());

    // If the folder is a other users folder block all alarms from default
    if (isNamespaceFolder(path, resourceState()->userNamespaces())) {
        Akonadi::BlockAlarmsAttribute *attr = c.attribute<Akonadi::BlockAlarmsAttribute>(Akonadi::Collection::AddIfMissing);
        attr->blockEverything(true);
    }

    // If the folder is a other users top-level folder mark it accordingly
    if (pathParts.size() == 1 && isNamespaceFolder(path, resourceState()->userNamespaces())) {
        Akonadi::EntityDisplayAttribute *attr = c.attribute<Akonadi::EntityDisplayAttribute>(Akonadi::Collection::AddIfMissing);
        attr->setDisplayName(i18n("Other Users"));
        attr->setIconName(QLatin1String("x-mail-distribution-list"));
    }

    //Mark user folders for searching
    if (pathParts.size() >= 2 && isNamespaceFolder(path, resourceState()->userNamespaces())) {
        CollectionIdentificationAttribute *attr = c.attribute<CollectionIdentificationAttribute>(Akonadi::Collection::AddIfMissing);
        if (pathParts.size() == 2) {
            attr->setCollectionNamespace("usertoplevel");
        } else {
            attr->setCollectionNamespace("user");
        }
    }

    // If the folder is a shared folders top-level folder mark it accordingly
    if (pathParts.size() == 1 && isNamespaceFolder(path, resourceState()->sharedNamespaces())) {
        Akonadi::EntityDisplayAttribute *attr = c.attribute<Akonadi::EntityDisplayAttribute>(Akonadi::Collection::AddIfMissing);
        attr->setDisplayName(i18n("Shared Folders"));
        attr->setIconName(QLatin1String("x-mail-distribution-list"));
    }

    //Mark shared folders for searching
    if (pathParts.size() >= 2 && isNamespaceFolder(path, resourceState()->sharedNamespaces())) {
        CollectionIdentificationAttribute *attr = c.attribute<CollectionIdentificationAttribute>(Akonadi::Collection::AddIfMissing);
        attr->setCollectionNamespace("shared");
    }

}

void KolabRetrieveCollectionsTask::createCollection(const QString &mailbox, const QList<QByteArray> &currentFlags, bool isSubscribed)
{
    const QString separator = separatorCharacter();
    Q_ASSERT(separator.size() == 1);
    const QString boxName = mailbox.endsWith( separator )
                          ? mailbox.left( mailbox.size()-1 )
                          : mailbox;
    const QStringList pathParts = boxName.split( separator );
    const QString pathPart = pathParts.last();

    Akonadi::Collection c;
    //If we had a dummy collection we need to replace it
    if (mMailCollections.contains(mailbox)) {
        c = mMailCollections.value(mailbox);
    }
    c.setName( pathPart );
    c.setRemoteId( separator + pathPart );
    const QStringList parentPath = pathParts.mid(0, pathParts.size() - 1);
    const Akonadi::Collection parentCollection = getOrCreateParent(parentPath.join(separator));
    c.setParentCollection(parentCollection);
    //TODO get from ResourceState, and add KMime::Message::mimeType() for the normal imap resource by default
    //We add a dummy mimetype, otherwise the itemsync doesn't even work (action is disabled and resourcebase aborts the operation)
    c.setContentMimeTypes(cDefaultMimeTypes);
    c.setKeepLocalChanges(cDefaultKeepLocalChanges);

    //assume LRS, until myrights is executed
    if (serverCapabilities().contains(QLatin1String("ACL"))) {
        c.setRights(Akonadi::Collection::ReadOnly);
    } else {
        c.setRights(Akonadi::Collection::AllRights);
    }

    setAttributes(c, pathParts, mailbox);

    // If the folder is the Inbox, make some special settings.
    if (pathParts.size() == 1 && pathPart.compare(QLatin1String("inbox") , Qt::CaseInsensitive) == 0) {
        Akonadi::EntityDisplayAttribute *attr = c.attribute<Akonadi::EntityDisplayAttribute>(Akonadi::Collection::AddIfMissing);
        attr->setDisplayName(i18n("Inbox"));
        attr->setIconName(QLatin1String("mail-folder-inbox"));
        setIdleCollection(c);
    }

    // If this folder is a noselect folder, make some special settings.
    if (currentFlags.contains("\\noselect")) {
        c.addAttribute(new NoSelectAttribute(true));
        c.setContentMimeTypes(cCollectionOnlyContentMimeTypes);
        c.setRights( Akonadi::Collection::ReadOnly );
    } else {
        // remove the noselect attribute explicitly, in case we had set it before (eg. for non-subscribed non-leaf folders)
        c.removeAttribute<NoSelectAttribute>();
    }

    // If this folder is a noinferiors folder, it is not allowed to create subfolders inside.
    if (currentFlags.contains("\\noinferiors")) {
        //kDebug() << "Noinferiors: " << currentPath;
        c.addAttribute(new NoInferiorsAttribute(true));
        c.setRights(c.rights() & ~Akonadi::Collection::CanCreateCollection);
    }
    c.setEnabled(isSubscribed);

    // kDebug() << "creating collection " << mailbox << " with parent " << parentPath;
    mMailCollections.insert(mailbox, c);
}

void KolabRetrieveCollectionsTask::onMailBoxesReceiveDone(KJob* job)
{
    Trace();
    kDebug() << "All mailboxes received: " << mTime.elapsed();
    kDebug() << "in total: " << mMailCollections.size();
    mJobs--;
    if (job->error()) {
        kWarning() << QLatin1String("Failed to retrieve mailboxes: ") + job->errorString();
        cancelTask("Collection retrieval failed");
    } else {
        QSet<QString> mailboxes;
        Q_FOREACH(const QString &mailbox, mMailCollections.keys()) {
            if (!mailbox.isEmpty() && !isNamespaceFolder(mailbox, resourceState()->userNamespaces() + resourceState()->sharedNamespaces())) {
                mailboxes << mailbox;
            }
        }

        //Only request metadata for subscribed Other Users Folders
        const QStringList metadataMailboxes = mailboxes.unite( mSubscribedMailboxes.toList().toSet()).toList();

        RetrieveMetadataJob *metadata = new RetrieveMetadataJob(mSession, metadataMailboxes, serverCapabilities(), mRequestedMetadata, separatorCharacter(), resourceState()->sharedNamespaces(), resourceState()->userNamespaces(), this);
        connect(metadata, SIGNAL(result(KJob*)), this, SLOT(onMetadataRetrieved(KJob*)));
        mJobs++;
        metadata->start();
    }
}

void KolabRetrieveCollectionsTask::applyRights(QHash<QString, KIMAP::Acl::Rights> rights)
{
    // kDebug() << rights;
    Q_FOREACH(const QString &mailbox, rights.keys()) {
        if (mMailCollections.contains(mailbox)) {
            const KIMAP::Acl::Rights imapRights = rights.value(mailbox);
            QStringList parts = mailbox.split(separatorCharacter());
            parts.removeLast();
            QString parentMailbox = parts.join(separatorCharacter());

            KIMAP::Acl::Rights parentImapRights;
            //If the parent folder is not existing we cant rename
            if (!parentMailbox.isEmpty() && rights.contains(parentMailbox)) {
                parentImapRights = rights.value(parentMailbox);
            }
            // kDebug() << mailbox << parentMailbox << imapRights << parentImapRights;

            Akonadi::Collection &collection = mMailCollections[mailbox];
            CollectionMetadataHelper::applyRights(collection, imapRights, parentImapRights);

            // Store the mailbox ACLs
            Akonadi::ImapAclAttribute *aclAttribute = collection.attribute<Akonadi::ImapAclAttribute>( Akonadi::Collection::AddIfMissing );
            const KIMAP::Acl::Rights oldRights = aclAttribute->myRights();
            if ( oldRights != imapRights ) {
                aclAttribute->setMyRights( imapRights );
            }
        } else {
            kWarning() << "Can't find mailbox " << mailbox;
        }
    }
}

void KolabRetrieveCollectionsTask::applyMetadata(QHash<QString, QMap<QByteArray, QByteArray> > metadataMap)
{
    // kDebug() << metadataMap;
    Q_FOREACH(const QString &mailbox, metadataMap.keys()) {
        const QMap<QByteArray, QByteArray> metadata  = metadataMap.value(mailbox);
        if (mMailCollections.contains(mailbox)) {
            Akonadi::Collection &collection = mMailCollections[mailbox];
            // kDebug() << "setting metadata: " << mailbox << metadata;
            collection.attribute<Akonadi::CollectionAnnotationsAttribute>(Akonadi::Collection::AddIfMissing)->setAnnotations(metadata);
            const QByteArray type = KolabHelpers::getFolderTypeAnnotation(metadata);
            const Kolab::FolderType folderType = KolabHelpers::folderTypeFromString(type);
            collection.setContentMimeTypes(KolabHelpers::getContentMimeTypes(folderType));
            QSet<QByteArray> keepLocalChanges = collection.keepLocalChanges();
            keepLocalChanges.remove(cContentMimeTypes);
            collection.setKeepLocalChanges(keepLocalChanges);
        }
    }
}

void KolabRetrieveCollectionsTask::onMetadataRetrieved(KJob *job)
{
    Trace();
    kDebug() << mTime.elapsed();
    mJobs--;
    if (job->error()) {
        kWarning() << "Error while retrieving metadata, aborting collection retrieval: " << job->errorString();
        cancelTask("Collection retrieval failed");
    } else {
        RetrieveMetadataJob *metadata = static_cast<RetrieveMetadataJob*>(job);
        applyRights(metadata->mRights);
        applyMetadata(metadata->mMetadata);
        checkDone();
    }
}

void KolabRetrieveCollectionsTask::checkDone()
{
    if (!mJobs) {
        Trace() << "done " << mMailCollections.size();
        collectionsRetrieved(mMailCollections.values());
        kDebug() << "done " <<  mTime.elapsed();
    }
}

void KolabRetrieveCollectionsTask::onFullMailBoxesReceived(const QList< KIMAP::MailBoxDescriptor >& descriptors,
                                                       const QList< QList< QByteArray > >& flags)
{
    Q_UNUSED(flags);
    foreach (const KIMAP::MailBoxDescriptor &descriptor, descriptors) {
        mSubscribedMailboxes.insert(descriptor.name);
    }
}

void KolabRetrieveCollectionsTask::onFullMailBoxesReceiveDone(KJob* job)
{
    Trace();
    kDebug() << "received subscribed collections " <<  mTime.elapsed();
    mJobs--;
    if (job->error()) {
        kWarning() << QLatin1String("Failed to retrieve subscribed collections: ") + job->errorString();
        cancelTask("Collection retrieval failed");
    } else {
        checkDone();
    }
}
