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
#include "imaplibmanagementwidget.h"

#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <qboxlayout.h>

K_PLUGIN_FACTORY( ImaplibConfigFactory, registerPlugin<ConfigModule>(); )
K_EXPORT_PLUGIN( ImaplibConfigFactory( "imaplib" ) )

ConfigModule::ConfigModule( QWidget * parent, const QVariantList & args ) :
    KCModule( ImaplibConfigFactory::componentData(), parent, args )
{
  setButtons( 0 );
  QVBoxLayout *l = new QVBoxLayout( this );
  l->setMargin( 0 );
  ImaplibManagementWidget *tmw = new ImaplibManagementWidget( this );
  l->addWidget( tmw );
}
