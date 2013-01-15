//
// Copyright (C) 2012 OpenSim Ltd
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
// @author: Zoltan Bojthe

#ifndef __INET_MANETIPV4HOOK_H
#define __INET_MANETIPV4HOOK_H

#include "IPv4.h"

#include "ARP.h"

#include <vector>
#include <set>

class INET_API ManetIPv4Hook : public IPv4::Hook
{
  protected:
    cModule* module;    // Manet module
    IPv4 *ipLayer;      // IPv4 module
    int gateIndex;      // gateindex for manet protocol on transportOut gate in IPv4 module
    bool isReactive;    // true if it's a reactive routing

  public:
    ManetIPv4Hook() : module(NULL), ipLayer(NULL), gateIndex(-1), isReactive(false) {}

  protected:
    void initHook(cModule* module);
    void finishHook();

  protected:
    // Helper functions
    /**
     * Sends a MANET_ROUTE_UPDATE packet to Manet. The datagram is
     * not transmitted, only its source and destination address is used.
     * About DSR datagrams no update message is sent.
     */
    virtual void sendRouteUpdateMessageToManet(IPv4Datagram *datagram);

    /**
     * Sends a MANET_ROUTE_NOROUTE packet to Manet. The packet
     * will encapsulate the given datagram, so this method takes
     * ownership.
     * DSR datagrams are transmitted as they are, i.e. without
     * encapsulation. (?)
     */
    virtual void sendNoRouteMessageToManet(IPv4Datagram *datagram);

    /**
     * Sends a packet to the Manet module.
     */
    virtual void sendToManet(cPacket *packet);

    /**
     *
     */
    virtual bool checkPacketUnroutable(IPv4Datagram* datagram, const InterfaceEntry* outIE);

  public:
    virtual IPv4::Hook::Result datagramPreRoutingHook(IPv4Datagram* datagram, const InterfaceEntry* inIE);
    virtual IPv4::Hook::Result datagramLocalInHook(IPv4Datagram* datagram, const InterfaceEntry* inIE);
    virtual IPv4::Hook::Result datagramForwardHook(IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry*& outIE, IPv4Address& nextHopAddr);
    virtual IPv4::Hook::Result datagramPostRoutingHook(IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry*& outIE, IPv4Address& nextHopAddr);
    virtual IPv4::Hook::Result datagramLocalOutHook(IPv4Datagram* datagram, const InterfaceEntry*& outIE);
};

#endif  // __INET_MANETIPV4HOOK_H
