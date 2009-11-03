/*
    Copyright (c) 2009 Grégory Oestreicher <greg@kamago.net>
    
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

#include <QDomDocument>

#include <kio/davjob.h>
#include <kio/jobclasses.h>
#include <kio/job.h>
#include <klocalizedstring.h>
#include <kdebug.h>

#include "groupdavcalendar.h"

groupdavCalendarAccessor::groupdavCalendarAccessor()
{
}

void groupdavCalendarAccessor::retrieveCollections( const KUrl &url )
{
  QString propfind =
      "<propfind xmlns=\"DAV:\">"
      "  <prop>"
      "    <displayname/>"
      "    <resourcetype/>"
      "  </prop>"
      "</propfind>";
  
  QDomDocument props;
  props.setContent( propfind );
  
  KIO::DavJob *job = doPropfind( url, props, "1" );
  connect( job, SIGNAL( result( KJob* ) ), this, SLOT( collectionsPropfindFinished( KJob* ) ) );
}

void groupdavCalendarAccessor::retrieveItems( const KUrl &url )
{
  QString propfind =
      "<propfind xmlns=\"DAV:\">"
      "  <prop>"
      "    <displayname/>"
      "    <resourcetype/>"
      "    <getetag/>"
      "  </prop>"
      "</propfind>";
  
  QDomDocument props;
  props.setContent( propfind );
  
  KIO::DavJob *job = doPropfind( url, props, "1" );
  connect( job, SIGNAL( result( KJob* ) ), this, SLOT( itemsPropfindFinished( KJob* ) ) );
}

void groupdavCalendarAccessor::retrieveItem( const KUrl &url )
{
  // TODO: implement this, if needed
}

void groupdavCalendarAccessor::getItem( const KUrl &url )
{
  KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::Reload, KIO::HideProgressInfo | KIO::DefaultFlags );
  job->addMetaData( "PropagateHttpHeader", "true" );
  connect( job, SIGNAL( result( KJob* ) ), this, SLOT( itemGetFinished( KJob* ) ) );
}

void groupdavCalendarAccessor::collectionsPropfindFinished( KJob *j )
{
  KIO::DavJob* job = dynamic_cast<KIO::DavJob*>( j );
  if( !job )
    return;
  
  if( job->error() ) {
    emit accessorError( job->errorString(), true );
    return;
  }
  
  QDomDocument xml = job->response();
  
  QDomElement root = xml.documentElement();
  if( !root.hasChildNodes() ) {
    emit accessorError( i18n( "No calendars found" ), true );
    return;
  }
  
  QDomNode n = root.firstChild();
  while( !n.isNull() ) {
    if( !n.isElement() ) {
      n = n.nextSibling();
      continue;
    }
    
    QDomElement r = n.toElement();
    n = n.nextSibling();
    QDomNodeList tmp;
    QDomElement propstat;
    QString href, status, displayname;
    KUrl url = job->url();
    
    tmp = r.elementsByTagNameNS( "DAV:", "href" );
    if( tmp.length() == 0 )
      continue;
    href = tmp.item( 0 ).firstChild().toText().data();
    if( href.startsWith( "/" ) )
      url.setEncodedPath( href.toAscii() );
    else
      url = href;
    
    tmp = r.elementsByTagNameNS( "DAV:", "propstat" );
    if( tmp.length() == 0 )
      continue;
    int nPropstat = tmp.length();
    for( int i = 0; i < nPropstat; ++i ) {
      QDomElement node = tmp.item( i ).toElement();
      QDomNode status = node.elementsByTagNameNS( "DAV:", "status" ).item( 0 );
      QString statusText = status.firstChild().toText().data();
      if( statusText.contains( "200" ) ) {
        propstat = node.toElement();
      }
    }
    if( propstat.isNull() )
      continue;
    
    tmp = propstat.elementsByTagNameNS( "http://groupdav.org/", "vevent-collection" );
    if( tmp.length() == 0 ) {
      tmp = propstat.elementsByTagNameNS( "http://groupdav.org/", "vtodo-collection" );
      if( tmp.length() == 0 )
        continue;
    }
    
    tmp = propstat.elementsByTagNameNS( "DAV:", "displayname" );
    if( tmp.length() != 0 )
      displayname = tmp.item( 0 ).firstChild().toText().data();
    else
      displayname = "GroupDAV calendar at " + url.url();
    
    emit( collectionRetrieved( url.url(), displayname ) );
  }
  
  emit collectionsRetrieved();
}

void groupdavCalendarAccessor::itemsPropfindFinished( KJob *j )
{
  KIO::DavJob* job = dynamic_cast<KIO::DavJob*>( j );
  if( !job )
    return;
  
  if( job->error() ) {
    emit accessorError( job->errorString(), true );
    return;
  }
  
  clearSeenUrls( job->url().url() );
  
  QDomDocument xml = job->response();
  QDomElement root = xml.documentElement();
  QDomNode n = root.firstChild();
  while( !n.isNull() ) {
    if( !n.isElement() ) {
      n = n.nextSibling();
      continue;
    }
    
    QDomElement r = n.toElement();
    n = n.nextSibling();
    QDomNodeList tmp;
    QDomElement propstat;
    QString href, status, etag;
    KUrl url = job->url();
    
    tmp = r.elementsByTagNameNS( "DAV:", "href" );
    if( tmp.length() == 0 )
      continue;
    href = tmp.item( 0 ).firstChild().toText().data();
    if( href.startsWith( "/" ) )
      url.setEncodedPath( href.toAscii() );
    else
      url = href;
    href = url.url();
    
    tmp = r.elementsByTagNameNS( "DAV:", "propstat" );
    if( tmp.length() == 0 )
      continue;
    propstat = tmp.item( 0 ).toElement();
    
    tmp = propstat.elementsByTagNameNS( "DAV:", "resourcetype" );
    if( tmp.length() != 0 ) {
      tmp = tmp.item( 0 ).toElement().elementsByTagNameNS( "DAV:", "collection" );
      if( tmp.length() != 0 )
        continue;
    }
    
    // NOTE: nothing below should invalidate the item (return an error
    // and exit the function)
    seenUrl( job->url().url(), href );
    
    tmp = propstat.elementsByTagNameNS( "DAV:", "getetag" );
    if( tmp.length() != 0 ) {
      etag = tmp.item( 0 ).firstChild().toText().data();
      
      davItemCacheStatus itemStatus = itemCacheStatus( href, etag );
      if( itemStatus == CACHED ) {
        emit itemRetrieved( getItemFromCache( href ) );
        continue;
      }
      else if( itemStatus == EXPIRED ) {
        backendChangedItems << href;
      }
    }
    
    fetchItemsQueue << href;
  }
  
  runItemsFetch();
}

void groupdavCalendarAccessor::itemGetFinished( KJob *j )
{
  KIO::StoredTransferJob *job = dynamic_cast<KIO::StoredTransferJob*>( j );
  if( !job ) {
    emit accessorError( i18n( "Problem with the retrieval job" ), true );
    return;
  }
  
  QByteArray d = job->data();
  QString url = job->url().url();
  QString mimeType = job->queryMetaData( "content-type" );
  QString etag = getEtagFromHeaders( job->queryMetaData( "HTTP-Headers" ) );
  
  if( etag.isEmpty() ) {
    emit accessorError( i18n( "The server returned an invalid answer (no etag header)" ), true );
    return;
  }
  
  kDebug() << "Got Item at URL " << url;
  davItem i( url, mimeType, d );
  addItemToCache( i, etag );
  
  if( backendChangedItems.contains( url ) ) {
    emit backendItemChanged( i );
    backendChangedItems.remove( url );
  }
  else {
    emit itemRetrieved( i );
  }
  
  runItemsFetch();
}

void groupdavCalendarAccessor::runItemsFetch()
{
  if( fetchItemsQueue.isEmpty() ) {
    emit itemsRetrieved();
    return;
  }
  
  QString next = fetchItemsQueue.takeFirst();
  getItem( next );
}
