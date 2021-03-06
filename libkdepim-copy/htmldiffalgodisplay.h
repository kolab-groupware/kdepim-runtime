/*
    This file is part of libkdepim.

    Copyright (c) 2004 Tobias Koenig <tokoe@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KDEPIM_HTMLDIFFALGODISPLAY_H
#define KDEPIM_HTMLDIFFALGODISPLAY_H

#include "libkdepim-copy_export.h"
#include "diffalgo.h"

#include <KTextBrowser>

namespace KPIM {

class KDEPIM_COPY_EXPORT HTMLDiffAlgoDisplay : virtual public DiffAlgoDisplay, public KTextBrowser
{
  public:
    HTMLDiffAlgoDisplay( QWidget *parent );

    void begin();
    void end();
    void setLeftSourceTitle( const QString &title );
    void setRightSourceTitle( const QString &title );
    void additionalLeftField( const QString &id, const QString &value );
    void additionalRightField( const QString &id, const QString &value );
    void conflictField( const QString &id, const QString &leftValue,
                        const QString &rightValue );

  private:
    QString mLeftTitle;
    QString mRightTitle;
    QString mText;
};

}

#endif
