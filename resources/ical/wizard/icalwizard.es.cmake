/*
    Copyright (c) 2010 Till Adam <adam@kde.org>

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

var page = Dialog.addPage( "icalwizard.ui", qsTr("Settings") );

page.widget().lineEdit.text = "${ICAL_FILE_DEFAULT_PATH}";

function validateInput()
{
  if ( page.widget().lineEdit.text == "" ) {
    page.setValid( false );
  } else {
    page.setValid( true );
  }
}

function setup()
{
  var icalRes = SetupManager.createResource( "akonadi_ical_resource" );
  icalRes.setOption( "Path", page.widget().lineEdit.text );
  icalRes.setName( qsTr("Default Calendar") );
  SetupManager.execute();
}

page.widget().lineEdit.textChanged.connect( validateInput );
page.pageLeftNext.connect( setup );
validateInput();
