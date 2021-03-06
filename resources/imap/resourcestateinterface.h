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

#ifndef RESOURCESTATEINTERFACE_H
#define RESOURCESTATEINTERFACE_H

#include <QtCore/QStringList>

#include <Akonadi/Collection>
#include <Akonadi/Item>

#include <kimap/listjob.h>

#include <boost/shared_ptr.hpp>
#include "messagehelper.h"

class ResourceStateInterface
{
public:
  typedef boost::shared_ptr<ResourceStateInterface> Ptr;

  virtual ~ResourceStateInterface();

  virtual QString userName() const = 0;
  virtual QString resourceName() const = 0;
  virtual QString resourceIdentifier() const = 0;
  virtual QStringList serverCapabilities() const = 0;
  virtual QList<KIMAP::MailBoxDescriptor> serverNamespaces() const = 0;
  virtual QList<KIMAP::MailBoxDescriptor> personalNamespaces() const = 0;
  virtual QList<KIMAP::MailBoxDescriptor> userNamespaces() const = 0;
  virtual QList<KIMAP::MailBoxDescriptor> sharedNamespaces() const = 0;

  virtual bool isAutomaticExpungeEnabled() const = 0;
  virtual bool isSubscriptionEnabled() const = 0;
  virtual bool isDisconnectedModeEnabled() const = 0;
  virtual int intervalCheckTime() const = 0;

  virtual Akonadi::Collection collection() const = 0;
  virtual Akonadi::Item item() const = 0;
  virtual Akonadi::Item::List items() const = 0;

  virtual Akonadi::Collection parentCollection() const = 0;

  virtual Akonadi::Collection sourceCollection() const = 0;
  virtual Akonadi::Collection targetCollection() const = 0;

  virtual QSet<QByteArray> parts() const = 0;
  virtual QSet<QByteArray> addedFlags() const = 0;
  virtual QSet<QByteArray> removedFlags() const = 0;

  virtual Akonadi::Tag tag() const = 0;
  virtual QSet<Akonadi::Tag> addedTags() const = 0;
  virtual QSet<Akonadi::Tag> removedTags() const = 0;

  virtual QString rootRemoteId() const = 0;
  static QString mailBoxForCollection( const Akonadi::Collection &collection, bool showWarnings = true );

  virtual void setIdleCollection( const Akonadi::Collection &collection ) = 0;
  virtual void applyCollectionChanges( const Akonadi::Collection &collection ) = 0;

  virtual void itemRetrieved( const Akonadi::Item &item ) = 0;

  virtual void itemsRetrieved( const Akonadi::Item::List &items ) = 0;
  virtual void itemsRetrievedIncremental( const Akonadi::Item::List &changed,
                                          const Akonadi::Item::List &removed ) = 0;
  virtual void itemsRetrievalDone() = 0;

  virtual void setTotalItems(int) = 0;

  virtual void itemChangeCommitted( const Akonadi::Item &item ) = 0;
  virtual void itemsChangesCommitted( const Akonadi::Item::List &items ) = 0;

  virtual void collectionsRetrieved( const Akonadi::Collection::List &collections ) = 0;
  virtual void collectionAttributesRetrieved( const Akonadi::Collection &collection ) = 0;

  virtual void collectionChangeCommitted( const Akonadi::Collection &collection ) = 0;

  virtual void tagChangeCommitted( const Akonadi::Tag &tag ) = 0;

  virtual void changeProcessed() = 0;

  virtual void searchFinished( const QVector<qint64> &result, bool isRid = true ) = 0;

  virtual void cancelTask( const QString &errorString ) = 0;
  virtual void deferTask() = 0;
  virtual void restartItemRetrieval(Akonadi::Collection::Id col) = 0;
  virtual void taskDone() = 0;

  virtual void emitError( const QString &message ) = 0;
  virtual void emitWarning( const QString &message ) = 0;
  virtual void emitPercent( int percent ) = 0;

  virtual void synchronizeCollectionTree() = 0;
  virtual void scheduleConnectionAttempt() = 0;

  virtual QChar separatorCharacter() const = 0;
  virtual void setSeparatorCharacter( const QChar &separator ) = 0;

  virtual void showInformationDialog( const QString &message, const QString &title, const QString &dontShowAgainName ) = 0;

  virtual int batchSize() const = 0;

  virtual MessageHelper::Ptr messageHelper() const = 0;
  virtual void tagsRetrieved( const Akonadi::Tag::List &tags, const QHash<QString, Akonadi::Item::List> & ) = 0;
  virtual void relationsRetrieved( const Akonadi::Relation::List &tags) = 0;

  virtual Akonadi::Relation::List addedRelations() const = 0;
  virtual Akonadi::Relation::List removedRelations() const = 0;
};

#endif
