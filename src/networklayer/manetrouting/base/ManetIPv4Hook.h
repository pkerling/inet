/***************************************************************************
 *   Copyright (C) 2012 OpenSim Ltd                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef __INET_MANETIPV4HOOK_H
#define __INET_MANETIPV4HOOK_H

#include "IPv4.h"

#include "ARP.h"

#include <vector>
#include <set>

class INET_API ManetIPv4Hook : public IPv4::Hook
{
  protected:
    cModule* module;
    IPv4 *ipLayer;
    bool isReactive;
  public:
    ManetIPv4Hook() : module(NULL), ipLayer(NULL), isReactive(false) {}
  protected:
    void initHook(cModule* module);
    void finishHook();
  public:
    virtual IPv4::Hook::Result datagramPreRoutingHook(IPv4Datagram* datagram, InterfaceEntry* inIE);
    virtual IPv4::Hook::Result datagramLocalInHook(IPv4Datagram* datagram, InterfaceEntry* inIE);
    virtual IPv4::Hook::Result datagramForwardHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry* outIE, IPv4Address& nextHopAddr);
    virtual IPv4::Hook::Result datagramPostRoutingHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry* outIE, IPv4Address& nextHopAddr);
    virtual IPv4::Hook::Result datagramLocalOutHook(IPv4Datagram* datagram, InterfaceEntry* outIE);
};

#endif  // __INET_MANETIPV4HOOK_H