/*
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>
    Copyright (c) 2008 Omat Holding B.V. <info@omat.nl>

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

#include "settings.h"
#include "settingsadaptor.h"

#include "imapaccount.h"

#include <kwallet.h>
using KWallet::Wallet;

#include <klocale.h>
#include <kpassworddialog.h>

#include <QDBusConnection>

#include <KDE/Akonadi/Collection>
#include <KDE/Akonadi/CollectionFetchJob>
#include <KDE/Akonadi/CollectionModifyJob>

/**
 * Maps the enum used to represent authentication in MailTransport (kdepimlibs)
 * to the one used by the imap resource.
 * @param authType the MailTransport auth enum value
 * @return the corresponding KIMAP auth value.
 * @note will cause fatal error if there is no mapping, so be careful not to pass invalid auth options (e.g., APOP) to this function.
 */
KIMAP::LoginJob::AuthenticationMode Settings::mapTransportAuthToKimap( MailTransport::Transport::EnumAuthenticationType::type authType )
{
  // typedef these for readability
  typedef MailTransport::Transport::EnumAuthenticationType MTAuth;
  typedef KIMAP::LoginJob KIAuth;
  switch ( authType ) {
    case MTAuth::ANONYMOUS:
      return KIAuth::Anonymous;
    case MTAuth::PLAIN:
      return KIAuth::Plain;
    case MTAuth::NTLM:
      return KIAuth::NTLM;
    case MTAuth::LOGIN:
      return KIAuth::Login;
    case MTAuth::GSSAPI:
      return KIAuth::GSSAPI;
    case MTAuth::DIGEST_MD5:
      return KIAuth::DigestMD5;
    case MTAuth::CRAM_MD5:
      return KIAuth::CramMD5;
    case MTAuth::CLEAR:
      return KIAuth::ClearText;
    default:
      kFatal() << "mapping from Transport::EnumAuthenticationType ->  KIMAP::LoginJob::AuthenticationMode not possible";
  }
  return KIAuth::ClearText; // dummy value, shouldn't get here.
}

Settings::Settings( WId winId ) : SettingsBase(), m_winId( winId )
{
    readConfig();

    new SettingsAdaptor( this );
    QDBusConnection::sessionBus().registerObject( QLatin1String( "/Settings" ), this, QDBusConnection::ExportAdaptors | QDBusConnection::ExportScriptableContents );
}

void Settings::setWinId( WId winId )
{
    m_winId = winId;
}

void Settings::clearCachedPassword()
{
    m_password.clear();
}

void Settings::cleanup()
{
    Wallet* wallet = Wallet::openWallet( Wallet::NetworkWallet(), m_winId );
    if ( wallet && wallet->isOpen() ) {
        if ( wallet->hasFolder( QLatin1String("imap") ) ) {
            wallet->setFolder( QLatin1String("imap") );
            wallet->removeEntry( config()->name() );
        }
        delete wallet;
    }
}

void Settings::requestPassword()
{
  if ( !m_password.isEmpty() ||
       ( mapTransportAuthToKimap( (MailTransport::TransportBase::EnumAuthenticationType::type)authentication() ) == KIMAP::LoginJob::GSSAPI ) ) {
    emit passwordRequestCompleted( m_password, false );
  } else {
    Wallet *wallet = Wallet::openWallet( Wallet::NetworkWallet(), m_winId, Wallet::Asynchronous );
    if ( wallet ) {
      connect( wallet, SIGNAL(walletOpened(bool)),
               this, SLOT(onWalletOpened(bool)) );
    } else {
      QMetaObject::invokeMethod( this, "onWalletOpened", Qt::QueuedConnection, Q_ARG(bool, true) );
    }
  }
}

void Settings::onWalletOpened( bool success )
{
  if ( !success ) {
    emit passwordRequestCompleted( QString(), true );
  } else {
    Wallet *wallet = qobject_cast<Wallet*>( sender() );
    bool passwordNotStoredInWallet = true;
    if ( wallet && wallet->hasFolder( QLatin1String("imap") ) ) {
      wallet->setFolder( QLatin1String("imap") );
      wallet->readPassword( config()->name(), m_password );
      passwordNotStoredInWallet = false;
    }
    if ( passwordNotStoredInWallet || m_password.isEmpty() )
      requestManualAuth();
    else
      emit passwordRequestCompleted( m_password, passwordNotStoredInWallet );
    
    if ( wallet ) {
      wallet->deleteLater();
    }
  }
}

