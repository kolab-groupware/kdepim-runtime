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

#ifndef DUMMYRESOURCESTATE_H
#define DUMMYRESOURCESTATE_H

#include <QtCore/QPair>
#include <QtCore/QVariant>

#include "resourcestateinterface.h"

typedef QPair<Akonadi::Tag::List, QHash<QString, Akonadi::Item::List> > TagListAndMembers;

class DummyResourceState : public ResourceStateInterface
{
public:
  typedef boost::shared_ptr<DummyResourceState> Ptr;

  explicit DummyResourceState();
  ~DummyResourceState();

  void setUserName( const QString &name );
  virtual QString userName() const;

  void setResourceName( const QString &name );
  virtual QString resourceName() const;

  void setResourceIdentifier( const QString &identifier );
  virtual QString resourceIdentifier() const;

  void setServerCapabilities( const QStringList &capabilities );
  virtual QStringList serverCapabilities() const;

  void setServerNamespaces( const QList<KIMAP::MailBoxDescriptor> &namespaces );
  virtual QList<KIMAP::MailBoxDescriptor> serverNamespaces() const;
  virtual QList<KIMAP::MailBoxDescriptor> personalNamespaces() const;
  virtual QList<KIMAP::MailBoxDescriptor> userNamespaces() const;
  virtual QList<KIMAP::MailBoxDescriptor> sharedNamespaces() const;

  void setAutomaticExpungeEnagled( bool enabled );
  virtual bool isAutomaticExpungeEnabled() const;

  void setSubscriptionEnabled( bool enabled );
  virtual bool isSubscriptionEnabled() const;
  void setDisconnectedModeEnabled( bool enabled );
  virtual bool isDisconnectedModeEnabled() const;
  void setIntervalCheckTime( int interval );
  virtual int intervalCheckTime() const;


  void setCollection( const Akonadi::Collection &collection );
  virtual Akonadi::Collection collection() const;
  void setItem( const Akonadi::Item &item );
  virtual Akonadi::Item item() const;
  virtual Akonadi::Item::List items() const;

  void setParentCollection( const Akonadi::Collection &collection );
  virtual Akonadi::Collection parentCollection() const;

  void setSourceCollection( const Akonadi::Collection &collection );
  virtual Akonadi::Collection sourceCollection() const;
  void setTargetCollection( const Akonadi::Collection &collection );
  virtual Akonadi::Collection targetCollection() const;

  void setParts( const QSet<QByteArray> &parts );
  virtual QSet<QByteArray> parts() const;

  void setTag( const Akonadi::Tag &tag );
  virtual Akonadi::Tag tag() const;
  void setAddedTags( const QSet<Akonadi::Tag> &addedTags );
  virtual QSet<Akonadi::Tag> addedTags() const;
  void setRemovedTags( const QSet<Akonadi::Tag> &removedTags );
  virtual QSet<Akonadi::Tag> removedTags() const;

  virtual Akonadi::Relation::List addedRelations() const;
  virtual Akonadi::Relation::List removedRelations() const;

  virtual QString rootRemoteId() const;

  virtual void setIdleCollection( const Akonadi::Collection &collection );
  virtual void applyCollectionChanges( const Akonadi::Collection &collection );

  virtual void collectionAttributesRetrieved( const Akonadi::Collection &collection );

  virtual void itemRetrieved( const Akonadi::Item &item );

  virtual void itemsRetrieved( const Akonadi::Item::List &items );
  virtual void itemsRetrievedIncremental( const Akonadi::Item::List &changed, const Akonadi::Item::List &removed );
  virtual void itemsRetrievalDone();

  virtual void setTotalItems(int);

  virtual QSet< QByteArray > addedFlags() const;
  virtual QSet< QByteArray > removedFlags() const;

  virtual void itemChangeCommitted( const Akonadi::Item &item );
  virtual void itemsChangesCommitted(const Akonadi::Item::List& items);

  virtual void collectionsRetrieved( const Akonadi::Collection::List &collections );

  virtual void collectionChangeCommitted( const Akonadi::Collection &collection );

  virtual void tagsRetrieved( const Akonadi::Tag::List &tags, const QHash<QString, Akonadi::Item::List> & );
  virtual void relationsRetrieved( const Akonadi::Relation::List &tags );
  virtual void tagChangeCommitted( const Akonadi::Tag &tag );

  virtual void searchFinished( const QVector<qint64> &result, bool isRid = true );

  virtual void changeProcessed();

  virtual void cancelTask( const QString &errorString );
  virtual void deferTask();
  virtual void restartItemRetrieval(Akonadi::Collection::Id col);
  virtual void taskDone();

  virtual void emitError( const QString &message );
  virtual void emitWarning( const QString &message );
  virtual void emitPercent( int percent );

  virtual void synchronizeCollectionTree();
  virtual void scheduleConnectionAttempt();

  virtual QChar separatorCharacter() const;
  virtual void setSeparatorCharacter( const QChar &separator );

  virtual void showInformationDialog( const QString &message, const QString &title, const QString &dontShowAgainName );

  virtual int batchSize() const;

  virtual MessageHelper::Ptr messageHelper() const;

  QList< QPair<QByteArray, QVariant> > calls() const;

private:
  void recordCall( const QByteArray callName, const QVariant &parameter = QVariant() );

  QString m_userName;
  QString m_resourceName;
  QString m_resourceIdentifier;
  QStringList m_capabilities;
  QList<KIMAP::MailBoxDescriptor> m_namespaces;

  bool m_automaticExpunge;
  bool m_subscriptionEnabled;
  bool m_disconnectedMode;
  int m_intervalCheckTime;
  QChar m_separator;

  Akonadi::Collection m_collection;
  Akonadi::Item::List m_items;

  Akonadi::Collection m_parentCollection;

  Akonadi::Collection m_sourceCollection;
  Akonadi::Collection m_targetCollection;

  QSet<QByteArray> m_parts;

  Akonadi::Tag m_tag;
  QSet<Akonadi::Tag> m_addedTags;
  QSet<Akonadi::Tag> m_removedTags;

  QList< QPair<QByteArray, QVariant> > m_calls;
};

#endif
