/*
 *  settingsdialog.cpp  -  Akonadi KAlarm directory resource configuration dialog
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "settingsdialog.h"
#include "settings.h"
#include "alarmtypewidget.h"

#include <KConfigDialogManager>
#include <KWindowSystem>

#include <QTimer>

namespace Akonadi_KAlarm_Dir_Resource
{

SettingsDialog::SettingsDialog(WId windowId, Settings* settings)
  : KDialog(),
    mSettings(settings),
    mReadOnlySelected(false)
{
    ui.setupUi(mainWidget());
    mTypeSelector = new AlarmTypeWidget(ui.tab, ui.tabLayout);
    ui.ktabwidget->setTabBarHidden(true);
    ui.kcfg_Path->setMode(KFile::LocalOnly | KFile::Directory);
    setButtons(Ok | Cancel);
    setCaption(i18nc("@title", "Configure Calendar"));

    if (windowId)
        KWindowSystem::setMainWindow(this, windowId);

    // Make directory path read-only if the resource already exists
    KUrl path(mSettings->path());
    ui.kcfg_Path->setUrl(path);
    if (!path.isEmpty())
        ui.kcfg_Path->setEnabled(false);

    mTypeSelector->setAlarmTypes(CalEvent::types(mSettings->alarmTypes()));
    mManager = new KConfigDialogManager(this, mSettings);
    mManager->updateWidgets();

    connect(this, SIGNAL(okClicked()), SLOT(save()));
    connect(ui.kcfg_Path, SIGNAL(textChanged(QString)), SLOT(textChanged()));
    connect(ui.kcfg_ReadOnly, SIGNAL(clicked(bool)), SLOT(readOnlyClicked(bool)));
    connect(mTypeSelector, SIGNAL(changed()), SLOT(validate()));

    QTimer::singleShot(0, this, SLOT(validate()));
}

void SettingsDialog::save()
{
    mManager->updateSettings();
    mSettings->setPath(ui.kcfg_Path->url().toLocalFile());
    mSettings->setAlarmTypes(CalEvent::mimeTypes(mTypeSelector->alarmTypes()));
    mSettings->writeConfig();
}

void SettingsDialog::readOnlyClicked(bool set)
{
    mReadOnlySelected = set;
}

void SettingsDialog::textChanged()
{
    bool oldReadOnly = ui.kcfg_ReadOnly->isEnabled();
    validate();
    if (ui.kcfg_ReadOnly->isEnabled()  &&  !oldReadOnly)
    {
        // If read-only was only set earlier by validating the input path,
        // and the path is now ok to be read-write, clear the read-only status.
        ui.kcfg_ReadOnly->setChecked(mReadOnlySelected);
    }
}

void SettingsDialog::validate()
{
    bool enableOk = false;
    // At least one alarm type must be selected
    if (mTypeSelector->alarmTypes() != CalEvent::EMPTY)
    {
        // The entered URL must be valid and local
        const KUrl currentUrl = ui.kcfg_Path->url();
        if (currentUrl.isEmpty())
            ui.kcfg_ReadOnly->setEnabled(true);
        else if (currentUrl.isLocalFile())
        {
            QFileInfo file(currentUrl.toLocalFile());
            // It must either be an existing directory, or in a writable
            // directory, and it must not be an existing file.
            if (file.exists())
            {
                if (file.isDir())
                    enableOk = true;   // it's an existing directory
            }
            else
            {
                // Specified directory doesn't already exist.
                // Find the first level of parent directory which exists,
                // and check that it is writable.
                for ( ; ; )
                {
                    file.setFile(file.dir().absolutePath());   // get parent dir's file info
                    if (file.exists())
                    {
                        if (file.isDir()  &&  file.isWritable())
                            enableOk = true;   // it's possible to create the directory
                        break;
                    }
                }
            }
        }
    }
    enableButton(Ok, enableOk);
}

}

// vim: et sw=4:
