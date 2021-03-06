/*
    Copyright (c) 2008-2009 Kevin Krammer <kevin.krammer@gmx.at>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef KCALRESOURCE
#define KCALRESOURCE

#include <akonadi/resourcebase.h>

namespace KRES {
  template <class T> class Manager;
}

namespace KCal {
  class AssignmentVisitor;
  class ResourceCalendar;

  typedef KRES::Manager<ResourceCalendar> CalendarResourceManager;
}

namespace Akonadi {
  class IncidenceMimeTypeVisitor;
}

class QTimer;

class KCalResource : public Akonadi::ResourceBase, public Akonadi::AgentBase::Observer
{
  Q_OBJECT

  public:
    KCalResource( const QString &id );
    virtual ~KCalResource();

  public Q_SLOTS:
    virtual void configure( WId windowId );

  protected Q_SLOTS:
    void retrieveCollections();
    void retrieveItems( const Akonadi::Collection &collection );
    bool retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts );

  protected:
    virtual void aboutToQuit();

    virtual void doSetOnline( bool online );

    virtual void itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection );
    virtual void itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts );
    virtual void itemRemoved( const Akonadi::Item &item );

    virtual void collectionAdded( const Akonadi::Collection &collection,
                                  const Akonadi::Collection &parent );

    virtual void collectionChanged( const Akonadi::Collection &collection );

    virtual void collectionRemoved( const Akonadi::Collection &collection );

  private:
    KCal::CalendarResourceManager *mManager;
    KCal::ResourceCalendar *mResource;
    Akonadi::IncidenceMimeTypeVisitor *mMimeVisitor;

    bool mFullItemRetrieve;

    QTimer *mDelayedSaveTimer;

    KCal::AssignmentVisitor *mIncidenceAssigner;

  private:
    bool openConfiguration();
    void closeConfiguration();

    bool saveCalendar();

    bool scheduleSaveCalendar();

    typedef KCal::ResourceCalendar ResourceCalendar;

  private Q_SLOTS:
    void reloadConfig();

    void initialLoadingFinished( ResourceCalendar *resource );

    void resourceChanged( ResourceCalendar *resource );

    void loadingError( ResourceCalendar *resource, const QString &message );

    void savingError( ResourceCalendar *resource, const QString &message );

    void delayedSaveCalendar();
};

#endif
// kate: space-indent on; indent-width 2; replace-tabs on;
