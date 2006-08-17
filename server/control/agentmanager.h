/***************************************************************************
 *   Copyright (C) 2006 by Tobias Koenig <tokoe@kde.org>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#ifndef AGENTMANAGER_H
#define AGENTMANAGER_H

#include <QtCore/QStringList>

#include "pluginmanager.h"
#include "profilemanager.h"
#include "tracerinterface.h"

/**
 * Interface for handling profiles and agents in
 * the Akonadi server.
 */
class AgentManager : public QObject
{
  Q_OBJECT

  public:
    /**
     * Creates a new agent manager.
     *
     * @param parent The parent object.
     */
    AgentManager( QObject *parent = 0 );

    /**
     * Destroys the agent manager.
     */
    ~AgentManager();


  public Q_SLOTS:
    /**
     * Agent types specific methods
     */

    /**
     * Returns the list of identifiers of all available
     * agent types.
     */
    QStringList agentTypes() const;

    /**
     * Returns the i18n'ed name of the agent type for
     * the given @p identifier.
     */
    QString agentName( const QString &identifier ) const;

    /**
     * Returns the i18n'ed comment of the agent type for
     * the given @p identifier.
     */
    QString agentComment( const QString &identifier ) const;

    /**
     * Returns the icon name of the agent type for the
     * given @p identifier.
     */
    QString agentIcon( const QString &identifier ) const;

    /**
     * Returns a list of supported mimetypes of the agent type
     * for the given @p identifier.
     */
    QStringList agentMimeTypes( const QString &identifier ) const;

    /**
     * Returns a list of supported capabilities of the agent type
     * for the given @p identifier.
     */
    QStringList agentCapabilities( const QString &identifier ) const;


    /**
     * Agent specific methods
     */

    /**
     * Creates a new agent of the given agent type @p identifier.
     * The identifier is something like 'imap' or 'file'.
     *
     * @return The identifier of the new agent if created successfully,
     *         an empty string otherwise.
     *         The identifier consists of two parts, the type of the
     *         agent and an unique instance number, and looks like
     *         the following: 'file_1' or 'imap_267'.
     */
    QString createAgentInstance( const QString &identifier );

    /**
     * Removes the agent with the given @p identifier.
     */
    void removeAgentInstance( const QString &identifier );

    /**
     * Returns the type of the agent instance with the given @p identifier.
     */
    QString agentInstanceType( const QString &identifier );

    /**
     * Returns the list of identifiers of configured instances.
     */
    QStringList agentInstances() const;

    /**
     * Returns the current status code of the agent with the given @p identifier.
     */
    int agentInstanceStatus( const QString &identifier ) const;

    /**
     * Returns the i18n'ed description of the current status of the agent with
     * the given @p identifier.
     */
    QString agentInstanceStatusMessage( const QString &identifier ) const;

    /**
     * Asks the agent to store the item with the given
     * identifier to the given @p collection as full or lightwight
     * version, depending on @p type.
     */
    bool requestItemDelivery( const QString &agentIdentifier, const QString &itemIdentifier,
                              const QString &collection, int type );

    /**
     * Profile specific methods
     */

    /**
     * Returns the list of identifiers of available profiles.
     */
    QStringList profiles() const;

    /**
     * Creates a new profile with the given @p identifier.
     *
     * @return true if created successful, false a profile with the
     *         same @p identifier already exists.
     */
    bool createProfile( const QString &identifier );

    /**
     * Removes the profile with the given @p identifier.
     */
    void removeProfile( const QString &identifier );

    /**
     * Adds the agent with the given identifier to the profile with
     * the given identifier.
     *
     * @return true on success, false otherwise.
     */
    bool profileAddAgent( const QString &profileIdentifier, const QString &agentIdentifier );

    /**
     * Removes the agent with the given identifier from the profile with
     * the given identifier.
     *
     * @return true on success, false otherwise.
     */
    bool profileRemoveAgent( const QString &profileIdentifier, const QString &agentIdentifier );

    /**
     * Returns the list of identifiers of all agents in the profile
     * with the given identifier.
     */
    QStringList profileAgents( const QString &identifier ) const;

  Q_SIGNALS:
    /**
     * Agent types specific signals
     */

    /**
     * This signal is emitted whenever a new agent type was installed on the system.
     *
     * @param agentType The identifier of the new agent type.
     */
    void agentTypeAdded( const QString &agentType );

    /**
     * This signal is emitted whenever an agent type was removed from the system.
     *
     * @param agentType The identifier of the removed agent type.
     */
    void agentTypeRemoved( const QString &agentType );

    /**
     * Agent specific signals
     */

    /**
     * This signal is emitted whenever a new agent instance was created.
     *
     * @param agentIdentifier The identifier of the new agent instance.
     */
    void agentInstanceAdded( const QString &agentIdentifier );

    /**
     * This signal is emitted whenever an agent instance was removed.
     *
     * @param agentIdentifier The identifier of the removed agent instance.
     */
    void agentInstanceRemoved( const QString &agentIdentifier );

    /**
     * This signal is emitted whenever the status of an agent instance has
     * changed.
     *
     * @param agentIdentifier The identifier of the agent that has changed.
     * @param status The new status code.
     * @param message The i18n'ed description of the new status.
     */
    void agentInstanceStatusChanged( const QString &agentIdentifier, int status, const QString &message );

    /**
     * Profile specific signals
     */

    /**
     * This signal is emitted whenever a new profile was created.
     *
     * @param profileIdentifier The identifier of the new profile.
     */
    void profileAdded( const QString &profileIdentifier );

    /**
     * This signal is emitted whenever a profile was removed.
     *
     * @param profileIdentifier The identifier of the removed profile.
     */
    void profileRemoved( const QString &profileIdentifier );

    /**
     * This signal is emitted whenever an agent was added to a profile.
     *
     * @param profileIdentifier The identifier of the profile.
     * @param agentIdentifier The identifier of the agent.
     */
    void profileAgentAdded( const QString &profileIdentifier, const QString &agentIdentifier );

    /**
     * This signal is emitted whenever an agent was removed from a profile.
     *
     * @param profileIdentifier The identifier of the profile.
     * @param agentIdentifier The identifier of the agent.
     */
    void profileAgentRemoved( const QString &profileIdentifier, const QString &agentIdentifier );

  private:
    PluginManager mPluginManager;
    ProfileManager mProfileManager;
    org::kde::Akonadi::Tracer *mTracer;
};

#endif
