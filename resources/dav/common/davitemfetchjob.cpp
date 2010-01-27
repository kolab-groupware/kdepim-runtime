/*
    Copyright (c) 2010 Tobias Koenig <tokoe@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include "davitemfetchjob.h"

#include "davmanager.h"

#include <kio/davjob.h>
#include <kio/job.h>
#include <klocale.h>

#include <QtCore/QDebug>

static QString etagFromHeaders( const QString &headers )
{
  const QStringList allHeaders = headers.split( "\n" );

  QString etag;
  foreach ( const QString &header, allHeaders ) {
    if ( header.startsWith( "etag:", Qt::CaseInsensitive ) )
      etag = header.section( ' ', 1 );
  }

  return etag;
}


DavItemFetchJob::DavItemFetchJob( const DavItem &item, QObject *parent )
  : KJob( parent ), mItem( item )
{
}

void DavItemFetchJob::start()
{
  KUrl url( mItem.url() );
  url.setUser( DavManager::self()->user() );
  url.setPassword( DavManager::self()->password() );

  KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::Reload, KIO::HideProgressInfo | KIO::DefaultFlags );
  job->addMetaData( "PropagateHttpHeader", "true" );

  connect( job, SIGNAL( result( KJob* ) ), this, SLOT( davJobFinished( KJob* ) ) );
}

DavItem DavItemFetchJob::item() const
{
  return mItem;
}

void DavItemFetchJob::davJobFinished( KJob *job )
{
  if ( job->error() ) {
    setError( job->error() );
    setErrorText( job->errorText() );
    emitResult();
    return;
  }

  KIO::StoredTransferJob *storedJob = qobject_cast<KIO::StoredTransferJob*>( job );
  mItem.setData( storedJob->data() );
  mItem.setContentType( storedJob->queryMetaData( "content-type" ) );
  mItem.setEtag( etagFromHeaders( storedJob->queryMetaData( "HTTP-Headers" ) ) );

  qDebug() << mItem.etag() << mItem.contentType() << mItem.data();

  emitResult();
}

#include "davitemfetchjob.moc"
