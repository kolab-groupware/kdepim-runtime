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
#include <KCompositeJob>

#include "jobcomposer.h"

AddCollectionTask::AddCollectionTask(ResourceStateInterface::Ptr resource, QObject *parent)
  : ResourceTask(DeferIfNoSession, resource, parent), m_session(0)
{
}

AddCollectionTask::~AddCollectionTask()
{
}

KJob *AddCollectionTask::setAnnotations(const QMap<QByteArray, QByteArray> &annotations)
{
    auto compositejob = new ParallelCompositeJob;

    foreach (const QByteArray &entry, annotations.keys()) { //krazy:exclude=foreach
        KIMAP::SetMetaDataJob *job = new KIMAP::SetMetaDataJob(m_session);
        if (serverCapabilities().contains(QLatin1String("METADATA"))) {
            job->setServerCapability(KIMAP::MetaDataJobBase::Metadata);
        } else {
            job->setServerCapability(KIMAP::MetaDataJobBase::Annotatemore);
        }
        job->setMailBox(mailBoxForCollection(m_collection));

        if (!entry.startsWith("/shared") && !entry.startsWith("/private")) {
            //Support for legacy annotations that don't include the prefix
            job->addMetaData(QByteArray("/shared") + entry, annotations[entry]);
        } else {
            job->addMetaData(entry, annotations[entry]);
        }
        compositejob->addSubjob(job);
    }
    return compositejob;
}

void AddCollectionTask::doStart(KIMAP::Session *session)
{
    if (parentCollection().remoteId().isEmpty()) {
        emitError(i18n("Cannot add IMAP folder '%1' for a non-existing parent folder '%2'.",
                        collection().name(),
                        parentCollection().name()));
        changeProcessed();
        return;
    }

    const QChar separator = separatorCharacter();
    m_session = session;
    m_collection = collection();
    m_collection.setName(m_collection.name().replace(separator, QString()));
    m_collection.setRemoteId(separator + m_collection.name());

    QString newMailBox = mailBoxForCollection(parentCollection());

    if (!newMailBox.isEmpty()) {
        newMailBox += separator;
    }

    newMailBox += m_collection.name();

    kDebug(5327) << "New folder: " << newMailBox;

    auto task = new JobComposer;
    task->add([&](JobComposer &t, KJob *job){
        KIMAP::CreateJob *createJob = new KIMAP::CreateJob(session);
        createJob->setMailBox(newMailBox);
        t.run(createJob, [this](JobComposer &t, KJob *job) {
            emitError(i18n("Failed to create the folder '%1' on the IMAP server. ",
                            m_collection.name()));
            cancelTask(job->errorString());
            return false;
        });
    });
    task->add([this](JobComposer &t, KJob *job){
        // Automatically subscribe to newly created mailbox
        KIMAP::CreateJob *create = static_cast<KIMAP::CreateJob*>(job);

        KIMAP::SubscribeJob *subscribe = new KIMAP::SubscribeJob(create->session());
        subscribe->setMailBox(create->mailBox());

        t.run(subscribe, [this](JobComposer &t, KJob *job) {
            if (isSubscriptionEnabled()) {
                emitWarning(i18n("Failed to subscribe to the folder '%1' on the IMAP server. "
                                "It will disappear on next sync. Use the subscription dialog to overcome that",
                                m_collection.name()));
            }
            return true;
        });
    });
    task->add([this](JobComposer &t, KJob *job){
        const Akonadi::CollectionAnnotationsAttribute *attribute = m_collection.attribute<Akonadi::CollectionAnnotationsAttribute>();
        if (!attribute || !serverSupportsAnnotations()) {
            // we are finished
            changeCommitted(m_collection);
            synchronizeCollectionTree();
            return;
        }

        t.run(setAnnotations(attribute->annotations()), [this](JobComposer &t, KJob *job) {
            if (job->error()) {
                emitWarning(i18n("Failed to subscribe to the folder '%1' on the IMAP server. "
                                "It will disappear on next sync. Use the subscription dialog to overcome that",
                                m_collection.name()));
            }
            return true;
        });
    });
    task->add([this](JobComposer &t, KJob *job){
        changeCommitted(m_collection);
    });
    task->start();
}

