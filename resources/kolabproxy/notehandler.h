/*
  Copyright (c) 2009 Volker Krause <vkrause@kde.org>
  Copyright (c) 2012 Christian Mollekopf <mollekopf@kolabsys.com>

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

#ifndef KOLABPROXY_NOTEHANDLER_H
#define KOLABPROXY_NOTEHANDLER_H

#include "journalhandler.h"

class NotesHandler : public JournalHandler
{
  public:
    explicit NotesHandler( const Akonadi::Collection &imapCollection );

    virtual Akonadi::Item::List translateItems( const Akonadi::Item::List &kolabItems );
    virtual bool toKolabFormat( const Akonadi::Item &item, Akonadi::Item &imapItem );
    virtual QStringList contentMimeTypes();
    virtual QString iconName() const;

    virtual QString extractGid(const Akonadi::Item& kolabItem);

  private:
    bool noteFromKolab( const KMime::Message::Ptr &kolabMsg, Akonadi::Item &noteItem );
    void noteToKolab( const KMime::Message::Ptr &note, Akonadi::Item &kolabItem );

};

#endif