void Settings::requestManualAuth()
{
  KPasswordDialog *dlg = new KPasswordDialog( 0 );
  dlg->setModal( true );
  dlg->setPrompt( i18n( "Please enter password for user '%1' on IMAP server '%2'.",
                        userName(), imapServer() ) );
  dlg->setAttribute( Qt::WA_DeleteOnClose );
  connect( dlg, SIGNAL(finished(int)), this, SLOT(onDialogFinished(int)) );
  dlg->show();
}

void Settings::onDialogFinished( int result )
{
  if ( result == QDialog::Accepted ) {
    KPasswordDialog *dlg = qobject_cast<KPasswordDialog*>( sender() );
    setPassword( dlg->password() );
    emit passwordRequestCompleted( dlg->password(), false );
  } else {
    emit passwordRequestCompleted( QString(), true );
  }
}

QString Settings::password(bool *userRejected) const
{
    if ( userRejected != 0 ) {
      *userRejected = false;
    }

    if ( !m_password.isEmpty() ||
         ( mapTransportAuthToKimap( (MailTransport::TransportBase::EnumAuthenticationType::type)authentication() ) == KIMAP::LoginJob::GSSAPI ) )
      return m_password;
    Wallet* wallet = Wallet::openWallet( Wallet::NetworkWallet(), m_winId );
    if ( wallet && wallet->isOpen() ) {
      if ( wallet->hasFolder( QLatin1String("imap") ) ) {
        wallet->setFolder( QLatin1String("imap") );
        wallet->readPassword( config()->name(), m_password );
      } else {
        wallet->createFolder( QLatin1String("imap") );
      }
    } else if ( userRejected != 0 ) {
        *userRejected = true;
    }
    delete wallet;
    return m_password;
}

QString Settings::sieveCustomPassword(bool *userRejected) const
{
    if ( userRejected != 0 ) {
      *userRejected = false;
    }

    if ( !m_customSievePassword.isEmpty() )
      return m_customSievePassword;

    Wallet* wallet = Wallet::openWallet( Wallet::NetworkWallet(), m_winId );
    if ( wallet && wallet->isOpen() ) {
      if ( wallet->hasFolder( QLatin1String("imap") ) ) {
        wallet->setFolder( QLatin1String("imap") );
        wallet->readPassword( QLatin1String("custom_sieve_") + config()->name(), m_customSievePassword );
      } else {
        wallet->createFolder( QLatin1String("imap") );
      }
    } else if ( userRejected != 0 ) {
        *userRejected = true;
    }
    delete wallet;
    return m_customSievePassword;
}

void Settings::setSieveCustomPassword(const QString & password)
{
    if (m_customSievePassword == password)
        return;
    m_customSievePassword = password;
    Wallet* wallet = Wallet::openWallet( Wallet::NetworkWallet(), m_winId );
    if ( wallet && wallet->isOpen() ) {
        if ( !wallet->hasFolder( QLatin1String("imap") ) )
            wallet->createFolder( QLatin1String("imap") );
        wallet->setFolder( QLatin1String("imap") );
        wallet->writePassword( QLatin1String("custom_sieve_") + config()->name(), password );
        kDebug() << "Wallet save: " << wallet->sync();
    }
    delete wallet;
}

void Settings::setPassword( const QString & password )
{
    if ( password == m_password )
        return;

    if ( mapTransportAuthToKimap( (MailTransport::TransportBase::EnumAuthenticationType::type)authentication() ) == KIMAP::LoginJob::GSSAPI )
        return;

    m_password = password;
    Wallet* wallet = Wallet::openWallet( Wallet::NetworkWallet(), m_winId );
    if ( wallet && wallet->isOpen() ) {
        if ( !wallet->hasFolder( QLatin1String("imap") ) )
            wallet->createFolder( QLatin1String("imap") );
        wallet->setFolder( QLatin1String("imap") );
        wallet->writePassword( config()->name(), password );
        kDebug() << "Wallet save: " << wallet->sync();
    }
    delete wallet;
}

