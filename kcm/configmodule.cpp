/*
    Copyright (c) 2007 Volker Krause <vkrause@kde.org>
    Copyright (c) 2008 Omat Holding B.V. <tomalbers@kde.nl>

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

#include "configmodule.h"
#include "resourcesmanagementwidget.h"

#include <klocale.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <qboxlayout.h>
#include <akonadi/control.h>

K_PLUGIN_FACTORY( ResourcesConfigFactory, registerPlugin<ConfigModule>(); )
K_EXPORT_PLUGIN( ResourcesConfigFactory( "imaplib" ) )

ConfigModule::ConfigModule( QWidget * parent, const QVariantList & args ) :
        KCModule( ResourcesConfigFactory::componentData(), parent, args )
{
    KGlobal::locale()->insertCatalog( QLatin1String("kcm_akonadi") );
    KGlobal::locale()->insertCatalog( QLatin1String("libakonadi") );
    Akonadi::Control::widgetNeedsAkonadi(this);
    setButtons( KCModule::Default | KCModule::Apply );
    QVBoxLayout *l = new QVBoxLayout( this );
    l->setMargin( 0 );

    QStringList args2;
    foreach ( const QVariant& item, args )
      args2 << item.toString();

    ResourcesManagementWidget *tmw = new ResourcesManagementWidget( this, args2 );
    l->addWidget( tmw );
}
