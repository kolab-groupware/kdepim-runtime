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

#ifndef RETRIEVEITEMSTASK_H
#define RETRIEVEITEMSTASK_H

#include <kimap/fetchjob.h>

#include "resourcetask.h"

class BatchFetcher;
namespace Akonadi {
  class Session;
}

class RetrieveItemsTask : public ResourceTask
{
  Q_OBJECT

public:
  explicit RetrieveItemsTask( ResourceStateInterface::Ptr resource, QObject *parent = 0 );
  virtual ~RetrieveItemsTask();
  void setFetchMissingItemBodies(bool enabled);

public slots:
  void onFetchItemsWithoutBodiesDone( const QList<qint64> &items );
  void onReadyForNextBatch(int size);

private slots:
  void fetchItemsWithoutBodiesDone( KJob *job );
  void onPreExpungeSelectDone( KJob *job );
  void onExpungeDone( KJob *job );

  void onFinalSelectDone( KJob *job );

  void onItemsRetrieved(const Akonadi::Item::List &addedItems);
  void onRetrievalDone(KJob *job);

  void onFlagsReceived( const QString &mailBox, const QMap<qint64, qint64> &uids,
                        const QMap<qint64, qint64> &sizes,
                        const QMap<qint64, KIMAP::MessageFlags> &flags,
                        const QMap<qint64, KIMAP::MessagePtr> &messages );
  void onFlagsFetchDone( KJob *job );

protected:
  virtual void doStart( KIMAP::Session *session );

private:
  void startRetrievalTasks();
  void triggerPreExpungeSelect( const QString &mailBox );
  void triggerExpunge( const QString &mailBox );
  void triggerFinalSelect( const QString &mailBox );
  void retrieveItems(const KIMAP::ImapSet& set, const KIMAP::FetchJob::FetchScope &scope, qint64 oldHighestModSeq = 0, bool incremental = false);

  void listFlagsForImapSet( const KIMAP::ImapSet& set, qint64 highestmodseq );
  qint64 extractHighestModSeq( KJob *job ) const;

  KIMAP::Session *m_session;
  QList<qint64> m_messageUidsMissingBody;
  int m_fetchedMissingBodies;
  bool m_fetchMissingBodies;
  BatchFetcher *m_batchFetcher;
};

#endif
