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

#include "ManetIPv4Hook.h"

#include "ControlManetRouting_m.h"
#include "IInterfaceTable.h"
#include "InterfaceEntry.h"
#include "IPv4ControlInfo.h"
#include "IPv4Datagram.h"
#include "IRoutingTable.h"

#include "dsr-pkt_omnet.h"


void ManetIPv4Hook::initHook(cModule* _module)
{
    module = _module;
    ipLayer = check_and_cast<IPv4*>(findModuleWhereverInNode("ip", module));
    cProperties *props = module->getProperties();
    isReactive = props && props->getAsBool("reactive");

    ProtocolMapping mapping;
    mapping.parseProtocolMapping(ipLayer->par("protocolMapping"));
    gateIndex = mapping.getOutputGateForProtocol(IP_PROT_MANET);
    ipLayer->registerHook(0, this);
}

void ManetIPv4Hook::finishHook()
{
    ipLayer->unregisterHook(0, this);
}


INetfilter::IHook::Result ManetIPv4Hook::datagramPreRoutingHook(IPv4Datagram* datagram, const InterfaceEntry* inIE)
{
    if (isReactive)
    {
        if (!inIE->isLoopback() && !datagram->getDestAddress().isMulticast())
            sendRouteUpdateMessageToManet(datagram);

        if (checkPacketUnroutable(datagram, NULL))
        {
            delete datagram->removeControlInfo();
            sendNoRouteMessageToManet(datagram);
            return INetfilter::IHook::STOLEN;
        }
    }

    return INetfilter::IHook::ACCEPT;
}

INetfilter::IHook::Result ManetIPv4Hook::datagramLocalInHook(IPv4Datagram* datagram, const InterfaceEntry* inIE)
{
    if (isReactive)
    {
        if (datagram->getTransportProtocol() == IP_PROT_DSR)
        {
            sendToManet(datagram);
            return INetfilter::IHook::STOLEN;
        }
    }

    return INetfilter::IHook::ACCEPT;
}

INetfilter::IHook::Result ManetIPv4Hook::datagramLocalOutHook(IPv4Datagram* datagram, const InterfaceEntry*& outIE)
{
    if (isReactive)
    {
        bool isDsr = false;
        IPv4Address nextHopAddr(IPv4Address::UNSPECIFIED_ADDRESS);

        // Dsr routing, Dsr is a HL protocol and send IPv4Datagram
        if (datagram->getTransportProtocol()==IP_PROT_DSR)
        {
            isDsr = true;
            IPv4ControlInfo *controlInfo = check_and_cast<IPv4ControlInfo*>(datagram->getControlInfo());
            DSRPkt *dsrpkt = check_and_cast<DSRPkt *>(datagram);
            outIE = ipLayer->getInterfaceTable()->getInterfaceById(controlInfo->getInterfaceId());
            nextHopAddr = dsrpkt->nextAddress();
        }

        sendRouteUpdateMessageToManet(datagram);

        if (checkPacketUnroutable(datagram, outIE))
        {
            delete datagram->removeControlInfo();
            sendNoRouteMessageToManet(datagram);
            return INetfilter::IHook::STOLEN;
        }
        if (isDsr && outIE != NULL && !nextHopAddr.isUnspecified())
        {
            IPv4Address destAddr = datagram->getDestAddress();
            if (!destAddr.isMulticast() && !ipLayer->getRoutingTable()->isLocalAddress(destAddr))
            {
                delete datagram->removeControlInfo();
                ipLayer->insertDatagramToHookQueue(datagram, NULL, outIE, nextHopAddr, IPv4::QueuedDatagramForHook::POSTROUTING);
                ipLayer->reinjectDatagram(datagram);
                return INetfilter::IHook::STOLEN;
            }
        }
    }
    return INetfilter::IHook::ACCEPT;
}

INetfilter::IHook::Result ManetIPv4Hook::datagramForwardHook(IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry*& outIE, IPv4Address& nextHopAddr)
{
    return INetfilter::IHook::ACCEPT;
}

INetfilter::IHook::Result ManetIPv4Hook::datagramPostRoutingHook(IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry*& outIE, IPv4Address& nextHopAddr)
{
    return INetfilter::IHook::ACCEPT;
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

bool ManetIPv4Hook::checkPacketUnroutable(IPv4Datagram* datagram, const InterfaceEntry* outIE)
{
    if (outIE != NULL)
        return false;

    IPv4Address &destAddr = datagram->getDestAddress();

    if (destAddr.isMulticast() || destAddr.isLimitedBroadcastAddress())
        return false;

    IIPv4RoutingTable* rt = ipLayer->getRoutingTable();
    if (rt->isLocalAddress(destAddr) || rt->isLocalBroadcastAddress(destAddr))
        return false;

    return (rt->findBestMatchingRoute(destAddr) == NULL);
}

