/*
    Copyright (c) 2006 - 2007 Volker Krause <vkrause@kde.org>
    Copyright (c) 2008 Omat Holding B.V. <tomalbers@kde.nl>

    Based on KMail code by:
    Copyright (C) 2001-2003 Marc Mutz <mutz@kde.org>

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

#ifndef RFC822_MANAGEMENTWIDGET_H
#define RFC822_MANAGEMENTWIDGET_H

#include <rfc822_export.h>
#include <QtGui/QWidget>

/**
  A widget to manage imaplib
*/
class RFC822_EXPORT RFC822ManagementWidget : public QWidget
{
  Q_OBJECT

  public:
    /**
      Creates a new RFC822ManagementWidget.
      @param parent The parent widget.
    */
    RFC822ManagementWidget( QWidget *parent = 0 );

    /**
      Destroys the widget.
    */
    virtual ~RFC822ManagementWidget();

  private Q_SLOTS:
    void fillResourcesList();
    void updateButtonState();
    void addClicked( QAction* );
    void editClicked();
    void removeClicked();

  private:
    class Private;
    Private * const d;
};

#endif
