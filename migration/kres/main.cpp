/*
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>

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

#include "kabcmigrator.h"
#include "kcalmigrator.h"
#include "knotesmigrator.h"
#include "infodialog.h"

#include <kaboutdata.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kglobal.h>

#include <QDBusConnection>

void connectMigrator( KResMigratorBase *m, InfoDialog *dlg )
{
  if ( !dlg || !m )
    return;
  dlg->migratorAdded();
  QObject::connect( m, SIGNAL(message(KMigratorBase::MessageType,QString)), dlg,
                    SLOT(message(KMigratorBase::MessageType,QString)) );
  QObject::connect( m, SIGNAL(destroyed()), dlg, SLOT(migratorDone()) );
}

int main( int argc, char **argv )
{
  KAboutData aboutData( "kres-migrator", 0,
                        ki18n( "KResource Migration Tool" ),
                        "0.1",
                        ki18n( "Migration of KResource settings and application to Akonadi" ),
                        KAboutData::License_LGPL,
                        ki18n( "(c) 2008 the Akonadi developers" ),
                        KLocalizedString(),
                        "http://pim.kde.org/akonadi/" );
  aboutData.setProgramIconName( QLatin1String("akonadi") );
  aboutData.addAuthor( ki18n( "Volker Krause" ),  ki18n( "Author" ), "vkrause@kde.org" );

  const QStringList supportedTypes = QStringList() << QLatin1String("contact") << QLatin1String("calendar") << QLatin1String("notes");

  KCmdLineArgs::init( argc, argv, &aboutData );
  KCmdLineOptions options;
  options.add( "bridge-only", ki18n( "Only migrate to Akonadi KResource bridges" ) );
  options.add( "omit-client-bridge", ki18n( "Omit setting up of the client side compatibility bridges" ) );
  options.add( "contacts-only", ki18n( "Only migrate contact resources" ) );
  options.add( "calendar-only", ki18n( "Only migrate calendar resources" ) );
  options.add( "notes-only", ki18n( "Only migrate knotes resources" ) );
  options.add( "type <type>", ki18n( "Only migrate the specified types (supported: contact, calendar, notes)" ),
               supportedTypes.join( QLatin1String(",") ).toLatin1() );
  options.add( "interactive", ki18n( "Show reporting dialog" ) );
  options.add( "interactive-on-change", ki18n( "Show report only if changes were made" ) );
  KCmdLineArgs::addCmdLineOptions( options );
  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

  QStringList typesToMigrate;
  foreach ( const QString &type, args->getOption( "type" ).split( QLatin1Char(',') ) ) {
    if ( !supportedTypes.contains( type ) )
      kWarning() << "Unknown resource type: " << type;
    else if ( !QDBusConnection::sessionBus().registerService( QLatin1String("org.kde.Akonadi.KResMigrator.") + type ) )
      kWarning() << "Migrator instance already running for type " << type;
    else
      typesToMigrate << type;
  }
  if ( typesToMigrate.isEmpty() )
    return 1;

  KApplication app;
  app.setQuitOnLastWindowClosed( false );

  KGlobal::setAllowQuit( true );
  KGlobal::locale()->insertCatalog( QLatin1String("libakonadi") );

  InfoDialog *infoDialog = 0;
  if ( args->isSet( "interactive" ) || args->isSet( "interactive-on-change" ) ) {
    infoDialog = new InfoDialog( args->isSet( "interactive-on-change" ) );
    infoDialog->show();
  }

  foreach ( const QString &type, typesToMigrate ) {
    KResMigratorBase *m = 0;
    if ( type == QLatin1String("contact") )
      m = new KABCMigrator();
    else if ( type == QLatin1String("calendar") )
      m = new KCalMigrator();
    else if ( type == QLatin1String("notes") )
      m = new KNotesMigrator();
    else {
      kError() << "Unknown resource type: " << type;
      continue;
    }
    m->setBridgingOnly( args->isSet( "bridge-only" ) );
    m->setOmitClientBridge( args->isSet( "omit-client-bridge" ) );
    connectMigrator( m, infoDialog );
  }

  args->clear();
  const int result = app.exec();
  if ( InfoDialog::hasError() )
    return 3;
  return result;
}
