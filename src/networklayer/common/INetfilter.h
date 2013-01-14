//
// Copyright (C) 2012 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef __INET_INETFILTER_H_
#define __INET_INETFILTER_H_

#include <omnetpp.h>
#include "InterfaceEntry.h"
#include "INetworkDatagram.h"

class INetfilter {
  public:
    class IHook {
      public:
        enum Result {
            ACCEPT, /**< allow datagram to pass to next hook */
            DROP, /**< do not allow datagram to pass to next hook, delete it */
            QUEUE, /**< queue datagram for later re-injection */
            STOLEN /**< do not allow datagram to pass to next hook, but do not delete it */
        };

        virtual ~IHook() {};
        virtual Result datagramPreRoutingHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) = 0;
        virtual Result datagramLocalInHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) = 0;
        virtual Result datagramForwardHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry * outputInterfaceEntry, const Address & nextHopAddress) = 0;
        virtual Result datagramPostRoutingHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry * outputInterfaceEntry, const Address & nextHopAddress) = 0;
        virtual Result datagramLocalOutHook(INetworkDatagram * datagram, const InterfaceEntry * outputInterfaceEntry) = 0;
    };

    virtual ~INetfilter() { }
    virtual void registerHook(int priority, IHook * hook) = 0;
    virtual void unregisterHook(int priority, IHook * hook) = 0;
    virtual void reinjectDatagram(const INetworkDatagram * datagram, IHook::Result verdict) = 0;
};

#endif
