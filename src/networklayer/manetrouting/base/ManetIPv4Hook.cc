/***************************************************************************
 *   Copyright (C) 2008 by Alfonso Ariza                                   *
 *   aarizaq@uma.es                                                        *
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

///

#include "ManetIPv4Hook.h"

#include "InterfaceEntry.h"
#include "IPv4Datagram.h"


void ManetIPv4Hook::initHook(cModule* _module)
{
    module = _module;
    ipLayer = check_and_cast<IPv4*>(findModuleWhereverInNode("ip", module));
    ipLayer->registerHook(0, this);
    cProperties *props = module->getProperties();
    isReactive = props && props->getAsBool("reactive");
}

void ManetIPv4Hook::finishHook()
{
    ipLayer->unregisterHook(0, this);
}


IPv4::Hook::Result ManetIPv4Hook::datagramPreRoutingHook(IPv4Datagram* datagram, InterfaceEntry* inIE)
{
    EV << "HOOK: PREROUTING packet=" << datagram->getName()
       << " inIE=" << (inIE ? inIE->getName() : "NULL")
       << endl;

    if (isReactive && !inIE->isLoopback() && !datagram->getDestAddress().isMulticast())
        ipLayer->sendRouteUpdateMessageToManet(datagram);

    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result ManetIPv4Hook::datagramLocalInHook(IPv4Datagram* datagram, InterfaceEntry* inIE)
{
    EV << "HOOK " << module->getFullPath() << ": LOCAL IN: packet=" << datagram->getName()
       << " inIE=" << (inIE ? inIE->getName() : "NULL")
       << endl;

    if (isReactive && (datagram->getTransportProtocol() == IP_PROT_DSR))
    {
        ipLayer->sendToManet(datagram);
        return IPv4::Hook::STOLEN;
    }

    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result ManetIPv4Hook::datagramLocalOutHook(IPv4Datagram* datagram, InterfaceEntry* outIE)
{
    EV << "HOOK " << module->getFullPath() << ": LOCAL OUT: packet=" << datagram->getName()
       << " outIE=" << (outIE ? outIE->getName() : "NULL")
       << endl;

    if (isReactive)
        ipLayer->sendRouteUpdateMessageToManet(datagram);

    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result ManetIPv4Hook::datagramForwardHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry* outIE, IPv4Address& nextHopAddr)
{
    EV << "HOOK " << module->getFullPath() << ": FORWARD: packet=" << datagram->getName()
       << " inIE=" << (inIE ? inIE->getName() : "NULL")
       << " outIE=" << (outIE ? outIE->getName() : "NULL")
       << " nextHop=" << nextHopAddr
       << endl;
    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result ManetIPv4Hook::datagramPostRoutingHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry* outIE, IPv4Address& nextHopAddr)
{
    EV << "HOOK " << module->getFullPath() << ": POSTROUTING packet=" << datagram->getName()
       << " inIE=" << (inIE ? inIE->getName() : "NULL")
       << " outIE=" << (outIE ? outIE->getName() : "NULL")
       << " nextHop=" << nextHopAddr
       << endl;
    return IPv4::Hook::ACCEPT;
}

