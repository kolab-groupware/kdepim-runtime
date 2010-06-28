/*
    Copyright (c) 2010 Klar�lvdalens Datakonsult AB,
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

#include "resourcetask.h"

#include "sessionpool.h"
#include "resourcestateinterface.h"

ResourceTask::ResourceTask( ResourceStateInterface::Ptr resource, QObject *parent )
  : QObject( parent ),
    m_pool( 0 ),
    m_sessionRequestId( 0 ),
    m_session( 0 ),
    m_resource( resource )
{

}

ResourceTask::~ResourceTask()
{
  if ( m_pool && m_session ) {
    m_pool->releaseSession( m_session );
  }
}

void ResourceTask::start( SessionPool *pool )
{
  m_pool = pool;
  connect( m_pool, SIGNAL(sessionRequestDone(qint64, KIMAP::Session*, int, QString)),
           this, SLOT(onSessionRequested(qint64, KIMAP::Session*, int, QString)) );

  m_sessionRequestId = m_pool->requestSession();

  if ( m_sessionRequestId <= 0 ) {
    m_sessionRequestId = 0;
    deferTask();
  }
}

void ResourceTask::onSessionRequested( qint64 requestId, KIMAP::Session *session,
                                       int errorCode, const QString &/*errorString*/ )
{
  if ( requestId!=m_sessionRequestId ) {
    // Not for us, ignore
    return;
  }

  disconnect( m_pool, SIGNAL(sessionRequestDone(qint64, KIMAP::Session*, int, QString)),
              this, SLOT(onSessionRequested(qint64, KIMAP::Session*, int, QString)) );
  m_sessionRequestId = 0;

  if ( errorCode!=SessionPool::NoError ) {
    deferTask();
    return;
  }

  m_session = session;

  doStart( m_session );
}

Akonadi::Collection ResourceTask::collection() const
{
  return m_resource->collection();
}

Akonadi::Item ResourceTask::item() const
{
  return m_resource->item();
}

Akonadi::Collection ResourceTask::parentCollection() const
{
  return m_resource->parentCollection();
}

Akonadi::Collection ResourceTask::sourceCollection() const
{
  return m_resource->sourceCollection();
}

Akonadi::Collection ResourceTask::targetCollection() const
{
  return m_resource->targetCollection();
}

QSet<QByteArray> ResourceTask::parts() const
{
  return m_resource->parts();
}

QString ResourceTask::rootRemoteId() const
{
  return m_resource->rootRemoteId();
}

QString ResourceTask::mailBoxForCollection( const Akonadi::Collection &collection ) const
{
  return m_resource->mailBoxForCollection( collection );
}

void ResourceTask::itemRetrieved( const Akonadi::Item &item )
{
  m_resource->itemRetrieved( item );
  deleteLater();
}

void ResourceTask::cancelTask( const QString &errorString )
{
  m_resource->cancelTask( errorString );
  deleteLater();
}

void ResourceTask::deferTask()
{
  m_resource->deferTask();
  deleteLater();
}

#include "resourcetask.moc"

