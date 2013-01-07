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

#include "ControlManetRouting_m.h"
#include "InterfaceEntry.h"
#include "IPv4Datagram.h"
#include "IRoutingTable.h"


void ManetIPv4Hook::initHook(cModule* _module)
{
    module = _module;
    ipLayer = check_and_cast<IPv4*>(findModuleWhereverInNode("ip", module));
    cProperties *props = module->getProperties();
    isReactive = props && props->getAsBool("reactive");
    rt = RoutingTableAccess().get();

    ProtocolMapping mapping;
    mapping.parseProtocolMapping(ipLayer->par("protocolMapping"));
    gateIndex = mapping.getOutputGateForProtocol(IP_PROT_MANET);
    ipLayer->registerHook(0, this);
}

void ManetIPv4Hook::finishHook()
{
    ipLayer->unregisterHook(0, this);
}


IPv4::Hook::Result ManetIPv4Hook::datagramPreRoutingHook(IPv4Datagram* datagram, InterfaceEntry* inIE)
{
    if (isReactive)
    {
        if (!inIE->isLoopback() && !datagram->getDestAddress().isMulticast())
            sendRouteUpdateMessageToManet(datagram);

        if (checkPacketUnroutable(datagram, NULL))
        {
            delete datagram->removeControlInfo();
            sendNoRouteMessageToManet(datagram);
            return IPv4::Hook::STOLEN;
        }
    }

    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result ManetIPv4Hook::datagramLocalInHook(IPv4Datagram* datagram, InterfaceEntry* inIE)
{
    EV << "HOOK " << module->getFullPath() << ": LOCAL IN: packet=" << datagram->getName()
       << " inIE=" << (inIE ? inIE->getName() : "NULL")
       << endl;

    if (isReactive)
    {
        if (datagram->getTransportProtocol() == IP_PROT_DSR)
        {
            sendToManet(datagram);
            return IPv4::Hook::STOLEN;
        }
    }

    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result ManetIPv4Hook::datagramLocalOutHook(IPv4Datagram* datagram, InterfaceEntry*& outIE)
{
    if (isReactive)
    {
        sendRouteUpdateMessageToManet(datagram);

        if (checkPacketUnroutable(datagram, outIE))
        {
            delete datagram->removeControlInfo();
            sendNoRouteMessageToManet(datagram);
            return IPv4::Hook::STOLEN;
        }
    }
    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result ManetIPv4Hook::datagramForwardHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry*& outIE, IPv4Address& nextHopAddr)
{
    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result ManetIPv4Hook::datagramPostRoutingHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry*& outIE, IPv4Address& nextHopAddr)
{
    return IPv4::Hook::ACCEPT;
}

// Helper functions:

void ManetIPv4Hook::sendRouteUpdateMessageToManet(IPv4Datagram *datagram)
{
    if (datagram->getTransportProtocol() != IP_PROT_DSR) // Dsr don't use update code, the Dsr datagram is the update.
    {
        ControlManetRouting *control = new ControlManetRouting();
        control->setOptionCode(MANET_ROUTE_UPDATE);
        control->setSrcAddress(ManetAddress(datagram->getSrcAddress()));
        control->setDestAddress(ManetAddress(datagram->getDestAddress()));
        sendToManet(control);
    }
}

void ManetIPv4Hook::sendNoRouteMessageToManet(IPv4Datagram *datagram)
{
    if (datagram->getTransportProtocol()==IP_PROT_DSR)
    {
        sendToManet(datagram);
    }
    else
    {
        ControlManetRouting *control = new ControlManetRouting();
        control->setOptionCode(MANET_ROUTE_NOROUTE);
        control->setSrcAddress(ManetAddress(datagram->getSrcAddress()));
        control->setDestAddress(ManetAddress(datagram->getDestAddress()));
        control->encapsulate(datagram);
        sendToManet(control);
    }
}

void ManetIPv4Hook::sendToManet(cPacket *packet)
{
    ipLayer->send(packet, "transportOut", gateIndex);
}

bool ManetIPv4Hook::checkPacketUnroutable(IPv4Datagram* datagram, InterfaceEntry* outIE)
{
    if (outIE != NULL)
        return false;

    IPv4Address &destAddr = datagram->getDestAddress();

    if (destAddr.isMulticast() || destAddr.isLimitedBroadcastAddress())
        return false;
    if (rt->isLocalAddress(destAddr) || rt->isLocalBroadcastAddress(destAddr))
        return false;
    return (rt->findBestMatchingRoute(destAddr) == NULL);
}

