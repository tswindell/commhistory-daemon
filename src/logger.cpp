/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#include <QtDBus/QtDBus>
#include <QDebug>

#include <TelepathyQt4/ClientRegistrar>
#include <TelepathyQt4/Debug>
#include <TelepathyQt4/Channel>

#include "logger.h"
#include "channellistener.h"
#include "textchannellistener.h"
#include "streamchannellistener.h"
#include "loggerclientobserver.h"

#define COMMHISTORY_CHANNEL_OBSERVER QLatin1String("CommHistory")

using namespace RTComLogger;
using namespace Tp;

Logger::Logger(const Tp::AccountManagerPtr &accountManager,
               QObject *parent)
    : QObject(parent)
{
    Tp::registerTypes();
#ifdef QT_DEBUG
    Tp::enableDebug(true);
    Tp::enableWarnings(true);
#endif

    m_Registrar = ClientRegistrar::create(accountManager);

    ChannelClassList channelFilters;
    QMap<QString, QDBusVariant> textFilter, mediaFilter;
    // Registering Text channel observer
    textFilter.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".ChannelType"),
                  QDBusVariant(TELEPATHY_INTERFACE_CHANNEL_TYPE_TEXT));
    textFilter.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".TargetHandleType"),
                  QDBusVariant(Tp::HandleTypeNone));
    textFilter.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".ChannelType"),
                  QDBusVariant(TELEPATHY_INTERFACE_CHANNEL_TYPE_TEXT));
    textFilter.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".TargetHandleType"),
                  QDBusVariant(Tp::HandleTypeContact));
    textFilter.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".ChannelType"),
                  QDBusVariant(TELEPATHY_INTERFACE_CHANNEL_TYPE_TEXT));
    textFilter.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".TargetHandleType"),
                  QDBusVariant(Tp::HandleTypeRoom));
    channelFilters.append(textFilter);

    // Registering Media channel observer
    mediaFilter.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".ChannelType"),
                  QDBusVariant(TELEPATHY_INTERFACE_CHANNEL_TYPE_STREAMED_MEDIA));
    mediaFilter.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".TargetHandleType"),
                  QDBusVariant(Tp::HandleTypeContact));
    channelFilters.append(mediaFilter);

    LoggerClientObserver* observer = new LoggerClientObserver( channelFilters, this );
    bool registered = m_Registrar->registerClient(
      AbstractClientPtr::dynamicCast(SharedPtr<LoggerClientObserver>(observer)),
      COMMHISTORY_CHANNEL_OBSERVER);

    if(registered) {
        qDebug() << "commhistoryd: started";
    } else {
        qCritical() << "commhistoryd: observer registration failed";
    }
}

Logger::~Logger()
{
}

void Logger::createChannelListener(const QString &channelType,
                                   const Tp::MethodInvocationContextPtr<> &context,
                                   const Tp::AccountPtr &account,
                                   const Tp::ChannelPtr &channel)
{
    qDebug() << __PRETTY_FUNCTION__;

    QString channelObjectPath = channel->objectPath();

    if ( m_Channels.contains( channelObjectPath ) &&
         !channelType.isEmpty() &&
         !channelObjectPath.isEmpty() ) {
         context->setFinishedWithError(QLatin1String(TELEPATHY_ERROR_INVALID_ARGUMENT), QString());
         return;
    }

    qDebug() << "creating listener for: " << channelObjectPath << " type " << channelType;

    ChannelListener* listener = 0;
    if( channelType == QLatin1String(TELEPATHY_INTERFACE_CHANNEL_TYPE_TEXT) ) {
        listener = new TextChannelListener(account, channel, context, this);
    } else if ( channelType == QLatin1String(TELEPATHY_INTERFACE_CHANNEL_TYPE_STREAMED_MEDIA) ) {
        listener = new StreamChannelListener(account, channel, context, this);
    }

    if(listener) {
        connect(listener, SIGNAL(channelClosed(ChannelListener *)),
                this, SLOT(channelClosed(ChannelListener *)));
        m_Channels.append( channelObjectPath );
    }
}

void Logger::channelClosed(ChannelListener *listener)
{
    if(listener) {
        m_Channels.removeAll(listener->channel());
        listener->deleteLater();
        listener = 0;
    }
}