void Settings::loadAccount( ImapAccount *account ) const
{
  account->setServer( imapServer() );
  if ( imapPort()>=0 ) {
    account->setPort( imapPort() );
  }

  account->setUserName( userName() );
  account->setSubscriptionEnabled( subscriptionEnabled() );

  const QString encryption = safety();
  if ( encryption == QLatin1String("SSL") ) {
    account->setEncryptionMode( KIMAP::LoginJob::AnySslVersion );
  } else if (  encryption == QLatin1String("STARTTLS") ) {
    //KIMAP confused TLS and STARTTLS, TlsV1 really means "use STARTTLS"
    account->setEncryptionMode( KIMAP::LoginJob::TlsV1 );
  } else {
    account->setEncryptionMode( KIMAP::LoginJob::Unencrypted );
  }

  //Some SSL Server fail to advertise an ssl version they support (AnySslVersion),
  //we therefore allow overriding this in the config
  //(so we don't have to make the UI unnecessarily complex for properly working servers).
  const QString overrideEncryptionMode = overrideEncryption();
  if (!overrideEncryptionMode.isEmpty()) {
    kWarning() << "Overriding encryption mode with: " << overrideEncryptionMode;
    if ( overrideEncryptionMode == QLatin1String("SSLV2") ) {
      account->setEncryptionMode( KIMAP::LoginJob::SslV2 );
    } else if (  overrideEncryptionMode == QLatin1String("SSLV3") ) {
      account->setEncryptionMode( KIMAP::LoginJob::SslV3 );
    } else if (  overrideEncryptionMode == QLatin1String("TLSV1") ) {
      account->setEncryptionMode( KIMAP::LoginJob::SslV3_1 );
    } else if ( overrideEncryptionMode == QLatin1String("SSL") ) {
      account->setEncryptionMode( KIMAP::LoginJob::AnySslVersion );
    } else if (  overrideEncryptionMode == QLatin1String("STARTTLS") ) {
      account->setEncryptionMode( KIMAP::LoginJob::TlsV1 );
    } else if (  overrideEncryptionMode == QLatin1String("UNENCRYPTED") ) {
      account->setEncryptionMode( KIMAP::LoginJob::Unencrypted );
    } else {
      kWarning() << "Tried to force invalid encryption mode: " << overrideEncryptionMode;
    }
  }

  account->setAuthenticationMode(
      mapTransportAuthToKimap(
          (MailTransport::TransportBase::EnumAuthenticationType::type) authentication()
      )
  );

  account->setTimeout( sessionTimeout() );

}

QString Settings::rootRemoteId() const
{
  return QLatin1String("imap://") + userName() + QLatin1Char('@') + imapServer() + QLatin1Char('/');
}

void Settings::renameRootCollection( const QString &newName )
{
  Akonadi::Collection rootCollection;
  rootCollection.setRemoteId( rootRemoteId() );
  Akonadi::CollectionFetchJob *fetchJob =
      new Akonadi::CollectionFetchJob( rootCollection, Akonadi::CollectionFetchJob::Base );
  fetchJob->setProperty( "collectionName", newName );
  connect( fetchJob, SIGNAL(result(KJob*)),
           this, SLOT(onRootCollectionFetched(KJob*)) );
}

void Settings::onRootCollectionFetched( KJob *job )
{
  const QString newName = job->property( "collectionName" ).toString();
  Q_ASSERT( !newName.isEmpty() );
  Akonadi::CollectionFetchJob *fetchJob = static_cast<Akonadi::CollectionFetchJob*>( job );
  if ( fetchJob->collections().size() == 1 ) {
    Akonadi::Collection rootCollection = fetchJob->collections().first();
    rootCollection.setName( newName );
    new Akonadi::CollectionModifyJob( rootCollection );
    // We don't care about the result here, nothing we can/should do if the renaming fails
  }
}

