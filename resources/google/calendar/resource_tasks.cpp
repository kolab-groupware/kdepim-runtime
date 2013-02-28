/*
    Copyright (C) 2011, 2012  Dan Vratil <dan@progdan.cz>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "calendarresource.h"
#include "settings.h"

#include <libkgapi/accessmanager.h>
#include <libkgapi/auth.h>
#include <libkgapi/fetchlistjob.h>
#include <libkgapi/reply.h>
#include <libkgapi/objects/task.h>
#include <libkgapi/objects/tasklist.h>
#include <libkgapi/services/tasks.h>

#include <KLocalizedString>
#include <KDebug>

#include <Akonadi/ItemModifyJob>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/EntityDisplayAttribute>
#include <Akonadi/CollectionModifyJob>
#include <Akonadi/ItemFetchScope>

using namespace Akonadi;
using namespace KGAPI;
using namespace KCalCore;

void CalendarResource::taskDoUpdate( Reply *reply )
{
  Account::Ptr account = getAccount();
  if ( account.isNull() ) {
    deferTask();
    return;
  }

  Item item = reply->request()->property( "Item" ).value< Item >();
  if ( !item.hasPayload< Todo::Ptr >() ) {
      cancelTask();
      return;
  }

  Todo::Ptr todo = item.payload< TodoPtr >();
  Objects::Task ktodo( *todo );

  QUrl url = Services::Tasks::updateTaskUrl( item.parentCollection().remoteId(), item.remoteId() );

  Services::Tasks service;
  QByteArray data = service.objectToJSON( static_cast< KGAPI::Object *>( &ktodo ) );

  Request *request = new Request( url, Request::Update, "Tasks", account );
  request->setRequestData( data, "application/json" );
  request->setProperty( "Item", QVariant::fromValue( item ) );
  m_gam->sendRequest( request );
}


void CalendarResource::removeTaskFetchJobFinished( KJob *job )
{
  if ( job->error() ) {
    cancelTask( i18n( "Failed to delete task (1): %1", job->errorString() ) );
    return;
  }

  ItemFetchJob *fetchJob = dynamic_cast< ItemFetchJob * >( job );
  Item removedItem = fetchJob->property( "Item" ).value< Item >();

  Item::List detachItems;

  Item::List items = fetchJob->items();
  Q_FOREACH ( Item item, items ) { //krazy:exclude=foreach
    if( !item.hasPayload< Todo::Ptr >() ) {
      kDebug() << "Item " << item.remoteId() << " does not have Todo payload";
      continue;
    }

    Todo::Ptr todo = item.payload< Todo::Ptr >();
    /* If this item is child of the item we want to remove then add it to detach list */
    if ( todo->relatedTo( KCalCore::Incidence::RelTypeParent ) == removedItem.remoteId() ) {
      todo->setRelatedTo( QString(), KCalCore::Incidence::RelTypeParent );
      item.setPayload( todo );
      detachItems << item;
    }
  }

  /* If there are no items do detach, then delete the task right now */
  if ( detachItems.isEmpty() ) {
    doRemoveTask( job );
    return;
  }

  /* Send modify request to detach all the sub-tasks from the task that is about to be
   * removed. */

  ItemModifyJob *modifyJob = new ItemModifyJob( detachItems );
  modifyJob->setProperty( "Item", qVariantFromValue( removedItem ) );
  modifyJob->setAutoDelete( true );
  connect( modifyJob, SIGNAL(finished(KJob*)), this, SLOT(doRemoveTask(KJob*)) );
  modifyJob->start();
}

void CalendarResource::doRemoveTask( KJob *job )
{
  if ( job->error() ) {
    cancelTask( i18n( "Failed to delete task (2): %1", job->errorString() ) );
    return;
  }

  Account::Ptr account = getAccount();
  if ( account.isNull() ) {
    deferTask();
    return;
  }

  Item item = job->property( "Item" ).value< Item >();

  /* Now finally we can safely remove the task we wanted to */
  Request *request =
    new Request(
      Services::Tasks::removeTaskUrl( item.parentCollection().remoteId(), item.remoteId() ),
      KGAPI::Request::Remove, "Tasks", account );
  request->setProperty( "Item", qVariantFromValue( item ) );
  m_gam->sendRequest( request );
}

void CalendarResource::taskRemoved( KGAPI::Reply *reply )
{
  if ( reply->error() != NoContent ) {
    cancelTask( i18n( "Failed to delete task (5): %1", reply->errorString() ) );
    return;
  }

  Item item = reply->request()->property( "Item" ).value<Item>();
  changeCommitted( item );
}
