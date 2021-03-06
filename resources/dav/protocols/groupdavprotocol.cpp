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

#include "groupdavprotocol.h"

#include "davutils.h"

#include <QtXml/QDomDocument>

GroupdavProtocol::GroupdavProtocol()
{
  QDomDocument document;

  QDomElement propfindElement = document.createElementNS( QLatin1String("DAV:"), QLatin1String("propfind") );
  document.appendChild( propfindElement );

  QDomElement propElement = document.createElementNS( QLatin1String("DAV:"), QLatin1String("prop") );
  propfindElement.appendChild( propElement );

  propElement.appendChild( document.createElementNS( QLatin1String("DAV:"), QLatin1String("displayname") ) );
  propElement.appendChild( document.createElementNS( QLatin1String("DAV:"), QLatin1String("resourcetype" )) );
  propElement.appendChild( document.createElementNS( QLatin1String("DAV:"), QLatin1String("getetag") ) );

  mItemsQueries << document;
}

bool GroupdavProtocol::supportsPrincipals() const
{
  return false;
}

bool GroupdavProtocol::useReport() const
{
  return false;
}

bool GroupdavProtocol::useMultiget() const
{
  return false;
}

QDomDocument GroupdavProtocol::collectionsQuery() const
{
  QDomDocument document;

  QDomElement propfindElement = document.createElementNS( QLatin1String("DAV:"), QLatin1String("propfind") );
  document.appendChild( propfindElement );

  QDomElement propElement = document.createElementNS( QLatin1String("DAV:"), QLatin1String("prop") );
  propfindElement.appendChild( propElement );

  propElement.appendChild( document.createElementNS( QLatin1String("DAV:"), QLatin1String("displayname") ) );
  propElement.appendChild( document.createElementNS( QLatin1String("DAV:"), QLatin1String("resourcetype") ) );

  return document;
}

QString GroupdavProtocol::collectionsXQuery() const
{
  const QString query( QLatin1String("//*[(local-name()='vevent-collection' or local-name()='vtodo-collection' or local-name()='vcard-collection') and namespace-uri()='http://groupdav.org/']/ancestor::*[local-name()='response' and namespace-uri()='DAV:']") );

  return query;
}

QList<QDomDocument> GroupdavProtocol::itemsQueries() const
{
  return mItemsQueries;
}

QString GroupdavProtocol::mimeTypeForQuery( int index ) const
{
  return QString();
}

DavCollection::ContentTypes GroupdavProtocol::collectionContentTypes( const QDomElement &propstatElement ) const
{
  /*
   * Extract the content type information from a propstat like the following
   *
   *  <propstat>
   *    <status>HTTP/1.1 200 OK</status>
   *    <prop>
   *      <displayname>Tasks</displayname>
   *      <resourcetype>
   *        <collection/>
   *        <G:vtodo-collection xmlns:G="http://groupdav.org/"/>
   *      </resourcetype>
   *      <getlastmodified>Sat, 30 Jan 2010 17:52:41 -0100</getlastmodified>
   *    </prop>
   *  </propstat>
   */

  const QDomElement propElement = DavUtils::firstChildElementNS( propstatElement, QLatin1String("DAV:"), QLatin1String("prop") );
  const QDomElement resourcetypeElement = DavUtils::firstChildElementNS( propElement, QLatin1String("DAV:"), QLatin1String("resourcetype") );

  DavCollection::ContentTypes contentTypes;

  if ( !DavUtils::firstChildElementNS( resourcetypeElement, QLatin1String("http://groupdav.org/"), QLatin1String("vevent-collection") ).isNull() )
    contentTypes |= DavCollection::Events;

  if ( !DavUtils::firstChildElementNS( resourcetypeElement, QLatin1String("http://groupdav.org/"), QLatin1String("vtodo-collection") ).isNull() )
    contentTypes |= DavCollection::Todos;

  if ( !DavUtils::firstChildElementNS( resourcetypeElement, QLatin1String("http://groupdav.org/"), QLatin1String("vcard-collection") ).isNull() )
    contentTypes |= DavCollection::Contacts;

  return contentTypes;
}

QString GroupdavProtocol::contactsMimeType() const
{
  return QLatin1String( "text/x-vcard" );
}
