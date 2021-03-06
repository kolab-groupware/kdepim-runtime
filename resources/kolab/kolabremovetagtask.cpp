/*
    Copyright (c) 2014 Klarälvdalens Datakonsult AB,
                       a KDAB Group company <info@kdab.com>
    Author: Kevin Krammer <kevin.krammer@kdab.com>

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

#include "kolabremovetagtask.h"

#include <imapflags.h>

#include <kimap/selectjob.h>
#include <kimap/session.h>
#include <kimap/storejob.h>
#include "tracer.h"

KolabRemoveTagTask::KolabRemoveTagTask(ResourceStateInterface::Ptr resource, QObject *parent)
    : KolabRelationResourceTask(resource, parent)
{
}

void KolabRemoveTagTask::startRelationTask(KIMAP::Session *session)
{
    // The imap specs do not allow for a single message to be deleted. We can only
    // set the \Deleted flag. The message will actually be deleted when EXPUNGE will
    // be issued on the next retrieveItems().

    const QString mailBox = mailBoxForCollection(relationCollection());

    Trace() << mailBox;
    kDebug(5327) << "Deleting tag " << resourceState()->tag().name() << " from " << mailBox;

    if (session->selectedMailBox() != mailBox) {
      KIMAP::SelectJob *select = new KIMAP::SelectJob(session);
      select->setMailBox(mailBox);

      connect(select, SIGNAL(result(KJob*)),
              this, SLOT(onSelectDone(KJob*)));

      select->start();

    } else {
      triggerStoreJob(session);
    }
}

void KolabRemoveTagTask::triggerStoreJob(KIMAP::Session *session)
{
    KIMAP::ImapSet set;
    set.add(resourceState()->tag().remoteId().toLong());
    Trace() << set.toImapSequenceSet();

    KIMAP::StoreJob *store = new KIMAP::StoreJob(session);
    store->setUidBased(true);
    store->setSequenceSet(set);
    store->setFlags(QList<QByteArray>() << ImapFlags::Deleted);
    store->setMode(KIMAP::StoreJob::AppendFlags);
    connect(store, SIGNAL(result(KJob*)), SLOT(onStoreFlagsDone(KJob*)));
    store->start();
}

void KolabRemoveTagTask::onSelectDone(KJob *job)
{
    if (job->error()) {
      kWarning() << "Failed to select mailbox: " << job->errorString();
      cancelTask(job->errorString());
    } else {
      KIMAP::SelectJob *select = static_cast<KIMAP::SelectJob*>(job);
      triggerStoreJob(select->session());
    }
}

void KolabRemoveTagTask::onStoreFlagsDone(KJob *job)
{
    Trace();
    //TODO use UID EXPUNGE if available
    if (job->error()) {
      kWarning() << "Failed to append flags: " << job->errorString();
      cancelTask(job->errorString());
    } else {
      changeProcessed();
    }
}
