/*  -*- mode: C++; c-file-style: "gnu" -*-

    This file is part of kdepim.
    Copyright (c) 2004 David Faure <faure@kde.org>

    KMail is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License, version 2, as
    published by the Free Software Foundation.

    KMail is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/
#ifndef PROCESSCOLLECTOR_H
#define PROCESSCOLLECTOR_H

#include <qobject.h>
class KProcess;

namespace KPIM {

/**
 * A helper class to collect the output (stdout/stderr) of a KProcess
 */
class ProcessCollector : public QObject {
  Q_OBJECT

public:
  ProcessCollector( KProcess* process, QObject* parent = 0, const char* name = 0 );

  QByteArray collectedStdOut() const { return mStdOutCollection; }
  QByteArray collectedStdErr() const { return mStdErrCollection; }

private slots:
  void slotCollectStdOut( KProcess * proc, char * buffer, int len );
  void slotCollectStdErr( KProcess * proc, char * buffer, int len );

private:
  QByteArray mStdOutCollection;
  QByteArray mStdErrCollection;
};

} // namespace

#endif /* PROCESSCOLLECTOR_H */

